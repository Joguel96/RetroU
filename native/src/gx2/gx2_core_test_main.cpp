#include "gx2/gx2_renderer.hpp"
#include "gx2/gx2_text.hpp"
#include "gx2/gx2_image.hpp"
#include "gx2/retrou_data.hpp"
#include "asset_names.hpp"
#include "retrou_assets.hpp"
#include "rpl_core.hpp"
#include "wpad_input.hpp"

#include <SDL.h>
#include <atomic>
#include <coreinit/foreground.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memheap.h>
#include <coreinit/systeminfo.h>
#include <coreinit/thread.h>
#include <libretro.h>
#include <png.h>
#include <padscore/wpad.h>
#include <proc_ui/procui.h>
#include <thread>
#include <vpad/input.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

// GX2 frontend per console: library grid with covers/focus, favorites, and a
// sidebar. A launches, B returns, X toggles favorite, Plus opens the sidebar,
// Minus+Plus exits.

extern const unsigned char _binary_ui_ttf_start[];
extern const unsigned char _binary_ui_ttf_end[];
extern const unsigned char _binary_icon_png_start[];
extern const unsigned char _binary_icon_png_end[];

// CTRL-3: imagen del mando de la consola (assets/controles/<system>.png,
// incrustada con objcopy). RETROU_PAD_PNG lo define CMake por consola.
#ifndef RETROU_PAD_PNG
#define RETROU_PAD_PNG nes
#endif
#define RETROU_PAD_START1(name) _binary_##name##_png_start
#define RETROU_PAD_START(name) RETROU_PAD_START1(name)
#define RETROU_PAD_END1(name) _binary_##name##_png_end
#define RETROU_PAD_END(name) RETROU_PAD_END1(name)
extern const unsigned char RETROU_PAD_START(RETROU_PAD_PNG)[];
extern const unsigned char RETROU_PAD_END(RETROU_PAD_PNG)[];

using namespace retrou;
using namespace retrou::gx2;

#ifndef RETROU_CORE_SYSTEM
#define RETROU_CORE_SYSTEM "nes"
#endif
#ifndef RETROU_CORE_LABEL
#define RETROU_CORE_LABEL "NES"
#endif

static void bootLog(const char *stage, bool truncate = false)
{
    mkdir("/vol/external01/retrou", 0777);
    if (truncate) {
        const std::string base = "/vol/external01/retrou/retrou_rpx.log";
        std::remove((base + ".3").c_str());
        std::rename((base + ".2").c_str(), (base + ".3").c_str());
        std::rename((base + ".1").c_str(), (base + ".2").c_str());
        std::rename(base.c_str(), (base + ".1").c_str());
    }
    std::ofstream out("/vol/external01/retrou/retrou_rpx.log",
                      truncate ? std::ios::trunc : std::ios::app);
    if (out) {
        out << "GX2 boot " << RETROU_CORE_SYSTEM << ": " << stage << "\n";
    }
}

static bool saveFrameThumbnail(const RplCore &core, const std::string &path)
{
    if (!core.hasFrame()) return false;
    constexpr int width = 160, height = 120;
    const int sourceW = static_cast<int>(core.frameWidth());
    const int sourceH = static_cast<int>(core.frameHeight());
    const auto *source = static_cast<const uint8_t *>(core.frameData());
    std::vector<uint8_t> pixels(width * height * 4);
    for (int y = 0; y < height; ++y) {
        const uint8_t *row = source + static_cast<size_t>(y * sourceH / height) * core.framePitch();
        for (int x = 0; x < width; ++x) {
            const int sx = x * sourceW / width;
            uint8_t *dst = &pixels[(y * width + x) * 4];
            if (core.pixelFormat() == RETRO_PIXEL_FORMAT_RGB565) {
                const uint16_t value = reinterpret_cast<const uint16_t *>(row)[sx];
                dst[0] = static_cast<uint8_t>(((value >> 11) & 31) * 255 / 31);
                dst[1] = static_cast<uint8_t>(((value >> 5) & 63) * 255 / 63);
                dst[2] = static_cast<uint8_t>((value & 31) * 255 / 31);
            } else {
                dst[0] = row[sx * 4]; dst[1] = row[sx * 4 + 1]; dst[2] = row[sx * 4 + 2];
            }
            dst[3] = 255;
        }
    }
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = width;
    image.height = height;
    image.format = PNG_FORMAT_RGBA;
    return png_image_write_to_file(&image, path.c_str(), 0, pixels.data(), 0, nullptr) != 0;
}

static Texture makeCover(uint8_t r0, uint8_t g0, uint8_t b0,
                         uint8_t r1, uint8_t g1, uint8_t b1)
{
    constexpr int w = 96, h = 128;
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        const float t = static_cast<float>(y) / (h - 1);
        for (int x = 0; x < w; ++x) {
            uint8_t *p = &px[(static_cast<size_t>(y) * w + x) * 4];
            p[0] = static_cast<uint8_t>(r0 * (1.0f - t) + r1 * t);
            p[1] = static_cast<uint8_t>(g0 * (1.0f - t) + g1 * t);
            p[2] = static_cast<uint8_t>(b0 * (1.0f - t) + b1 * t);
            p[3] = 255;
        }
    }
    return createTexture(w, h, px.data());
}

// Region id: 0 unknown, 1 USA, 2 Europe, 3 Japan.
static int detectRegion(const std::string &title)
{
    std::string t;
    t.reserve(title.size());
    for (char c : title) {
        t += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (t.find("(usa)") != std::string::npos || t.find("[usa]") != std::string::npos ||
        t.find("(u)") != std::string::npos) {
        return 1;
    }
    if (t.find("(europe)") != std::string::npos || t.find("(spain)") != std::string::npos ||
        t.find("(france)") != std::string::npos || t.find("(germany)") != std::string::npos ||
        t.find("(italy)") != std::string::npos || t.find("(australia)") != std::string::npos ||
        t.find("(world)") != std::string::npos || t.find("(scandinavia)") != std::string::npos ||
        t.find("(europe") != std::string::npos) {
        return 2;
    }
    if (t.find("(japan)") != std::string::npos || t.find("[japan]") != std::string::npos ||
        t.find("(j)") != std::string::npos) {
        return 3;
    }
    return 0;
}

static std::string toLower(const std::string &s)
{
    std::string r;
    r.reserve(s.size());
    for (char c : s) r += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

static const char *kbChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"; // 36

static void drawStar(float cx, float cy, float outer, float inner, Color color)
{
    const float pi = 3.14159265f;
    float px[10], py[10];
    for (int i = 0; i < 10; ++i) {
        const float r = (i % 2 == 0) ? outer : inner;
        const float a = -pi / 2.0f + i * (pi / 5.0f);
        px[i] = cx + std::cos(a) * r;
        py[i] = cy + std::sin(a) * r;
    }
    for (int i = 0; i < 10; ++i) {
        const int j = (i + 1) % 10;
        drawTriangle(cx, cy, px[i], py[i], px[j], py[j], color);
    }
}

static std::string titleYear(const std::string &title)
{
    for (size_t i = 0; i + 4 < title.size(); ++i) {
        if (title[i] == '(' && (title[i + 1] == '1' || title[i + 1] == '2') &&
            std::isdigit(static_cast<unsigned char>(title[i + 2])) &&
            std::isdigit(static_cast<unsigned char>(title[i + 3])) &&
            std::isdigit(static_cast<unsigned char>(title[i + 4]))) {
            return title.substr(i + 1, 4);
        }
    }
    return {};
}

static void loadMetaMap(const std::string &sdRoot, const std::string &system,
                        const char *kind, std::map<std::string, std::string> &out)
{
    std::ifstream in(sdRoot + "/retrou/metadata/" + system + "-" + kind + ".dat");
    if (!in) return;
    const std::string keyPrefix = std::string(kind) + " \"";
    std::string line;
    std::string currentTitle;
    while (std::getline(in, line)) {
        const size_t c = line.find("comment \"");
        if (c != std::string::npos) {
            const size_t s = c + 9;
            const size_t e = line.find('"', s);
            if (e != std::string::npos) {
                currentTitle = line.substr(s, e - s);
            }
            continue;
        }
        const size_t k = line.find(keyPrefix);
        if (k != std::string::npos && !currentTitle.empty()) {
            const size_t s = k + keyPrefix.size();
            const size_t e = line.find('"', s);
            if (e != std::string::npos) {
                out[currentTitle] = line.substr(s, e - s);
            }
        }
    }
}

struct Theme {
    const char *name;
    Color bgTop;
    Color bgBottom;
    Color accent;
};

static const Theme kThemes[5] = {
    {"Gris",   {28, 34, 48, 255},  {8, 11, 18, 255},   {210, 214, 222, 255}},
    {"Azul",   {16, 30, 52, 255},  {8, 10, 20, 255},   {92, 160, 255, 255}},
    {"Verde",  {14, 36, 30, 255},  {6, 12, 12, 255},   {96, 224, 142, 255}},
    {"Dorado", {38, 30, 16, 255},  {13, 9, 5, 255},    {245, 217, 127, 255}},
    {"Rosa",   {42, 20, 36, 255},  {15, 7, 13, 255},   {255, 122, 182, 255}},
};

static int loadThemeIndex(const std::string &sdRoot)
{
    std::ifstream in(sdRoot + "/retrou/config/theme.conf");
    std::string name;
    if (in >> name) {
        for (int i = 0; i < 5; ++i) {
            if (name == kThemes[i].name) return i;
        }
    }
    return 0;
}

static void saveThemeIndex(const std::string &sdRoot, int idx)
{
    std::ofstream out(sdRoot + "/retrou/config/theme.conf", std::ios::trunc);
    if (out) {
        out << kThemes[idx].name << "\n";
    }
}

static void loadVideoOptions(const std::string &sdRoot, int &aspect, int &scale, int &filter)
{
    std::ifstream in(sdRoot + "/retrou/config/" RETROU_CORE_SYSTEM "/video.conf");
    if (in >> aspect >> scale >> filter) {
        return;
    }
    aspect = 0;
    scale = 0;
    filter = 1;
}

static void saveVideoOptions(const std::string &sdRoot, int aspect, int scale, int filter)
{
    std::ofstream out(sdRoot + "/retrou/config/" RETROU_CORE_SYSTEM "/video.conf", std::ios::trunc);
    if (out) {
        out << aspect << " " << scale << " " << filter << "\n";
    }
}

// CTRL-3 (modelo Our64): el editor muestra las ACCIONES del core y cada una
// puede tener varias fuentes físicas (máscara del dispositivo). Acción ->
// fuentes en vez de botón físico -> acción. Acción: 0=B, 1=Y, 2=Select,
// 3=Start, 4..7=D-Pad, 8=A, 9=X, 10=L, 11=R.
struct PhysBtn {
    const char *label;
    uint32_t mask;
};

static const PhysBtn kVpadButtons[] = {
    {"A", VPAD_BUTTON_A},
    {"B", VPAD_BUTTON_B},
    {"X", VPAD_BUTTON_X},
    {"Y", VPAD_BUTTON_Y},
    {"L", VPAD_BUTTON_L},
    {"R", VPAD_BUTTON_R},
    {"ZL", VPAD_BUTTON_ZL},
    {"ZR", VPAD_BUTTON_ZR},
    {"+", VPAD_BUTTON_PLUS},
    {"-", VPAD_BUTTON_MINUS},
    {"D-Pad Arriba", VPAD_BUTTON_UP},
    {"D-Pad Abajo", VPAD_BUTTON_DOWN},
    {"D-Pad Izquierda", VPAD_BUTTON_LEFT},
    {"D-Pad Derecha", VPAD_BUTTON_RIGHT},
    {"Palanca L (boton)", VPAD_BUTTON_STICK_L},
};

static const PhysBtn kProButtons[] = {
    {"A", WPAD_PRO_BUTTON_A},
    {"B", WPAD_PRO_BUTTON_B},
    {"X", WPAD_PRO_BUTTON_X},
    {"Y", WPAD_PRO_BUTTON_Y},
    {"L", WPAD_PRO_BUTTON_L},
    {"R", WPAD_PRO_BUTTON_R},
    {"ZL", WPAD_PRO_BUTTON_ZL},
    {"ZR", WPAD_PRO_BUTTON_ZR},
    {"+", WPAD_PRO_BUTTON_PLUS},
    {"-", WPAD_PRO_BUTTON_MINUS},
    {"D-Pad Arriba", WPAD_PRO_BUTTON_UP},
    {"D-Pad Abajo", WPAD_PRO_BUTTON_DOWN},
    {"D-Pad Izquierda", WPAD_PRO_BUTTON_LEFT},
    {"D-Pad Derecha", WPAD_PRO_BUTTON_RIGHT},
    {"Palanca L (boton)", WPAD_PRO_BUTTON_STICK_L},
    {"Palanca R (boton)", WPAD_PRO_BUTTON_STICK_R},
};

constexpr int kVpadBtnCount = static_cast<int>(sizeof(kVpadButtons) / sizeof(kVpadButtons[0]));
constexpr int kProBtnCount = static_cast<int>(sizeof(kProButtons) / sizeof(kProButtons[0]));

// Fuentes físicas por acción (máscara del dispositivo; 0 = sin mapear).
// Es el mismo tipo que aplica el core (retrou::CoreActionMap).
using ActionSources = retrou::CoreActionMap;

// Etiqueta legible de una fuente (para el editor y el conf v2 no se usa,
// el conf guarda máscaras hexadecimales).
static std::string maskLabel(uint32_t mask, int device)
{
    const PhysBtn *btns = device == 0 ? kVpadButtons : kProButtons;
    const int n = device == 0 ? kVpadBtnCount : kProBtnCount;
    switch (mask) {
        case kStickSrcUp:     return "Stick L Arriba";
        case kStickSrcDown:   return "Stick L Abajo";
        case kStickSrcLeft:   return "Stick L Izquierda";
        case kStickSrcRight:  return "Stick L Derecha";
        case kStickRSrcUp:    return "Stick R Arriba";
        case kStickRSrcDown:  return "Stick R Abajo";
        case kStickRSrcLeft:  return "Stick R Izquierda";
        case kStickRSrcRight: return "Stick R Derecha";
        default: break;
    }
    for (int i = 0; i < n; ++i) {
        if (btns[i].mask == mask) return btns[i].label;
    }
    return "?";
}

// Defaults (idénticos a los del modelo invertido anterior, más el stick L
// como D-Pad por dirección, que antes era un toggle aparte).
static ActionSources defaultActionMap(const PhysBtn *btns, int n)
{
    ActionSources m;
    auto bind = [&m](uint32_t mask, int action) { m.sources[action] |= mask; };
    bind(btns[0].mask, 8);  // A -> A
    bind(btns[1].mask, 0);  // B -> B
    if (n > 3) {
        bind(btns[2].mask, 9); // X -> X
        bind(btns[3].mask, 1); // Y -> Y
    }
    if (n > 5) {
        bind(btns[4].mask, 10); // L -> L
        bind(btns[5].mask, 11); // R -> R
    }
    bind(btns[8].mask, 3);  // + -> Start
    bind(btns[9].mask, 2);  // - -> Select
    bind(btns[10].mask, 4); // D-Pad -> D-Pad
    bind(btns[11].mask, 5);
    bind(btns[12].mask, 6);
    bind(btns[13].mask, 7);
    bind(kStickSrcUp, 4);
    bind(kStickSrcDown, 5);
    bind(kStickSrcLeft, 6);
    bind(kStickSrcRight, 7);
    return m;
}

// Migración del formato v1 (botón físico -> acción, una acción por botón)
// al modelo por acciones: vuelca cada máscara en la acción que le tocaba.
static void assignToActionMap(const PhysBtn *btns, int n,
                              const std::array<int8_t, 16> &assign,
                              bool stickDpad, ActionSources &out)
{
    out = defaultActionMap(btns, n);
    for (int a = 0; a < 12; ++a) out.sources[a] = 0;
    for (int i = 0; i < n; ++i) {
        const int id = assign[i];
        if (id >= 0 && id < 12) out.sources[id] |= btns[i].mask;
    }
    if (stickDpad) {
        out.sources[4] |= kStickSrcUp;
        out.sources[5] |= kStickSrcDown;
        out.sources[6] |= kStickSrcLeft;
        out.sources[7] |= kStickSrcRight;
    }
}

static void setAssignByMask(std::array<int8_t, 16> &assign, const PhysBtn *btns, int n, uint32_t mask, int id)
{
    for (int i = 0; i < n; ++i) {
        if (btns[i].mask == mask) {
            assign[i] = static_cast<int8_t>(id);
            return;
        }
    }
}

static const char *retroName(int id);

// Nombre legible del tipo de mando conectado (para la lista de Mando 1/2).
static const char *wpadTypeLabel(int type)
{
    switch (type) {
        case WPAD_TYPE_PRO:     return "Wii U Pro Controller";
        case WPAD_TYPE_CLASSIC: return "Classic Controller";
        case WPAD_TYPE_NUNCHUK: return "Wii Remote + Nunchuk";
        case WPAD_TYPE_WIIMOTE: return "Wii Remote";
        default:                return "Mando";
    }
}

// Direcciones de los sticks L/R como fuentes pseudo (para la captura del
// editor). Misma convención que rpl_core (+y = arriba).
static uint32_t stickSourceBits(float lx, float ly, float rx, float ry)
{
    const float dz = 0.5f;
    uint32_t bits = 0;
    if (ly > dz) bits |= kStickSrcUp;
    else if (ly < -dz) bits |= kStickSrcDown;
    else if (lx < -dz) bits |= kStickSrcLeft;
    else if (lx > dz) bits |= kStickSrcRight;
    if (ry > dz) bits |= kStickRSrcUp;
    else if (ry < -dz) bits |= kStickRSrcDown;
    else if (rx < -dz) bits |= kStickRSrcLeft;
    else if (rx > dz) bits |= kStickRSrcRight;
    return bits;
}

static bool hasSecondCorePort()
{
    return std::string(RETROU_CORE_SYSTEM) != "gba";
}

// CTRL-3: resaltado sobre la imagen del mando ( coordenadas en píxeles de
// las imágenes de assets/controles/<consola>.png, 1536x1024). r == 0 =
// la consola no tiene ese botón.
struct PadSpot {
    float x, y, r;
};

static const PadSpot *padSpots()
{
    if (std::string(RETROU_CORE_SYSTEM) == "nes") {
        static const PadSpot spots[12] = {
            {1080.0f, 768.0f, 70.0f}, // B
            {0, 0, 0},                // Y (no existe en NES)
            {640.0f, 770.0f, 55.0f},  // Select
            {820.0f, 770.0f, 55.0f},  // Start
            {320.0f, 575.0f, 55.0f},  // D-Pad Arriba
            {320.0f, 815.0f, 55.0f},  // Abajo
            {190.0f, 695.0f, 55.0f},  // Izquierda
            {450.0f, 695.0f, 55.0f},  // Derecha
            {1272.0f, 756.0f, 70.0f}, // A
            {0, 0, 0},                // X (no existe)
            {0, 0, 0}, {0, 0, 0},     // L/R (no existen)
        };
        return spots;
    }
    if (std::string(RETROU_CORE_SYSTEM) == "snes") {
        static const PadSpot spots[12] = {
            {1206.0f, 744.0f, 60.0f}, // B (amarillo)
            {1079.0f, 638.0f, 60.0f}, // Y (verde)
            {680.0f, 680.0f, 48.0f},  // Select
            {855.0f, 680.0f, 48.0f},  // Start
            {320.0f, 545.0f, 55.0f},  // D-Pad Arriba
            {320.0f, 740.0f, 55.0f},  // Abajo
            {205.0f, 640.0f, 55.0f},  // Izquierda
            {435.0f, 640.0f, 55.0f},  // Derecha
            {1323.0f, 642.0f, 60.0f}, // A (rojo)
            {1204.0f, 533.0f, 60.0f}, // X (azul)
            {310.0f, 340.0f, 80.0f},  // L
            {1230.0f, 340.0f, 80.0f}, // R
        };
        return spots;
    }
    // gba
    static const PadSpot spots[12] = {
        {1235.0f, 525.0f, 48.0f}, // B
        {0, 0, 0},                // Y (no existe en GBA)
        {337.0f, 751.0f, 26.0f},  // Select
        {337.0f, 677.0f, 26.0f},  // Start
        {261.0f, 430.0f, 45.0f},  // D-Pad Arriba
        {261.0f, 557.0f, 45.0f},  // Abajo
        {196.0f, 493.0f, 45.0f},  // Izquierda
        {326.0f, 493.0f, 45.0f},  // Derecha
        {1345.0f, 470.0f, 48.0f}, // A
        {0, 0, 0},                // X (no existe)
        {240.0f, 255.0f, 65.0f},  // L
        {1300.0f, 255.0f, 65.0f}, // R
    };
    return spots;
}

// New keyword format (v2, CTRL-3 modelo Our64):
//   v2
//   p1 <device> / p2 <device>
//   [gamepad] / [pro]  followed by:  <accion> = <mascara> <mascara> ...
//   (acción con el nombre de retroName; máscaras hexadecimales; las
//   pseudo-máscaras 0x01000000..0x08000000 son las direcciones del stick L).
// Formats v1 and pre-v1 (one line of 13 ints) are migrated on load.
static std::string controlConfigDir(const std::string &sdRoot)
{
    return sdRoot + "/retrou/config/" RETROU_CORE_SYSTEM;
}

static std::string romControlPath(const std::string &sdRoot, const std::string &romPath)
{
    uint64_t hash = 14695981039346656037ULL;
    for (unsigned char c : romPath) {
        hash ^= c;
        hash *= 1099511628211ULL;
    }
    char name[24];
    std::snprintf(name, sizeof(name), "%016llx.conf", static_cast<unsigned long long>(hash));
    return controlConfigDir(sdRoot) + "/rom-maps/" + name;
}

static std::string controlProfilePath(const std::string &sdRoot, int device, int slot)
{
    return controlConfigDir(sdRoot) + "/profiles/" +
        (device == 0 ? "gamepad-" : "pro-") + std::to_string(slot + 1) + ".conf";
}

static bool loadActionProfile(const std::string &path, ActionSources &map)
{
    std::ifstream in(path);
    std::string magic;
    if (!(in >> magic) || magic != "profile-v1") return false;
    ActionSources candidate{};
    for (int i = 0; i < 12; ++i) {
        unsigned value = 0;
        if (!(in >> std::hex >> value)) return false;
        candidate.sources[i] = value;
    }
    map = candidate;
    return true;
}

static bool saveActionProfile(const std::string &path, const ActionSources &map)
{
    std::ofstream out(path, std::ios::trunc);
    if (!out) return false;
    out << "profile-v1\n" << std::hex;
    for (uint32_t value : map.sources) out << value << "\n";
    out.flush();
    return static_cast<bool>(out);
}

static bool loadControlOptions(const std::string &sdRoot, int &player1, int &player2,
                               ActionSources &vpadMap, ActionSources &proMap,
                               const std::string &path = {})
{
    vpadMap = defaultActionMap(kVpadButtons, kVpadBtnCount);
    proMap = defaultActionMap(kProButtons, kProBtnCount);
    player1 = 0;
    player2 = 0;
    std::ifstream in(path.empty() ? controlConfigDir(sdRoot) + "/control-map.conf" : path);
    std::string tok;
    if (!(in >> tok)) return false;
    if (tok == "v2") {
        ActionSources *cur = nullptr;
        std::string line;
        while (std::getline(in, line)) {
            std::istringstream ls(line);
            if (!(ls >> tok)) continue;
            if (tok == "[gamepad]") {
                cur = &vpadMap;
            } else if (tok == "[pro]") {
                cur = &proMap;
            } else if (tok == "p1") {
                ls >> player1;
            } else if (tok == "p2") {
                ls >> player2;
            } else if (cur != nullptr) {
                // <accion> = <mascaras...>
                int action = -1;
                for (int a = 0; a < 12; ++a) {
                    if (tok == retroName(a)) {
                        action = a;
                        break;
                    }
                }
                if (action < 0) continue;
                std::string eq;
                if (!(ls >> eq)) continue;
                uint32_t sources = 0;
                std::string m;
                while (ls >> m) {
                    if (m == "none") continue;
                    char *end = nullptr;
                    errno = 0;
                    const unsigned long value = std::strtoul(m.c_str(), &end, 0);
                    if (m[0] == '-' || errno == ERANGE || end == m.c_str() || *end != '\0' ||
                        value > 0xffffffffUL) {
                        vpadMap = defaultActionMap(kVpadButtons, kVpadBtnCount);
                        proMap = defaultActionMap(kProButtons, kProBtnCount);
                        player1 = 0;
                        player2 = 0;
                        bootLog("control_map_parse_failed");
                        return false;
                    }
                    sources |= static_cast<uint32_t>(value);
                }
                cur->sources[action] = sources;
            }
        }
        return true;
    }
    // v1: botón físico -> acción (una acción por botón) + toggle del stick.
    if (tok == "v1") {
        std::array<int8_t, 16> vpadAssign{};
        std::array<int8_t, 16> proAssign{};
        vpadAssign.fill(-1);
        proAssign.fill(-1);
        bool vpadStick = false;
        bool proStick = false;
        while (in >> tok) {
            if (tok == "p1") {
                in >> player1;
            } else if (tok == "p2") {
                in >> player2;
            } else if (tok == "vpad") {
                unsigned m = 0;
                int id = -1;
                if (in >> m >> id) setAssignByMask(vpadAssign, kVpadButtons, kVpadBtnCount, m, id);
            } else if (tok == "vpadstick") {
                int v = 0;
                if (in >> v) vpadStick = (v != 0);
            } else if (tok == "pro") {
                unsigned m = 0;
                int id = -1;
                if (in >> m >> id) setAssignByMask(proAssign, kProButtons, kProBtnCount, m, id);
            } else if (tok == "prostick") {
                int v = 0;
                if (in >> v) proStick = (v != 0);
            }
        }
        assignToActionMap(kVpadButtons, kVpadBtnCount, vpadAssign, vpadStick, vpadMap);
        assignToActionMap(kProButtons, kProBtnCount, proAssign, proStick, proMap);
        return true;
    }
    // Old format migration: p1 followed by 12 masks indexed by core action id.
    player1 = std::atoi(tok.c_str());
    std::array<int8_t, 16> vpadAssign{};
    vpadAssign.fill(-1);
    for (int id = 0; id < 12; ++id) {
        unsigned m = 0;
        if (!(in >> m)) break;
        if (m != 0) setAssignByMask(vpadAssign, kVpadButtons, kVpadBtnCount, m, id);
    }
    assignToActionMap(kVpadButtons, kVpadBtnCount, vpadAssign, false, vpadMap);
    return true;
}

static bool saveControlOptions(const std::string &sdRoot, int player1, int player2,
                               const ActionSources &vpadMap, const ActionSources &proMap,
                               const std::string &path = {})
{
    std::ofstream out(path.empty() ? controlConfigDir(sdRoot) + "/control-map.conf" : path,
                      std::ios::trunc);
    if (!out) return false;
    out << "v2\n";
    out << "p1 " << player1 << "\n";
    out << "p2 " << player2 << "\n";
    auto writeSection = [&out](const char *name, const ActionSources &m) {
        out << "[" << name << "]\n";
        for (int a = 0; a < 12; ++a) {
            out << retroName(a) << " =";
            if (m.sources[a] == 0) {
                out << " none\n";
                continue;
            }
            for (uint32_t bit = 1; bit; bit <<= 1) {
                if (m.sources[a] & bit) {
                    char hex[16];
                    std::snprintf(hex, sizeof(hex), " 0x%x", bit);
                    out << hex;
                }
            }
            out << "\n";
        }
    };
    writeSection("gamepad", vpadMap);
    writeSection("pro", proMap);
    out.flush();
    return static_cast<bool>(out);
}

static std::vector<int> consoleActions()
{
    const std::string s = RETROU_CORE_SYSTEM;
    if (s == "nes") return {0, 8, 2, 3, 4, 5, 6, 7};
    if (s == "snes") return {0, 1, 8, 9, 10, 11, 2, 3, 4, 5, 6, 7};
    return {0, 8, 10, 11, 2, 3, 4, 5, 6, 7};
}

// NAV-6: el stick L navega toda la UI como si fuera el D-Pad (igual que
// Our64). Devuelve los bits "pulsados" al empujar hacia una nueva dirección;
// hay que volver al centro y empujar de nuevo para otro paso (igual que el
// trigger del D-Pad físico).
static uint32_t stickNavBits(const VPADStatus &vpad, uint32_t &lastDir)
{
    const float dz = 0.5f;
    uint32_t dir = 0;
    if (vpad.leftStick.y > dz) dir = VPAD_BUTTON_UP;
    else if (vpad.leftStick.y < -dz) dir = VPAD_BUTTON_DOWN;
    else if (vpad.leftStick.x < -dz) dir = VPAD_BUTTON_LEFT;
    else if (vpad.leftStick.x > dz) dir = VPAD_BUTTON_RIGHT;
    const uint32_t bits = (dir != 0 && dir != lastDir) ? dir : 0;
    lastDir = dir;
    return bits;
}

// CTRL-4: los mandos externos (Pro, Classic, Wii Remote...) también navegan
// la UI. Traduce sus botones a las constantes VPAD que usa la UI y trata el
// stick como D-Pad. Edge-triggered: solo dispara el cambio frente al frame
// anterior (un empuje = un paso, igual que el D-Pad físico).
static uint32_t wpadNavBits(const WpadSnapshot &s, uint32_t &lastBits)
{
    if (!s.connected) {
        lastBits = 0;
        return 0;
    }
    uint32_t bits = 0;
    if (s.type == WPAD_TYPE_PRO || s.type == WPAD_TYPE_CLASSIC) {
        if (s.buttons & WPAD_PRO_BUTTON_A) bits |= VPAD_BUTTON_A;
        if (s.buttons & WPAD_PRO_BUTTON_B) bits |= VPAD_BUTTON_B;
        if (s.buttons & WPAD_PRO_BUTTON_X) bits |= VPAD_BUTTON_X;
        if (s.buttons & WPAD_PRO_BUTTON_Y) bits |= VPAD_BUTTON_Y;
        if (s.buttons & (WPAD_PRO_BUTTON_L | WPAD_PRO_BUTTON_ZL)) bits |= VPAD_BUTTON_L;
        if (s.buttons & (WPAD_PRO_BUTTON_R | WPAD_PRO_BUTTON_ZR)) bits |= VPAD_BUTTON_R;
        if (s.buttons & WPAD_PRO_BUTTON_PLUS) bits |= VPAD_BUTTON_PLUS;
        if (s.buttons & WPAD_PRO_BUTTON_MINUS) bits |= VPAD_BUTTON_MINUS;
        if (s.buttons & WPAD_PRO_BUTTON_UP) bits |= VPAD_BUTTON_UP;
        if (s.buttons & WPAD_PRO_BUTTON_DOWN) bits |= VPAD_BUTTON_DOWN;
        if (s.buttons & WPAD_PRO_BUTTON_LEFT) bits |= VPAD_BUTTON_LEFT;
        if (s.buttons & WPAD_PRO_BUTTON_RIGHT) bits |= VPAD_BUTTON_RIGHT;
    } else if (s.type == WPAD_TYPE_WIIMOTE || s.type == WPAD_TYPE_NUNCHUK) {
        if (s.buttons & WPAD_BUTTON_A) bits |= VPAD_BUTTON_A;
        if (s.buttons & WPAD_BUTTON_B) bits |= VPAD_BUTTON_B;
        if (s.buttons & WPAD_BUTTON_PLUS) bits |= VPAD_BUTTON_PLUS;
        if (s.buttons & WPAD_BUTTON_MINUS) bits |= VPAD_BUTTON_MINUS;
        if (s.buttons & WPAD_BUTTON_UP) bits |= VPAD_BUTTON_UP;
        if (s.buttons & WPAD_BUTTON_DOWN) bits |= VPAD_BUTTON_DOWN;
        if (s.buttons & WPAD_BUTTON_LEFT) bits |= VPAD_BUTTON_LEFT;
        if (s.buttons & WPAD_BUTTON_RIGHT) bits |= VPAD_BUTTON_RIGHT;
    }
    // Stick como D-Pad (el centro o un cambio de dirección reinician el paso).
    const int32_t dz = 16383; // ~0.5
    uint32_t dir = 0;
    if (s.left_y > dz) dir = VPAD_BUTTON_UP;
    else if (s.left_y < -dz) dir = VPAD_BUTTON_DOWN;
    else if (s.left_x < -dz) dir = VPAD_BUTTON_LEFT;
    else if (s.left_x > dz) dir = VPAD_BUTTON_RIGHT;
    if (dir != 0) bits |= dir;
    const uint32_t trig = bits & ~lastBits;
    lastBits = bits;
    return trig;
}

static const char *retroName(int id)
{
    switch (id) {
        case 0: return "B";
        case 1: return "Y";
        case 2: return "Select";
        case 3: return "Start";
        case 4: return "Arriba";
        case 5: return "Abajo";
        case 6: return "Izquierda";
        case 7: return "Derecha";
        case 8: return "A";
        case 9: return "X";
        case 10: return "L";
        case 11: return "R";
    }
    return "?";
}

static void loadAppSettings(const std::string &sdRoot, int &columns, bool &showCovers,
                            int &uiVolume, bool &bgMusicOn, bool &sfxOn, int &coreVolume)
{
    std::ifstream in(sdRoot + "/retrou/config/" RETROU_CORE_SYSTEM "/" RETROU_CORE_SYSTEM ".conf");
    int c = 0, v = 0, bg = 0, sfx = 0, cv = 80;
    if (in >> columns >> c >> v >> bg >> sfx) {
        showCovers = (c != 0);
        uiVolume = v;
        bgMusicOn = (bg != 0);
        sfxOn = (sfx != 0);
        if (!(in >> cv)) cv = uiVolume;
        coreVolume = cv;
        return;
    }
    columns = 5;
    showCovers = true;
    uiVolume = 80;
    bgMusicOn = true;
    sfxOn = true;
    coreVolume = 80;
}

static void saveAppSettings(const std::string &sdRoot, int columns, bool showCovers,
                            int uiVolume, bool bgMusicOn, bool sfxOn, int coreVolume)
{
    std::ofstream out(sdRoot + "/retrou/config/" RETROU_CORE_SYSTEM "/" RETROU_CORE_SYSTEM ".conf", std::ios::trunc);
    if (out) {
        out << columns << " " << (showCovers ? 1 : 0) << " " << uiVolume << " "
            << (bgMusicOn ? 1 : 0) << " " << (sfxOn ? 1 : 0) << " " << coreVolume << "\n";
    }
}

// CTRL-7: acceso al menú durante el core. Es global para las tres apps y se
// configura por familia física, nunca por ROM ni por puerto del core.
struct MenuShortcuts {
    int choice[3] = {0, 0, 0}; // GamePad, Pro/Classic, Wii Remote/Nunchuk
};

static const char *kMenuShortcutLabels[3][3] = {
    {"Minus + Plus", "L + R + Plus", "ZL + ZR + Plus"},
    {"Minus + Plus", "L + R + Plus", "ZL + ZR + Plus"},
    {"Minus + Plus", "1 + 2", "A + B + Minus"},
};

static uint32_t menuShortcutMask(int family, int choice)
{
    if (family == 0) {
        if (choice == 1) return VPAD_BUTTON_L | VPAD_BUTTON_R | VPAD_BUTTON_PLUS;
        if (choice == 2) return VPAD_BUTTON_ZL | VPAD_BUTTON_ZR | VPAD_BUTTON_PLUS;
        return VPAD_BUTTON_MINUS | VPAD_BUTTON_PLUS;
    }
    if (family == 1) {
        if (choice == 1) return WPAD_PRO_BUTTON_L | WPAD_PRO_BUTTON_R | WPAD_PRO_BUTTON_PLUS;
        if (choice == 2) return WPAD_PRO_BUTTON_ZL | WPAD_PRO_BUTTON_ZR | WPAD_PRO_BUTTON_PLUS;
        return WPAD_PRO_BUTTON_MINUS | WPAD_PRO_BUTTON_PLUS;
    }
    if (choice == 1) return WPAD_BUTTON_1 | WPAD_BUTTON_2;
    if (choice == 2) return WPAD_BUTTON_A | WPAD_BUTTON_B | WPAD_BUTTON_MINUS;
    return WPAD_BUTTON_MINUS | WPAD_BUTTON_PLUS;
}

static bool menuShortcutHeld(uint32_t buttons, int family, int choice)
{
    const uint32_t selected = menuShortcutMask(family, choice);
    const uint32_t fallback = menuShortcutMask(family, 0);
    return (buttons & selected) == selected || (buttons & fallback) == fallback;
}

static MenuShortcuts loadMenuShortcuts(const std::string &sdRoot)
{
    MenuShortcuts shortcuts;
    std::ifstream in(sdRoot + "/retrou/config/menu-shortcut.conf");
    if (!in) return shortcuts;
    std::string version;
    int values[3] = {0, 0, 0};
    if (!(in >> version >> values[0] >> values[1] >> values[2]) || version != "v1" ||
        values[0] < 0 || values[0] > 2 || values[1] < 0 || values[1] > 2 ||
        values[2] < 0 || values[2] > 2) {
        bootLog("menu_shortcut_invalid_using_defaults");
        return shortcuts;
    }
    for (int i = 0; i < 3; ++i) shortcuts.choice[i] = values[i];
    bootLog("menu_shortcut_loaded");
    return shortcuts;
}

static bool saveMenuShortcuts(const std::string &sdRoot, const MenuShortcuts &shortcuts)
{
    std::ofstream out(sdRoot + "/retrou/config/menu-shortcut.conf", std::ios::trunc);
    if (out) out << "v1 " << shortcuts.choice[0] << " " << shortcuts.choice[1]
                 << " " << shortcuts.choice[2] << "\n";
    out.close();
    return out.good();
}

// ---- Asset download (covers/backdrops/metadata) ----
static std::atomic<bool> g_downloading{false};
static std::atomic<bool> g_downloadDone{false};
static std::atomic<unsigned> g_coverGeneration{0};
static std::atomic<int> g_downloadProgress{0};
static std::atomic<int> g_downloadTotal{0};
static std::atomic<int> g_downloadFailures{0};
static std::atomic<int> g_downloadQueued{0};
struct AssetJob {
    std::string sdRoot;
    std::string system;
    std::vector<std::string> titles;
    bool covers;
    bool backdrops;
    bool force;
};
static std::mutex g_downloadMutex;
static std::deque<AssetJob> g_assetQueue;

static bool loadAutoDownload(const std::string &sdRoot)
{
    std::ifstream in(sdRoot + "/retrou/config/" RETROU_CORE_SYSTEM "/auto.conf");
    int v = 1;
    if (in >> v) return v != 0;
    return true;
}

static void saveAutoDownload(const std::string &sdRoot, bool on)
{
    std::ofstream out(sdRoot + "/retrou/config/" RETROU_CORE_SYSTEM "/auto.conf", std::ios::trunc);
    if (out) out << (on ? 1 : 0) << "\n";
}

static void startAssetDownload(const std::string &sdRoot, const std::string &system,
                               std::vector<std::string> titles, bool covers,
                               bool backdrops, bool force)
{
    AssetJob job{sdRoot, system, std::move(titles), covers, backdrops, force};
    bool queued = false;
    {
        std::lock_guard<std::mutex> lock(g_downloadMutex);
        if (g_downloading) {
            g_assetQueue.push_back(std::move(job));
            g_downloadQueued = static_cast<int>(g_assetQueue.size());
            queued = true;
        } else {
            g_downloading = true;
        }
    }
    if (queued) {
        bootLog("asset_download_queued");
        return;
    }
    std::thread([job = std::move(job)]() mutable {
        for (;;) {
            g_downloadDone = false;
            g_downloadProgress = 0;
            g_downloadFailures = 0;
            g_downloadTotal = retrou::assets::pendingAssetCount(
                job.sdRoot, job.system, job.titles, job.covers, job.backdrops, job.force);
            retrou::assets::downloadAll(job.sdRoot, job.system, job.titles,
                                        job.covers, job.backdrops, job.force,
                                        &g_downloadProgress, &g_downloadFailures);
            if (job.covers) ++g_coverGeneration;
            std::lock_guard<std::mutex> lock(g_downloadMutex);
            if (g_assetQueue.empty()) {
                g_downloading = false;
                g_downloadDone = true;
                break;
            }
            job = std::move(g_assetQueue.front());
            g_assetQueue.pop_front();
            g_downloadQueued = static_cast<int>(g_assetQueue.size());
        }
    }).detach();
}

// ---- UI audio: 16-bit retro background loop + navigation blip (queue-based) ----
static SDL_AudioDeviceID g_uiDevice = 0;
static std::vector<int16_t> g_loop;
static std::vector<int16_t> g_blip;
static size_t g_loopPos = 0;
static bool g_navBlipPending = false;
static int g_uiVolume = 80;
static bool g_bgMusicOn = true;
static bool g_sfxOn = true;

static void initUiAudio()
{
    const int rate = 32000;
    const int blipFrames = static_cast<int>(rate * 0.05f);
    for (int i = 0; i < blipFrames; ++i) {
        const float t = static_cast<float>(i) / rate;
        const float w = std::sin(6.2831853f * 880.0f * t);
        g_blip.push_back(static_cast<int16_t>((w >= 0.0f ? 0.3f : -0.3f) * 32767.0f));
    }
    SDL_AudioSpec want{};
    want.freq = rate;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 512;
    want.callback = nullptr;
    SDL_AudioSpec obtained{};
    g_uiDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &obtained, 0);
    if (g_uiDevice) {
        SDL_PauseAudioDevice(g_uiDevice, 0);
    }
}

static void navBlip()
{
    if (!g_sfxOn) {
        return;
    }
    g_navBlipPending = true;
}

static void queueScaled(const int16_t *data, size_t samples)
{
    if (samples == 0) {
        return;
    }
    static std::vector<int16_t> tmp;
    const float gain = g_uiVolume / 100.0f;
    tmp.resize(samples * 2);
    for (size_t i = 0; i < samples; ++i) {
        const int16_t s = static_cast<int16_t>(data[i] * gain);
        tmp[i * 2] = s;
        tmp[i * 2 + 1] = s;
    }
    SDL_QueueAudio(g_uiDevice, tmp.data(), static_cast<Uint32>(tmp.size() * sizeof(int16_t)));
}

static void uiAudioTick()
{
    if (!g_uiDevice) {
        return;
    }
    if (g_bgMusicOn && !g_loop.empty()) {
        Uint32 queued = SDL_GetQueuedAudioSize(g_uiDevice);
        const Uint32 minBytes = 8192;
        while (queued < minBytes) {
            const size_t remaining = g_loop.size() - g_loopPos;
            const size_t n = std::min<size_t>(remaining, 2048);
            queueScaled(g_loop.data() + g_loopPos, n);
            g_loopPos += n;
            if (g_loopPos >= g_loop.size()) {
                g_loopPos = 0;
            }
            queued += static_cast<Uint32>(n * sizeof(int16_t));
        }
    }
    if (g_navBlipPending) {
        queueScaled(g_blip.data(), g_blip.size());
        g_navBlipPending = false;
    }
}

int main()
{
    bootLog("main_enter", true);
    bootLog("build " __DATE__ " " __TIME__);
    OSEnableHomeButtonMenu(TRUE);
    // Without ProcUI the system cannot ever show the HOME menu (and cannot
    // hand the screen over on power press); VPADRead is dead without
    // VPADInit. The save callback cannot be NULL (wut docs) and must call
    // OSSavesDone_ReadyToRelease() — same pattern as RetroArch's Wii U port.
    ProcUIInit([]() { OSSavesDone_ReadyToRelease(); });
    bootLog("procui_ready");
    VPADInit();
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
    bootLog("sdl_ready");
    initUiAudio();
    bootLog("audio_ready");

    if (!init()) {
        bootLog("gx2_init_failed");
        return 1;
    }
    bootLog("gx2_ready");
    if (!text::loadFont(_binary_ui_ttf_start,
                        static_cast<size_t>(_binary_ui_ttf_end - _binary_ui_ttf_start))) {
        bootLog("font_failed");
        return 1;
    }
    bootLog("font_ready");

    Library library;
    library.sd_root = mountSd();
    bootLog("sd_ready");
    mkdir((library.sd_root + "/retrou").c_str(), 0777);
    mkdir((library.sd_root + "/retrou/config").c_str(), 0777);
    mkdir(controlConfigDir(library.sd_root).c_str(), 0777);
    scanRoms(library, RETROU_CORE_SYSTEM);
    bootLog("rom_scan_ready");
    loadFavorites(library, RETROU_CORE_SYSTEM);
    int themeIndex = loadThemeIndex(library.sd_root);
    int videoAspect = 0;
    int videoScale = 0;
    int videoFilter = 1;
    loadVideoOptions(library.sd_root, videoAspect, videoScale, videoFilter);
    int player1Device = 0;
    int player2Device = 0;
    ActionSources vpadMap;
    ActionSources proMap;
    loadControlOptions(library.sd_root, player1Device, player2Device, vpadMap, proMap);
    MenuShortcuts menuShortcuts = loadMenuShortcuts(library.sd_root);
    int globalPlayer1 = player1Device;
    int globalPlayer2 = player2Device;
    ActionSources globalVpadMap = vpadMap;
    ActionSources globalProMap = proMap;
    mkdir((controlConfigDir(library.sd_root) + "/profiles").c_str(), 0777);
    mkdir((controlConfigDir(library.sd_root) + "/rom-maps").c_str(), 0777);
    auto mapOfDevice = [&](int device) -> ActionSources & {
        return device == 0 ? vpadMap : proMap;
    };
    // CTRL-3: editor de mapeo estilo Our64 (acciones -> botones físicos).
    bool mapMode = false;
    bool profileMode = false;
    int profileSlot = 0;
    std::string profileStatus;
    bool orderMode = false;
    int orderPort = 0;
    bool orderArmed = false;
    int orderOriginal1 = 0;
    int orderOriginal2 = 0;
    int mapDevice = 0;      // 0 = GamePad, 1 = Pro/Classic
    int mapRow = 0;         // índice sobre consoleActions()
    int mapTop = 0;         // primera fila visible (paginación, 4 por página)
    int mapCapture = 0;     // 0 = no, 1 = reemplazar (A), 2 = añadir (Y)
    double mapCaptureUntil = 0.0;
    bool mapCaptureArmed = false; // espera a que se suelten los botones antes de aceptar
    int columns = 5;
    bool showCovers = true;
    int uiVolume = 80;
    bool bgMusicOn = true;
    bool sfxOn = true;
    int coreVolume = 80;
    loadAppSettings(library.sd_root, columns, showCovers, uiVolume, bgMusicOn, sfxOn, coreVolume);
    int viewMode = 0; // cards, lista, carrusel
    bool dynamicBg = true;
    constexpr int backdropDelaysMs[] = {0, 1000, 3000, 5000, 10000};
    int backdropDelayIndex = 2;
    {
        std::ifstream viewIn(library.sd_root + "/retrou/config/" RETROU_CORE_SYSTEM "/view.conf");
        int backdrop = 1;
        if (viewIn >> viewMode >> backdrop) {
            viewMode = std::max(0, std::min(2, viewMode));
            dynamicBg = backdrop != 0;
            int savedDelay = 2;
            if (viewIn >> savedDelay)
                backdropDelayIndex = std::max(0, std::min(4, savedDelay));
        }
    }
    auto saveViewOptions = [&]() {
        std::ofstream viewOut(library.sd_root + "/retrou/config/" RETROU_CORE_SYSTEM "/view.conf", std::ios::trunc);
        if (viewOut) viewOut << viewMode << " " << (dynamicBg ? 1 : 0) << " "
                             << backdropDelayIndex << "\n";
    };
    g_uiVolume = uiVolume;
    g_bgMusicOn = bgMusicOn;
    g_sfxOn = sfxOn;
    bool autoDownload = loadAutoDownload(library.sd_root);
    bool downloadAllMode = false;
    std::vector<std::string> autoTitles;
    if (autoDownload)
        for (const auto &rom : library.roms) autoTitles.push_back(rom.title);
    // AS-2: metadata se actualiza al inicio incluso con auto-descarga apagada.
    startAssetDownload(library.sd_root, RETROU_CORE_SYSTEM, autoTitles,
                       autoDownload, autoDownload, false);

    std::map<std::string, std::string> metaDev, metaYear;
    loadMetaMap(library.sd_root, RETROU_CORE_SYSTEM, "developer", metaDev);
    loadMetaMap(library.sd_root, RETROU_CORE_SYSTEM, "releaseyear", metaYear);
    bool metaApplied = false;
    int settingsSection = -1; // -1 = section list, 0..4 = inside a section
    int sectionCursor = 0;
    int settingsItem = 0;
    bool searchActive = false;
    std::string searchQuery;
    int searchCursor = 0;
    int scroll = 0;

    std::vector<int> visible;
    for (int i = 0; i < static_cast<int>(library.roms.size()); ++i) {
        visible.push_back(i);
    }
    bool favoritesOnly = false;
    bool sidebarOpen = false;
    bool settingsOpen = false;
    uint32_t lastStickDir = 0; // última dirección del stick para la navegación UI
    uint32_t wpadNavLastBits[4] = {0, 0, 0, 0}; // CTRL-4: edge por mando externo
    bool lastMenuShortcutHeld[5] = {false, false, false, false, false};
    int sidebarIndex = 0;
    int focused = 0;
    int lastLoggedGbaRom = -1;
    int lastRenderedGbaRom = -1;
    int sortMode = 0;      // 0 name, 1 region
    int regionFilter = 0;  // 0 all, 1 USA, 2 Europe, 3 Japan
    int alphaFilter = 0;   // 0 = All, 1 = favoritos, 2 = #, 3..28 = A..Z

    auto rebuildVisible = [&]() {
        visible.clear();
        const std::string q = toLower(searchQuery);
        for (int i = 0; i < static_cast<int>(library.roms.size()); ++i) {
            if (favoritesOnly && !library.roms[i].favorite) continue;
            if (regionFilter != 0 && detectRegion(library.roms[i].title) != regionFilter) {
                continue;
            }
            if (viewMode != 0 && alphaFilter >= 2) {
                const unsigned char first = library.roms[i].title.empty() ? 0 :
                    static_cast<unsigned char>(library.roms[i].title.front());
                const char letter = static_cast<char>(std::toupper(first));
                if (alphaFilter == 2 ? (letter >= 'A' && letter <= 'Z')
                                     : letter != 'A' + alphaFilter - 3) continue;
            }
            if (!q.empty() && toLower(library.roms[i].title).find(q) == std::string::npos) {
                continue;
            }
            visible.push_back(i);
        }
        std::sort(visible.begin(), visible.end(), [&](int a, int b) {
            const RomEntry &ra = library.roms[a];
            const RomEntry &rb = library.roms[b];
            if (sortMode == 1) {
                const int ka = detectRegion(ra.title);
                const int kb = detectRegion(rb.title);
                const int oa = ka == 0 ? 4 : ka;
                const int ob = kb == 0 ? 4 : kb;
                if (oa != ob) return oa < ob;
            } else if (sortMode == 2) {
                auto ita = metaYear.find(ra.title);
                auto itb = metaYear.find(rb.title);
                const std::string ya = ita != metaYear.end() ? ita->second : titleYear(ra.title);
                const std::string yb = itb != metaYear.end() ? itb->second : titleYear(rb.title);
                if (ya != yb) return ya < yb;
            } else if (sortMode == 3) {
                auto ita = metaDev.find(ra.title);
                auto itb = metaDev.find(rb.title);
                const std::string da = ita != metaDev.end() ? ita->second : std::string();
                const std::string db = itb != metaDev.end() ? itb->second : std::string();
                if (da != db) return da < db;
            } else if (sortMode == 4) {
                if (ra.favorite != rb.favorite) return ra.favorite;
            }
            return ra.title < rb.title;
        });
        if (focused >= static_cast<int>(visible.size())) focused = 0;
        scroll = 0;
    };

    const int rows = 2;
    const float gridX = 24.0f, gridY = 88.0f;
    const float gridW = 1280.0f - 48.0f, gridH = 560.0f;
    const float gap = 14.0f;
    float cardW = (gridW - gap * (columns - 1)) / columns;
    float cardH = (gridH - gap * (rows - 1)) / rows;
    float coverH = cardH * 0.62f;
    int lastColumns = columns;

    std::map<int, Texture> coverCache;
    std::vector<int> coverOrder;
    static Texture thumbTex = -2;
    if (thumbTex == -2) {
        thumbTex = image::loadFromMemory(
            _binary_icon_png_start,
            static_cast<size_t>(_binary_icon_png_end - _binary_icon_png_start));
        if (thumbTex < 0) {
            thumbTex = image::loadFromFile(
                (library.sd_root + "/retrou/thumb_" + RETROU_CORE_SYSTEM + ".png").c_str());
        }
        if (thumbTex < 0) thumbTex = makeCover(54, 72, 120, 10, 14, 24);
        bootLog(thumbTex >= 0 ? "cover_fallback_ready" : "cover_fallback_failed");
    }
    auto getCover = [&](int romIndex) -> Texture {
        auto it = coverCache.find(romIndex);
        if (it != coverCache.end()) {
            coverOrder.erase(std::remove(coverOrder.begin(), coverOrder.end(), romIndex),
                             coverOrder.end());
            coverOrder.push_back(romIndex);
            return it->second;
        }
        constexpr size_t kMaxCachedCovers = 16;
        if (coverOrder.size() >= kMaxCachedCovers) {
            const int oldest = coverOrder.front();
            coverOrder.erase(coverOrder.begin());
            const Texture oldTexture = coverCache[oldest];
            if (oldTexture >= 0 && oldTexture != thumbTex) destroyTexture(oldTexture);
            coverCache.erase(oldest);
        }
        const std::string &title = library.roms[romIndex].title;
        const bool diagnoseGbaP = std::string(RETROU_CORE_SYSTEM) == "gba" &&
            !title.empty() && std::toupper(static_cast<unsigned char>(title.front())) == 'P';
        if (diagnoseGbaP) bootLog(("gba_cover_load title=" + title).c_str());
        std::string cp = coverPath(library.sd_root, RETROU_CORE_SYSTEM, library.roms[romIndex].title);
        Texture real = image::loadFromFile(cp.c_str());
        if (real < 0) real = thumbTex;
        coverCache[romIndex] = real;
        coverOrder.push_back(romIndex);
        if (diagnoseGbaP) bootLog(("gba_cover_ready title=" + title).c_str());
        return real;
    };
    auto clearCoverCache = [&]() {
        for (auto &entry : coverCache)
            if (entry.second >= 0 && entry.second != thumbTex)
                destroyTexture(entry.second);
        coverCache.clear();
        coverOrder.clear();
    };
    unsigned seenCoverGeneration = 0;

    std::map<int, Texture> backdropCache;
    auto getBackdrop = [&](int romIndex) -> Texture {
        auto it = backdropCache.find(romIndex);
        if (it != backdropCache.end()) return it->second;
        const std::string safe = assetFileStem(library.roms[romIndex].title);
        const std::string path = library.sd_root + "/retrou/backdrops/" +
                                 RETROU_CORE_SYSTEM + "/" + safe + ".png";
        Texture bd = image::loadFromFileBlurred(path.c_str(), 64, 36);
        backdropCache[romIndex] = bd;
        return bd;
    };
    // CTRL-3: imagen del mando de la consola para el editor de mapeo
    // (carga perezosa del PNG incrustado).
    Texture padTexture = -1;
    auto getPadTexture = [&]() -> Texture {
        if (padTexture < 0) {
            padTexture = image::loadFromMemory(
                RETROU_PAD_START(RETROU_PAD_PNG),
                static_cast<size_t>(RETROU_PAD_END(RETROU_PAD_PNG) -
                                    RETROU_PAD_START(RETROU_PAD_PNG)));
        }
        return padTexture;
    };

    uint32_t focusedTick = SDL_GetTicks();
    int lastFocused = -1;
    int lastFocusedRom = -1;
    int activeRom = -1;

    const std::string saveRoot = library.sd_root + "/retrou/saves/" RETROU_CORE_SYSTEM;
    mkdir((library.sd_root + "/retrou/saves").c_str(), 0777);
    mkdir(saveRoot.c_str(), 0777);
    mkdir((saveRoot + "/save").c_str(), 0777);
    const std::string snapRoot = library.sd_root + "/retrou/snap/" RETROU_CORE_SYSTEM;
    mkdir((library.sd_root + "/retrou/snap").c_str(), 0777);
    mkdir(snapRoot.c_str(), 0777);
    auto snapPath = [&](int slot) -> std::string {
        std::string safe = activeRom >= 0 ? library.roms[activeRom].title : std::string("state");
        for (char &c : safe) {
            if (std::string("&*/:`<>?\\|\"").find(c) != std::string::npos) c = '_';
        }
        return snapRoot + "/" + safe + "-" + std::to_string(slot + 1) + ".state";
    };

    std::map<int, std::string> titleCache;
    auto getTitle = [&](int romIndex) -> const std::string & {
        auto it = titleCache.find(romIndex);
        if (it != titleCache.end()) return it->second;
        titleCache[romIndex] =
            text::fitText(library.roms[romIndex].title.c_str(), 15, cardW - 24.0f);
        return titleCache[romIndex];
    };

    text::prewarm("Biblioteca", 34);
    text::prewarm("FILTRAR Y ORGANIZAR", 16);
    text::prewarm("Buscar", 15);
    text::prewarm("Ordenar por", 15);
    text::prewarm("Región", 15);
    text::prewarm("Favoritos", 15);
    text::prewarm("Nombre", 15);
    text::prewarm("Todas", 15);
    text::prewarm("USA", 15);
    text::prewarm("Europa", 15);
    text::prewarm("Japón", 15);
    text::prewarm("Sí", 15);
    text::prewarm("No", 15);
    text::prewarm("Ajustes", 34);
    text::prewarm("PERSONALIZAR", 24);
    text::prewarm("ASSETS", 24);
    text::prewarm("CONTROLES", 24);
    text::prewarm("SONIDO", 24);
    text::prewarm("VÍDEO", 24);
    text::prewarm("Tema", 22);
    text::prewarm("Gris", 22);
    text::prewarm("Azul", 22);
    text::prewarm("Verde", 22);
    text::prewarm("Dorado", 22);
    text::prewarm("Rosa", 22);
    text::prewarm("Original", 22);
    text::prewarm("Ajustar", 22);
    text::prewarm("Entero", 22);
    text::prewarm("Estirar", 22);
    text::prewarm("Aspecto", 22);
    text::prewarm("Escala", 22);
    text::prewarm("Pendiente", 22);
    text::prewarm("Mando 1", 22);
    text::prewarm("GamePad", 22);
    text::prewarm("Wii U Pro Controller", 22);
    text::prewarm("Classic Controller", 22);
    text::prewarm("Wii Remote + Nunchuk", 22);
    text::prewarm("Wii Remote", 22);
    text::prewarm("Mapear botones", 34);
    text::prewarm("Mapear botones  (A)", 22);
    text::prewarm("Mando 2", 22);
    text::prewarm("Sin mapear", 24);
    text::prewarm("Stick Arriba", 24);
    text::prewarm("Stick Abajo", 24);
    text::prewarm("Stick Izquierda", 24);
    text::prewarm("Stick Derecha", 24);
    text::prewarm("Pulsa un botón para ", 22);
    text::prewarm("Pulsa un botón para añadir a ", 22);
    text::prewarm("La captura admite GamePad o Pro/Classic según el dispositivo", 14);
    text::prewarm("Mando Pro / Classic", 16);
    text::prewarm("A  CAMBIAR    Y  AÑADIR    X  DESASIGNAR    -  REINICIAR    L/R  DISPOSITIVO    B  VOLVER", 14);
    for (int i = 0; i < 12; ++i) text::prewarm(retroName(i), 24);
    text::prewarm("Dispositivo: GamePad    (L/R cambia)", 16);
    text::prewarm("Dispositivo: Mando Pro / Classic    (L/R cambia)", 16);
    text::prewarm("Columnas", 22);
    text::prewarm("Carátulas", 22);
    text::prewarm("Música", 22);
    text::prewarm("Efectos", 22);
    text::prewarm("Volumen", 22);
    text::prewarm("ESP", 16);
    text::prewarm("BORR", 16);
    text::prewarm("OK", 16);
    text::prewarm("Buscar...", 24);
    text::prewarm("A  TECLA    B  BORRAR    X  ACEPTAR    L  CANCELAR", 15);
    text::prewarm("A  ABRIR    B  BIBLIOTECA", 15);
    text::prewarm("IZQ/DER  CAMBIAR    B  ATRÁS", 15);
    text::prewarm("Sin ROMs", 24);
    text::prewarm("A  JUGAR    X  FAVORITO    -  FILTROS    +  AJUSTES", 15);
    text::prewarm("MENU", 26);
    text::prewarm("Continuar", 20);
    text::prewarm("Guardar estado", 20);
    text::prewarm("Cargar estado", 20);
    text::prewarm("Cerrar core", 20);
    text::prewarm(RETROU_CORE_LABEL, 12);

    RplCore core;
    std::string error;
    bootLog("before_static_core");
    if (!core.loadStaticCore(nullptr, nullptr, error)) {
        bootLog("static_core_failed");
        return 1;
    }
    bootLog("static_core_ready");
    core.setSaveDirectory(saveRoot + "/save");
    core.setControllers(nullptr, nullptr);
    core.setVpadEnabled(true);
    core.setVideoOptions(videoAspect, videoScale, videoFilter);
    core.setExternalAudioDevice(g_uiDevice);
    core.setAudioOptions(coreVolume, 0);

    wpad_input_init();
    // Controllers are re-detected in the loop (hot-plug): poll first, then
    // rebuild the device list and the core port assignment when it changes.
    std::vector<int> proChannels;
    std::vector<std::string> devOpts;   // core port 1 options: GamePad + Pros
    std::vector<std::string> p2Opts;    // core port 2 options: none + Pros
    auto applyControlMaps = [&]() {
        core.setVpadActionMap(vpadMap);
        core.setWpadActionMap(proMap);
    };
    auto refreshControllers = [&]() {
        wpad_input_poll();
        proChannels = wpad_input_channels();
        const auto &states = wpad_input_states();
        // Sufijo numerico solo cuando hay mas de un mando del mismo tipo.
        int typeCount[5] = {0, 0, 0, 0, 0};
        int typeSeen[5] = {0, 0, 0, 0, 0};
        for (int ch : proChannels) typeCount[states[ch].type]++;
        devOpts.clear();
        devOpts.push_back("GamePad");
        p2Opts.clear();
        p2Opts.push_back("-");
        p2Opts.push_back("GamePad");
        for (int ch : proChannels) { // todos los mandos conectados (4 canales)
            const int t = states[ch].type;
            std::string name = wpadTypeLabel(t);
            if (typeCount[t] > 1) name += " " + std::to_string(++typeSeen[t]);
            else ++typeSeen[t];
            devOpts.push_back(name);
            p2Opts.push_back(name);
        }
        if (player1Device >= static_cast<int>(devOpts.size())) player1Device = 0;
        if (player2Device >= static_cast<int>(p2Opts.size())) player2Device = 0;
        auto chan1 = [&](int dev) -> int {
            // dev 0 = GamePad (sin canal WPAD), dev 1.. = mando externo.
            return dev >= 1 && dev <= static_cast<int>(proChannels.size())
                ? proChannels[dev - 1] : -1;
        };
        auto chan2 = [&](int dev) -> int {
            // dev 0 = ninguno, dev 1 = GamePad (sin canal WPAD), dev 2.. = mando externo.
            if (dev == 1) return -1;
            return dev >= 2 && dev <= static_cast<int>(proChannels.size()) + 1
                ? proChannels[dev - 2] : -1;
        };
        core.setWpadPorts(chan1(player1Device), hasSecondCorePort() ? chan2(player2Device) : -1);
        // El GamePad alimenta el puerto del core donde esté asignado
        // (Mando 1 tiene prioridad si está en ambos).
        int vpadPort = -1;
        if (player1Device == 0) vpadPort = 0;
        else if (hasSecondCorePort() && player2Device == 1) vpadPort = 1;
        core.setVpadPort(vpadPort);
        applyControlMaps();
    };
    refreshControllers();

    bool inGame = false;
    auto persistControls = [&]() {
        if (inGame && activeRom >= 0) {
            bootLog(saveControlOptions(library.sd_root, player1Device, player2Device,
                                       vpadMap, proMap,
                                       romControlPath(library.sd_root, library.roms[activeRom].path))
                        ? "rom_controls_saved" : "rom_controls_save_failed");
        } else {
            const bool saved = saveControlOptions(library.sd_root, player1Device, player2Device,
                                                  vpadMap, proMap);
            globalPlayer1 = player1Device;
            globalPlayer2 = player2Device;
            globalVpadMap = vpadMap;
            globalProMap = proMap;
            bootLog(saved ? "global_controls_saved" : "global_controls_save_failed");
        }
    };
    auto restoreGlobalControls = [&]() {
        player1Device = globalPlayer1;
        player2Device = globalPlayer2;
        vpadMap = globalVpadMap;
        proMap = globalProMap;
        refreshControllers();
        bootLog("global_controls_restored");
    };
    bool coreMenu = false;
    int coreMenuIndex = 0;
    int slotMode = 0; // 0 = menú, 1 = guardar, 2 = cargar
    int slotIndex = 0;
    std::array<Texture, 4> slotThumbs{{-1, -1, -1, -1}};
    auto slotThumbPath = [&](int slot) {
        const std::string state = snapPath(slot);
        return state.substr(0, state.size() - 6) + ".png";
    };
    auto refreshSlotThumbs = [&]() {
        for (int slot = 0; slot < 4; ++slot) {
            if (slotThumbs[slot] >= 0) destroyTexture(slotThumbs[slot]);
            slotThumbs[slot] = image::loadFromFile(slotThumbPath(slot).c_str());
        }
    };
    bool waitingRelease = false;
    bool touchHeld = false;
    Texture frameTex = -1;

    const Color white{255, 255, 255, 255};
    const Color muted{148, 163, 184, 255};
    const Color panel{21, 27, 40, 235};
    const Color cardPanel{16, 21, 33, 235};
    const Color cardFooter{10, 14, 24, 235};
    const Color gold{245, 217, 127, 255};

    while (true) {
        // System ownership of the screen (HOME menu, power dialog, ...): the
        // app only draws and reads input in the foreground. ProcUI runs with
        // block=TRUE (the non-blocking return value is undefined). On
        // RELEASE_FOREGROUND the system waits for ProcUIDrawDoneRelease()
        // before it can show the HOME menu; the game stays paused (the core
        // runs in this loop) and nothing is drawn until we get the screen
        // back (same pattern as RetroArch's Wii U port).
        const ProcUIStatus pui = ProcUIProcessMessages(TRUE);
        if (pui == PROCUI_STATUS_EXITING) break;
        if (pui == PROCUI_STATUS_RELEASE_FOREGROUND) ProcUIDrawDoneRelease();
        if (!ProcUIInForeground()) {
            waitingRelease = true; // do not replay the buttons held at the switch
            OSSleepTicks(OSMillisecondsToTicks(16));
            continue;
        }

        const Color bg = kThemes[themeIndex].bgBottom;
        const Color bgTop = kThemes[themeIndex].bgTop;
        const Color accent = kThemes[themeIndex].accent;

        if (columns != lastColumns) {
            cardW = (gridW - gap * (columns - 1)) / columns;
            cardH = (gridH - gap * (rows - 1)) / rows;
            coverH = cardH * 0.62f;
            titleCache.clear();
            lastColumns = columns;
            scroll = 0;
        }

        if (g_downloadDone && !metaApplied) {
            metaDev.clear();
            metaYear.clear();
            loadMetaMap(library.sd_root, RETROU_CORE_SYSTEM, "developer", metaDev);
            loadMetaMap(library.sd_root, RETROU_CORE_SYSTEM, "releaseyear", metaYear);
            for (auto &entry : backdropCache)
                if (entry.second >= 0) destroyTexture(entry.second);
            backdropCache.clear();
            metaApplied = true;
        }
        const unsigned coverGeneration = g_coverGeneration.load();
        if (coverGeneration != seenCoverGeneration) {
            clearCoverCache();
            seenCoverGeneration = coverGeneration;
            bootLog("covers_refreshed");
        }

        setNearestFilter(videoFilter == 0);

        const int focusedRom = !visible.empty() && focused >= 0 &&
            focused < static_cast<int>(visible.size()) ? visible[focused] : -1;
        if (focused != lastFocused || focusedRom != lastFocusedRom) {
            lastFocused = focused;
            lastFocusedRom = focusedRom;
            focusedTick = SDL_GetTicks();
        }

        // Hot-plug: re-detect Pro Controllers when the connected set changes.
        wpad_input_poll();
        core.pushWpadSamples(wpad_input_states()); // single poll per frame; the core reuses it
        if (wpad_input_channels() != proChannels) {
            refreshControllers();
        }

        VPADStatus vpad{};
        VPADReadError vpadError = VPAD_READ_NO_SAMPLES;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadError);
        core.pushVpadSample(vpad, vpadError); // single read per frame; the core reuses it
        // CTRL-4: los mandos externos navegan la UI (botones + stick como D-Pad).
        uint32_t wpadTrigger = 0;
        const auto &wpadStates = wpad_input_states();
        for (int ch = 0; ch < 4; ++ch) {
            wpadTrigger |= wpadNavBits(wpadStates[ch], wpadNavLastBits[ch]);
        }
        int openCoreMenuSource = -1;
        const bool gamePadCombo = vpadError == VPAD_READ_SUCCESS &&
            menuShortcutHeld(vpad.hold, 0, menuShortcuts.choice[0]);
        if (gamePadCombo && !lastMenuShortcutHeld[0]) openCoreMenuSource = 0;
        lastMenuShortcutHeld[0] = gamePadCombo;
        for (int ch = 0; ch < 4; ++ch) {
            const WpadSnapshot &s = wpadStates[ch];
            const int family = (s.type == WPAD_TYPE_PRO || s.type == WPAD_TYPE_CLASSIC) ? 1 : 2;
            const bool held = s.connected && menuShortcutHeld(s.buttons, family,
                                                               menuShortcuts.choice[family]);
            if (held && !lastMenuShortcutHeld[ch + 1]) openCoreMenuSource = ch + 1;
            lastMenuShortcutHeld[ch + 1] = held;
        }
        uint32_t navTrigger = vpad.trigger | stickNavBits(vpad, lastStickDir) | wpadTrigger;
        // NAV-1: el touch del GamePad usa las mismas acciones que el mando.
        if (vpadError == VPAD_READ_SUCCESS) {
            VPADTouchData touch{};
            VPADGetTPCalibratedPointEx(VPAD_CHAN_0, VPAD_TP_1280X720,
                                       &touch, &vpad.tpNormal);
            const bool down = touch.touched != 0;
            if (down && !touchHeld) {
                const float tx = touch.x, ty = touch.y;
                if (tx < 155 && ty < 85 && (coreMenu || mapMode || orderMode || settingsOpen || sidebarOpen || searchActive)) {
                    navTrigger |= VPAD_BUTTON_B;
                } else if (inGame && coreMenu && !mapMode) {
                    if (slotMode != 0 && tx >= 315 && tx < 965 && ty >= 195 && ty < 555) {
                        slotIndex = std::min(3, static_cast<int>((ty - 195) / 88));
                        navTrigger |= VPAD_BUTTON_A;
                    } else if (slotMode == 0 && tx >= 390 && tx < 890 && ty >= 260 && ty < 480) {
                        coreMenuIndex = std::min(4, static_cast<int>((ty - 260) / 44));
                        navTrigger |= VPAD_BUTTON_A;
                    }
                } else if (mapMode && profileMode && tx >= 120 && tx < 930 && ty >= 190 && ty < 460) {
                    profileSlot = std::min(2, static_cast<int>((ty - 190) / 90));
                    navTrigger |= VPAD_BUTTON_A;
                } else if (mapMode && !profileMode && !mapCapture && tx >= 120 && tx < 930 && ty >= 100 && ty < 440) {
                    const int index = mapTop + static_cast<int>((ty - 100) / 64);
                    if (index < static_cast<int>(consoleActions().size())) {
                        mapRow = index;
                        navTrigger |= VPAD_BUTTON_A;
                    }
                } else if (searchActive && tx >= 150 && tx < 750 && ty >= 130 && ty < 538) {
                    const int col = static_cast<int>((tx - 150) / 100);
                    const int row = static_cast<int>((ty - 130) / 58);
                    const int key = row * 6 + col;
                    if (key < 39) { searchCursor = key; navTrigger |= VPAD_BUTTON_A; }
                } else if (settingsOpen && tx >= 50 && tx < 810 && ty >= 105) {
                    if (settingsSection == -1 && ty < 520) {
                        sectionCursor = std::min(4, static_cast<int>((ty - 105) / 80));
                        navTrigger |= VPAD_BUTTON_A;
                    } else if (settingsSection >= 0 && ty < (settingsSection == 2 ? 610 : 520)) {
                        const int counts[] = {5, 5, hasSecondCorePort() ? 7 : 6, 3, 2};
                        settingsItem = std::min(counts[settingsSection] - 1,
                                                static_cast<int>((ty - 105) / 70));
                        navTrigger |= settingsSection == 1 && settingsItem >= 3 ? VPAD_BUTTON_A : VPAD_BUTTON_RIGHT;
                        if (settingsSection == 2 && settingsItem >= (hasSecondCorePort() ? 2 : 1))
                            navTrigger |= VPAD_BUTTON_A;
                    }
                } else if (sidebarOpen && tx >= 36 && tx < 350 && ty >= 140 && ty < 330) {
                    sidebarIndex = std::min(3, static_cast<int>((ty - 140) / 44));
                    navTrigger |= VPAD_BUTTON_A;
                } else if (!inGame && !settingsOpen && !sidebarOpen && !searchActive && ty > 650) {
                    navTrigger |= tx < 500 ? VPAD_BUTTON_MINUS : VPAD_BUTTON_PLUS;
                } else if (!inGame && !settingsOpen && !sidebarOpen && !searchActive && viewMode != 0 &&
                           ty >= 75 && ty < 120 && tx >= 40 && tx < 1270) {
                    alphaFilter = std::min(28, static_cast<int>((tx - 40) / 43));
                    favoritesOnly = alphaFilter == 1;
                    rebuildVisible();
                } else if (!inGame && !settingsOpen && !sidebarOpen && !searchActive && viewMode == 1 &&
                           tx >= 500 && ty >= 142 && ty < 592) {
                    const int index = scroll + static_cast<int>((ty - 142) / 90);
                    if (index < static_cast<int>(visible.size())) { focused = index; navTrigger |= VPAD_BUTTON_A; }
                } else if (!inGame && !settingsOpen && !sidebarOpen && !searchActive && viewMode == 2 &&
                           ty >= 130 && ty < 580) {
                    navTrigger |= tx < 290 ? VPAD_BUTTON_LEFT : tx > 990 ? VPAD_BUTTON_RIGHT : VPAD_BUTTON_A;
                } else if (!inGame && !settingsOpen && !sidebarOpen && !searchActive && viewMode == 0 &&
                           tx >= gridX && ty >= gridY && ty < gridY + gridH) {
                    const int col = static_cast<int>((tx - gridX) / (cardW + gap));
                    const int row = static_cast<int>((ty - gridY) / (cardH + gap));
                    const int index = scroll + row * columns + col;
                    if (col >= 0 && col < columns && row >= 0 && row < 2 && index < static_cast<int>(visible.size())) {
                        focused = index;
                        navTrigger |= VPAD_BUTTON_A;
                    }
                }
            }
            touchHeld = down;
        }
        if (vpadError == VPAD_READ_SUCCESS || !proChannels.empty()) {
            if (inGame && !mapMode && !orderMode) {
                if (coreMenu) {
                    if (slotMode != 0) {
                        if (navTrigger & VPAD_BUTTON_DOWN) slotIndex = (slotIndex + 1) % 4;
                        else if (navTrigger & VPAD_BUTTON_UP) slotIndex = (slotIndex + 3) % 4;
                        else if (navTrigger & VPAD_BUTTON_B) slotMode = 0;
                        else if (navTrigger & VPAD_BUTTON_A) {
                            if (slotMode == 1) {
                                if (core.saveState(snapPath(slotIndex))) {
                                    saveFrameThumbnail(core, slotThumbPath(slotIndex));
                                    refreshSlotThumbs();
                                }
                            } else {
                                core.loadState(snapPath(slotIndex));
                            }
                            slotMode = 0;
                        }
                    } else if (navTrigger & VPAD_BUTTON_DOWN) {
                        coreMenuIndex = (coreMenuIndex + 1) % 5;
                    } else if (navTrigger & VPAD_BUTTON_UP) {
                        coreMenuIndex = (coreMenuIndex + 4) % 5;
                    } else if (navTrigger & VPAD_BUTTON_A) {
                        if (coreMenuIndex == 0) {
                            coreMenu = false;
                            core.setInputEnabled(false);
                            waitingRelease = true;
                        } else if (coreMenuIndex == 1) {
                            slotMode = 1;
                            slotIndex = 0;
                            refreshSlotThumbs();
                        } else if (coreMenuIndex == 2) {
                            slotMode = 2;
                            slotIndex = 0;
                            refreshSlotThumbs();
                        } else if (coreMenuIndex == 3) {
                            mapMode = true;
                            profileMode = false;
                            profileStatus.clear();
                            mapRow = 0;
                            mapTop = 0;
                            mapDevice = 0;
                            mapCapture = 0;
                        } else {
                            core.unloadGame();
                            inGame = false;
                            coreMenu = false;
                            activeRom = -1;
                            restoreGlobalControls();
                        }
                    } else if (navTrigger & VPAD_BUTTON_B) {
                        coreMenu = false;
                        core.setInputEnabled(false);
                        waitingRelease = true;
                    }
                } else {
                    if (openCoreMenuSource >= 0) {
                        coreMenu = true;
                        coreMenuIndex = 0;
                        const std::string event = "core_menu_shortcut_opened source=" +
                            (openCoreMenuSource == 0 ? std::string("gamepad") :
                             "wpad_ch" + std::to_string(openCoreMenuSource - 1));
                        bootLog(event.c_str());
                    }
                }
            } else if (mapMode) {
                // CTRL-3: editor estilo Our64. Filas = acciones del core;
                // A reemplaza (con intercambio), Y añade, X desasigna,
                // - reinicia el dispositivo, L/R cambia GamePad/Pro.
                const std::vector<int> acts = consoleActions();
                const int nRows = static_cast<int>(acts.size());
                constexpr int kMapShown = 4;
                ActionSources &m = mapOfDevice(mapDevice);
                if (profileMode) {
                    if (navTrigger & VPAD_BUTTON_B) {
                        profileMode = false;
                    } else if (navTrigger & VPAD_BUTTON_UP) {
                        profileSlot = (profileSlot + 2) % 3;
                    } else if (navTrigger & VPAD_BUTTON_DOWN) {
                        profileSlot = (profileSlot + 1) % 3;
                    } else if (navTrigger & VPAD_BUTTON_A) {
                        if (loadActionProfile(controlProfilePath(library.sd_root, mapDevice, profileSlot), m)) {
                            persistControls();
                            applyControlMaps();
                            bootLog("control_profile_loaded");
                            profileStatus = "Perfil cargado";
                        } else {
                            bootLog("control_profile_load_failed");
                            profileStatus = "No se pudo cargar el perfil";
                        }
                    } else if (navTrigger & VPAD_BUTTON_Y) {
                        const bool saved = saveActionProfile(
                            controlProfilePath(library.sd_root, mapDevice, profileSlot), m);
                        bootLog(saved ? "control_profile_saved" : "control_profile_save_failed");
                        profileStatus = saved ? "Perfil guardado" : "No se pudo guardar el perfil";
                    }
                } else if (mapCapture) {
                    // Captura: cualquier mando del tipo editado puede pulsar
                    // (GamePad para device 0; cualquier Pro/Classic para 1).
                    uint32_t held = 0;
                    if (mapDevice == 0) {
                        if (vpadError == VPAD_READ_SUCCESS) {
                            held = vpad.hold |
                stickSourceBits(vpad.leftStick.x, vpad.leftStick.y,
                                vpad.rightStick.x, vpad.rightStick.y);
                        }
                    } else {
                        const auto &wpadStates = wpad_input_states();
                        for (int ch = 0; ch < 4; ++ch) {
                            const WpadSnapshot &s = wpadStates[ch];
                            if (s.type != WPAD_TYPE_PRO && s.type != WPAD_TYPE_CLASSIC) continue;
                            held |= s.buttons |
                                stickSourceBits(s.left_x / 32767.0f, s.left_y / 32767.0f,
                                                s.right_x / 32767.0f, s.right_y / 32767.0f);
                        }
                    }
                    const double nowS = SDL_GetTicks() / 1000.0;
                    if (!mapCaptureArmed && held == 0) {
                        mapCaptureArmed = true;
                    }
                    if (mapCaptureArmed && held != 0) {
                        // Our64: un solo botón (el más bajo de los pulsados).
                        const uint32_t one = held & (~held + 1);
                        const int row = acts[mapRow];
                        if (mapCapture == 2) {
                            // Y Add: comparte el botón con otras acciones.
                            m.sources[row] |= one;
                        } else {
                            // A Change: regla un-botón-una-acción con
                            // intercambio (la acción desplazada recibe los
                            // botones viejos de esta fila).
                            const uint32_t old = m.sources[row];
                            int displaced = -1;
                            for (int z = 0; z < 12; ++z) {
                                if (z != row && (m.sources[z] & one)) {
                                    m.sources[z] &= ~one;
                                    if (displaced < 0) displaced = z;
                                }
                            }
                            m.sources[row] = one;
                            if (displaced >= 0 && old != 0) m.sources[displaced] = old;
                        }
                        persistControls();
                        applyControlMaps();
                        navBlip();
                        mapCapture = 0;
                    } else if (nowS >= mapCaptureUntil) {
                        mapCapture = 0; // caducó sin pulsación
                    }
                } else {
                    if (navTrigger & VPAD_BUTTON_B) {
                        mapMode = false;
                    } else if (navTrigger & VPAD_BUTTON_UP) {
                        mapRow = (mapRow + nRows - 1) % nRows;
                        if (mapRow < mapTop) mapTop = mapRow;
                        if (mapRow >= mapTop + kMapShown) mapTop = mapRow - kMapShown + 1;
                        navBlip();
                    } else if (navTrigger & VPAD_BUTTON_DOWN) {
                        mapRow = (mapRow + 1) % nRows;
                        if (mapRow < mapTop) mapTop = mapRow;
                        if (mapRow >= mapTop + kMapShown) mapTop = mapRow - kMapShown + 1;
                        navBlip();
                    } else if (navTrigger & (VPAD_BUTTON_L | VPAD_BUTTON_R)) {
                        mapDevice = 1 - mapDevice;
                        mapRow = 0;
                        mapTop = 0;
                        navBlip();
                    } else if (navTrigger & VPAD_BUTTON_PLUS) {
                        profileMode = true;
                        profileSlot = 0;
                        profileStatus.clear();
                    } else if (navTrigger & VPAD_BUTTON_A) {
                        mapCapture = 1;
                        mapCaptureUntil = SDL_GetTicks() / 1000.0 + 5.0;
                        mapCaptureArmed = false;
                        navBlip();
                    } else if (navTrigger & VPAD_BUTTON_Y) {
                        mapCapture = 2;
                        mapCaptureUntil = SDL_GetTicks() / 1000.0 + 5.0;
                        mapCaptureArmed = false;
                        navBlip();
                    } else if (navTrigger & VPAD_BUTTON_X) {
                        m.sources[acts[mapRow]] = 0;
                        persistControls();
                        applyControlMaps();
                        navBlip();
                    } else if (navTrigger & VPAD_BUTTON_MINUS) {
                        m = mapDevice == 0
                            ? defaultActionMap(kVpadButtons, kVpadBtnCount)
                            : defaultActionMap(kProButtons, kProBtnCount);
                        persistControls();
                        applyControlMaps();
                        navBlip();
                    }
                }
            } else if (orderMode) {
                bool anyA = vpadError == VPAD_READ_SUCCESS && (vpad.hold & VPAD_BUTTON_A);
                for (const WpadSnapshot &s : wpadStates) {
                    if (!s.connected) continue;
                    const uint32_t a = (s.type == WPAD_TYPE_PRO || s.type == WPAD_TYPE_CLASSIC)
                        ? static_cast<uint32_t>(WPAD_PRO_BUTTON_A)
                        : static_cast<uint32_t>(WPAD_BUTTON_A);
                    anyA |= (s.buttons & a) != 0;
                }
                if (navTrigger & VPAD_BUTTON_B) {
                    player1Device = orderOriginal1;
                    player2Device = orderOriginal2;
                    refreshControllers();
                    orderMode = false;
                } else if (!orderArmed) {
                    if (!anyA) orderArmed = true;
                } else if (anyA) {
                    int selected = -1;
                    if (vpad.trigger & VPAD_BUTTON_A) selected = orderPort == 0 ? 0 : 1;
                    if (selected < 0) {
                        for (int i = 0; i < static_cast<int>(proChannels.size()); ++i) {
                            const WpadSnapshot &s = wpadStates[proChannels[i]];
                            const uint32_t a = (s.type == WPAD_TYPE_PRO || s.type == WPAD_TYPE_CLASSIC)
                                ? static_cast<uint32_t>(WPAD_PRO_BUTTON_A)
                                : static_cast<uint32_t>(WPAD_BUTTON_A);
                            if (s.buttons & a) {
                                selected = i + (orderPort == 0 ? 1 : 2);
                                break;
                            }
                        }
                    }
                    if (selected >= 0) {
                        if (orderPort == 0) player1Device = selected;
                        else player2Device = selected;
                        ++orderPort;
                        orderArmed = false;
                        if (orderPort >= (hasSecondCorePort() ? 2 : 1)) {
                            refreshControllers();
                            persistControls();
                            bootLog("controller_order_saved");
                            orderMode = false;
                        }
                    }
                }
            } else if (settingsOpen) {
                const int sectionCount = 5;
                if (settingsSection == -1) {
                    if (navTrigger & VPAD_BUTTON_B) {
                        settingsOpen = false; // NAV-5: Ajustes -> biblioteca
                    } else if (navTrigger & VPAD_BUTTON_UP) {
                        sectionCursor = (sectionCursor + sectionCount - 1) % sectionCount;
                    } else if (navTrigger & VPAD_BUTTON_DOWN) {
                        sectionCursor = (sectionCursor + 1) % sectionCount;
                    } else if (navTrigger & VPAD_BUTTON_A) {
                        settingsSection = sectionCursor;
                        settingsItem = 0;
                    }
                } else {
                    int itemCount = 1;
                    if (settingsSection == 0) itemCount = 5;
                    else if (settingsSection == 1) itemCount = 5;
                    else if (settingsSection == 2) itemCount = hasSecondCorePort() ? 7 : 6;
                    else if (settingsSection == 3) itemCount = 3;
                    else if (settingsSection == 4) itemCount = 2;

                    if (navTrigger & VPAD_BUTTON_B) {
                        settingsSection = -1; // NAV-5: sección -> raíz de Ajustes
                    } else if (navTrigger & VPAD_BUTTON_UP) {
                        settingsItem = (settingsItem + itemCount - 1) % itemCount;
                    } else if (navTrigger & VPAD_BUTTON_DOWN) {
                        settingsItem = (settingsItem + 1) % itemCount;
                    } else if (navTrigger & VPAD_BUTTON_A) {
                        if (settingsSection == 0 && settingsItem == 3) {
                            dynamicBg = !dynamicBg;
                            saveViewOptions();
                        } else if (settingsSection == 2 && settingsItem == (hasSecondCorePort() ? 2 : 1)) {
                            orderMode = true;
                            orderPort = 0;
                            orderArmed = false;
                            orderOriginal1 = player1Device;
                            orderOriginal2 = player2Device;
                        } else if (settingsSection == 2 && settingsItem == (hasSecondCorePort() ? 3 : 2)) {
                            // CTRL-3: editor de mapeo estilo Our64.
                            mapMode = true;
                            profileMode = false;
                            profileStatus.clear();
                            mapRow = 0;
                            mapTop = 0;
                            mapDevice = 0;
                            mapCapture = 0;
                        } else if (settingsSection == 2 && settingsItem >= (hasSecondCorePort() ? 4 : 3)) {
                            const int family = settingsItem - (hasSecondCorePort() ? 4 : 3);
                            menuShortcuts.choice[family] = (menuShortcuts.choice[family] + 1) % 3;
                            const bool saved = saveMenuShortcuts(library.sd_root, menuShortcuts);
                            const std::string event = std::string(saved ? "menu_shortcut_saved" :
                                "menu_shortcut_save_failed") + " family=" + std::to_string(family) +
                                " choice=" + std::to_string(menuShortcuts.choice[family]);
                            bootLog(event.c_str());
                        } else if (settingsSection == 1 && (settingsItem == 3 || settingsItem == 4)) {
                            std::vector<std::string> titles;
                            for (const auto &rom : library.roms) titles.push_back(rom.title);
                            startAssetDownload(library.sd_root, RETROU_CORE_SYSTEM, titles,
                                               settingsItem == 3, settingsItem == 4,
                                               downloadAllMode);
                            metaApplied = false;
                        }
                    } else if ((navTrigger & VPAD_BUTTON_LEFT) || (navTrigger & VPAD_BUTTON_RIGHT)) {
                        const int dir = (navTrigger & VPAD_BUTTON_RIGHT) ? 1 : -1;
                        if (settingsSection == 0 && settingsItem == 0) {
                            themeIndex = (themeIndex + dir + 5) % 5;
                            saveThemeIndex(library.sd_root, themeIndex);
                        } else if (settingsSection == 0 && settingsItem == 1) {
                            columns = std::max(3, std::min(6, columns + dir));
                            saveAppSettings(library.sd_root, columns, showCovers, uiVolume, bgMusicOn, sfxOn, coreVolume);
                        } else if (settingsSection == 0 && settingsItem == 2) {
                            viewMode = (viewMode + dir + 3) % 3;
                            alphaFilter = viewMode != 0 && favoritesOnly ? 1 : 0;
                            focused = 0;
                            scroll = 0;
                            rebuildVisible();
                            saveViewOptions();
                        } else if (settingsSection == 0 && settingsItem == 3) {
                            dynamicBg = !dynamicBg;
                            saveViewOptions();
                        } else if (settingsSection == 0 && settingsItem == 4) {
                            backdropDelayIndex = (backdropDelayIndex + dir + 5) % 5;
                            saveViewOptions();
                        } else if (settingsSection == 1 && settingsItem == 0) {
                            showCovers = !showCovers;
                            saveAppSettings(library.sd_root, columns, showCovers, uiVolume, bgMusicOn, sfxOn, coreVolume);
                        } else if (settingsSection == 1 && settingsItem == 1) {
                            autoDownload = !autoDownload;
                            saveAutoDownload(library.sd_root, autoDownload);
                        } else if (settingsSection == 1 && settingsItem == 2) {
                            downloadAllMode = !downloadAllMode;
                        } else if (settingsSection == 3 && settingsItem == 0) {
                            uiVolume = std::max(0, std::min(100, uiVolume + dir * 10));
                            g_uiVolume = uiVolume;
                            saveAppSettings(library.sd_root, columns, showCovers, uiVolume, bgMusicOn, sfxOn, coreVolume);
                        } else if (settingsSection == 3 && settingsItem == 1) {
                            coreVolume = std::max(0, std::min(100, coreVolume + dir * 10));
                            core.setAudioOptions(coreVolume, 0);
                            saveAppSettings(library.sd_root, columns, showCovers, uiVolume, bgMusicOn, sfxOn, coreVolume);
                        } else if (settingsSection == 3 && settingsItem == 2) {
                            sfxOn = !sfxOn;
                            g_sfxOn = sfxOn;
                            saveAppSettings(library.sd_root, columns, showCovers, uiVolume, bgMusicOn, sfxOn, coreVolume);
                        } else if (settingsSection == 4 && settingsItem == 0) {
                            videoAspect = (videoAspect + dir + 3) % 3;
                            saveVideoOptions(library.sd_root, videoAspect, videoScale, videoFilter);
                            core.setVideoOptions(videoAspect, videoScale, videoFilter);
                        } else if (settingsSection == 4 && settingsItem == 1) {
                            videoScale = (videoScale + dir + 3) % 3;
                            saveVideoOptions(library.sd_root, videoAspect, videoScale, videoFilter);
                            core.setVideoOptions(videoAspect, videoScale, videoFilter);
                        } else if (settingsSection == 2 && settingsItem == 0) {
                            const int n = static_cast<int>(devOpts.size());
                            if (n > 0) player1Device = (player1Device + dir + n) % n;
                            persistControls();
                            refreshControllers();
                        } else if (settingsSection == 2 && settingsItem == 1 && hasSecondCorePort()) {
                            const int n = static_cast<int>(p2Opts.size());
                            if (n > 0) player2Device = (player2Device + dir + n) % n;
                            persistControls();
                            refreshControllers();
                        } else if (settingsSection == 2 && settingsItem >= (hasSecondCorePort() ? 4 : 3)) {
                            const int family = settingsItem - (hasSecondCorePort() ? 4 : 3);
                            menuShortcuts.choice[family] = (menuShortcuts.choice[family] + dir + 3) % 3;
                            const bool saved = saveMenuShortcuts(library.sd_root, menuShortcuts);
                            const std::string event = std::string(saved ? "menu_shortcut_saved" :
                                "menu_shortcut_save_failed") + " family=" + std::to_string(family) +
                                " choice=" + std::to_string(menuShortcuts.choice[family]);
                            bootLog(event.c_str());
                        }
                    }
                }
            } else if (searchActive) {
                if (navTrigger & VPAD_BUTTON_B) {
                    if (!searchQuery.empty()) searchQuery.pop_back();
                    else searchActive = false;
                    rebuildVisible();
                } else if (navTrigger & VPAD_BUTTON_X) {
                    searchActive = false;
                    rebuildVisible();
                } else if (navTrigger & VPAD_BUTTON_L) {
                    searchActive = false;
                    searchQuery.clear();
                    rebuildVisible();
                } else if (navTrigger & VPAD_BUTTON_LEFT) {
                    searchCursor = (searchCursor + 38) % 39;
                } else if (navTrigger & VPAD_BUTTON_RIGHT) {
                    searchCursor = (searchCursor + 1) % 39;
                } else if (navTrigger & VPAD_BUTTON_UP) {
                    searchCursor = (searchCursor + 33) % 39;
                } else if (navTrigger & VPAD_BUTTON_DOWN) {
                    searchCursor = (searchCursor + 6) % 39;
                } else if (navTrigger & VPAD_BUTTON_A) {
                    if (searchCursor < 36) {
                        searchQuery += kbChars[searchCursor];
                    } else if (searchCursor == 36) {
                        searchQuery += ' ';
                    } else if (searchCursor == 37) {
                        if (!searchQuery.empty()) searchQuery.pop_back();
                    } else {
                        searchActive = false;
                    }
                    rebuildVisible();
                }
            } else if (sidebarOpen) {
                if (navTrigger & VPAD_BUTTON_B) {
                    sidebarOpen = false;
                } else if (navTrigger & VPAD_BUTTON_DOWN) {
                    sidebarIndex = (sidebarIndex + 1) % 4;
                    navBlip();
                } else if (navTrigger & VPAD_BUTTON_UP) {
                    sidebarIndex = (sidebarIndex + 3) % 4;
                    navBlip();
                } else if (navTrigger & VPAD_BUTTON_A) {
                    if (sidebarIndex == 0) {
                        searchActive = true;
                        searchCursor = 0;
                    } else if (sidebarIndex == 1) {
                        sortMode = (sortMode + 1) % 5;
                        rebuildVisible();
                        navBlip();
                    } else if (sidebarIndex == 2) {
                        regionFilter = (regionFilter + 1) % 4;
                        rebuildVisible();
                    } else if (sidebarIndex == 3) {
                        favoritesOnly = !favoritesOnly;
                        rebuildVisible();
                    }
                }
            } else {
                const int rc = static_cast<int>(visible.size());
                if (navTrigger & VPAD_BUTTON_PLUS) {
                    settingsOpen = true; // NAV-4: Plus abre Ajustes
                } else if (viewMode == 0 && (navTrigger & VPAD_BUTTON_MINUS)) {
                    // NAV-3: Minus abre el sidebar, solo en la vista cards
                    // (lista/carrusel no tienen sidebar).
                    sidebarOpen = true;
                    sidebarIndex = 0;
                } else if (viewMode != 0 && (navTrigger & VPAD_BUTTON_MINUS)) {
                    searchActive = true;
                    searchCursor = 0;
                } else if (viewMode != 0 && (navTrigger & VPAD_BUTTON_Y)) {
                    alphaFilter = favoritesOnly ? 0 : 1;
                    favoritesOnly = alphaFilter == 1;
                    rebuildVisible();
                } else if (viewMode != 0 && (navTrigger & (VPAD_BUTTON_L | VPAD_BUTTON_R))) {
                    alphaFilter = (alphaFilter + ((navTrigger & VPAD_BUTTON_R) ? 1 : 28)) % 29;
                    favoritesOnly = alphaFilter == 1;
                    rebuildVisible();
                } else if (rc > 0) {
                    if (navTrigger & VPAD_BUTTON_A) {
                        if (core.loadGame(library.roms[visible[focused]].path, error)) {
                            activeRom = visible[focused];
                            player1Device = globalPlayer1;
                            player2Device = globalPlayer2;
                            vpadMap = globalVpadMap;
                            proMap = globalProMap;
                            if (loadControlOptions(library.sd_root, player1Device, player2Device,
                                                   vpadMap, proMap,
                                                   romControlPath(library.sd_root,
                                                                  library.roms[activeRom].path)))
                                bootLog("rom_controls_loaded");
                            else {
                                player1Device = globalPlayer1;
                                player2Device = globalPlayer2;
                                vpadMap = globalVpadMap;
                                proMap = globalProMap;
                                bootLog("global_controls_used_for_rom");
                            }
                            refreshControllers();
                            inGame = true;
                            slotMode = 0;
                        }
                    } else if (navTrigger & VPAD_BUTTON_X) {
                        RomEntry &rom = library.roms[visible[focused]];
                        rom.favorite = !rom.favorite;
                        saveFavorites(library, RETROU_CORE_SYSTEM);
                        if (favoritesOnly || sortMode == 4) rebuildVisible();
                    } else if (navTrigger & VPAD_BUTTON_RIGHT) {
                        focused = (focused + 1) % rc;
                        navBlip();
                    } else if (navTrigger & VPAD_BUTTON_LEFT) {
                        focused = (focused + rc - 1) % rc;
                        navBlip();
                    } else if (navTrigger & VPAD_BUTTON_DOWN) {
                        focused = std::min(rc - 1, focused + (viewMode == 0 ? columns : 1));
                        navBlip();
                    } else if (navTrigger & VPAD_BUTTON_UP) {
                        focused = std::max(0, focused - (viewMode == 0 ? columns : 1));
                        navBlip();
                    }
                    if (viewMode == 0) {
                        const int focusedRow = focused / columns;
                        int scrollRow = focusedRow - (rows - 1);
                        if (scrollRow < 0) scrollRow = 0;
                        scroll = scrollRow * columns;
                    } else if (viewMode == 1) {
                        if (focused < scroll) scroll = focused;
                        if (focused >= scroll + 5) scroll = focused - 4;
                    }
                }
            }
        }

        bool wpadReleased = true;
        for (const WpadSnapshot &s : wpadStates) wpadReleased &= !s.connected || s.buttons == 0;
        if (waitingRelease && !coreMenu &&
            (vpadError != VPAD_READ_SUCCESS || vpad.hold == 0) && wpadReleased) {
            core.setInputEnabled(true);
            waitingRelease = false;
        }

        if (inGame && !coreMenu) {
            core.runFrame();
        }

        uiAudioTick();
        const int drawRom = !visible.empty() && focused >= 0 &&
            focused < static_cast<int>(visible.size()) ? visible[focused] : -1;
        if (std::string(RETROU_CORE_SYSTEM) == "gba" && viewMode == 0 &&
            !inGame && drawRom >= 0 && drawRom != lastLoggedGbaRom) {
            const std::string &title = library.roms[drawRom].title;
            const char letter = title.empty() ? '#' :
                static_cast<char>(std::toupper(static_cast<unsigned char>(title.front())));
            MEMHeapHandle heap = MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2);
            const uint32_t freeBytes = heap ? MEMGetTotalFreeSizeForExpHeap(heap) : 0;
            const std::string event = "gba_cards_focus letter=" + std::string(1, letter) +
                " index=" + std::to_string(focused) +
                " cached=" + std::to_string(coverCache.size()) +
                " mem2_free=" + std::to_string(freeBytes) +
                " title=" + title;
            bootLog(event.c_str());
            lastLoggedGbaRom = drawRom;
        }
        beginFrame(bg);
        drawGradient(0.0f, 0.0f, 1280.0f, 720.0f, bgTop, bg);

        if (dynamicBg && !inGame && !mapMode && !settingsOpen && !sidebarOpen && !searchActive &&
            !visible.empty() && SDL_GetTicks() - focusedTick >=
                static_cast<uint32_t>(backdropDelaysMs[backdropDelayIndex])) {
            Texture bd = getBackdrop(visible[focused]);
            if (bd >= 0) {
                drawTexture(bd, 0.0f, 0.0f, 1280.0f, 720.0f);
                drawRect(0.0f, 0.0f, 1280.0f, 720.0f, Color{8, 11, 18, 170});
            }
        }

        if (inGame && !mapMode && !orderMode) {
            if (core.hasFrame()) {
                const bool rgb565 = core.pixelFormat() == RETRO_PIXEL_FORMAT_RGB565;
                frameTex = uploadFrame(frameTex, core.frameData(),
                                       static_cast<int>(core.frameWidth()),
                                       static_cast<int>(core.frameHeight()),
                                       static_cast<int>(core.framePitch()), rgb565);
                if (frameTex >= 0) {
                    const float srcW = static_cast<float>(core.frameWidth());
                    const float srcH = static_cast<float>(core.frameHeight());
                    float aspect = (srcH > 0.0f) ? srcW / srcH : 1.0f;
                    if (videoAspect == 1) aspect = 4.0f / 3.0f;
                    else if (videoAspect == 2) aspect = 16.0f / 9.0f;
                    const float screenAspect = 1280.0f / 720.0f;
                    float dw = 1280.0f, dh = 720.0f;
                    if (videoScale == 0) {
                        if (aspect > screenAspect) dh = 1280.0f / aspect;
                        else dw = 720.0f * aspect;
                    } else if (videoScale == 1) {
                        const int sx = static_cast<int>(1280.0f / srcW);
                        const int sy = static_cast<int>(720.0f / srcH);
                        const int scale = std::max(1, std::min(sx, sy));
                        dw = srcW * scale;
                        dh = srcH * scale;
                    }
                    drawTexture(frameTex, (1280.0f - dw) * 0.5f, (720.0f - dh) * 0.5f, dw, dh);
                }
            }
            if (coreMenu) {
                drawRect(0.0f, 0.0f, 1280.0f, 720.0f, Color{0, 0, 0, 170});
                if (slotMode != 0) {
                    drawRoundRect(290.0f, 110.0f, 700.0f, 500.0f, 18.0f, panel);
                    text::drawText(slotMode == 1 ? "Guardar estado" : "Cargar estado",
                                   330.0f, 138.0f, 26, white);
                    for (int i = 0; i < 4; ++i) {
                        const float y = 205.0f + i * 88.0f;
                        if (slotIndex == i) drawRoundRect(315.0f, y - 10.0f, 650.0f, 78.0f, 10.0f, Color{245, 217, 127, 40});
                        if (slotThumbs[i] >= 0) drawTexture(slotThumbs[i], 335.0f, y, 92.0f, 66.0f);
                        struct stat slotInfo{};
                        const bool occupied = stat(snapPath(i).c_str(), &slotInfo) == 0;
                        text::drawText(("Slot " + std::to_string(i + 1) +
                                        (occupied ? "" : "  · Vacío")).c_str(),
                                       455.0f, y + 18.0f, 20, slotIndex == i ? accent : muted);
                    }
                    text::drawText("A  Aceptar    B  Volver", 330.0f, 570.0f, 15, muted);
                } else {
                    drawRoundRect(390.0f, 170.0f, 500.0f, 380.0f, 18.0f, panel);
                    text::drawText("MENU", 430.0f, 200.0f, 26, white);
                    const char *items[5] = {"Continuar", "Guardar estado", "Cargar estado",
                                            "Controles", "Salir del juego"};
                    for (int i = 0; i < 5; ++i) {
                        text::drawText(items[i], 430.0f, 270.0f + i * 44.0f, 20,
                                       coreMenuIndex == i ? accent : muted);
                    }
                    text::drawText("A  Aceptar    B  Volver", 430.0f, 500.0f, 15, muted);
                }
            }
        } else if (mapMode) {
            // CTRL-3: editor de mapeo estilo Our64. Izquierda: acciones del
            // core (4 visibles, paginadas). Derecha: imagen del mando con el
            // resaltado verde de la acción seleccionada.
            const std::vector<int> acts = consoleActions();
            const int nRows = static_cast<int>(acts.size());
            constexpr int kMapShown = 4;
            const ActionSources &m = mapOfDevice(mapDevice);
            text::drawText("Mapear botones", 28.0f, 18.0f, 34, white);
            text::drawText((std::string("Dispositivo: ") +
                            (mapDevice == 0 ? "GamePad" : "Mando Pro / Classic") +
                            "    (L/R cambia)").c_str(), 28.0f, 62.0f, 16, muted);
            const float rowH = 64.0f;
            const float y0 = 110.0f;
            for (int i = 0; i < kMapShown && mapTop + i < nRows; ++i) {
                const int row = mapTop + i;
                const float y = y0 + i * rowH;
                const bool sel = row == mapRow;
                if (sel) drawRoundRect(40.0f, y - 8.0f, 900.0f, rowH, 10.0f,
                                       Color{245, 217, 127, 40});
                const int action = acts[row];
                text::drawText(retroName(action), 60.0f, y + 6.0f, 24, sel ? white : muted);
                text::drawText("=", 300.0f, y + 10.0f, 18, muted);
                // Valor: lista de fuentes físicas de la acción.
                uint32_t src = m.sources[action];
                std::string value;
                bool first = true;
                for (uint32_t bit = 1; bit; bit <<= 1) {
                    if (src & bit) {
                        if (!first) value += ", ";
                        value += maskLabel(bit, mapDevice);
                        first = false;
                    }
                }
                if (value.empty()) value = "Sin mapear";
                text::drawText(value.c_str(), 330.0f, y + 6.0f, 24,
                               value == "Sin mapear" ? Color{230, 180, 80, 255}
                                                     : (sel ? accent : muted));
            }
            // Imagen del mando con el resaltado de la acción seleccionada.
            const Texture pad = getPadTexture();
            if (pad >= 0) {
                const float padX = 930.0f, padY = 180.0f, padW = 320.0f;
                const float padH = padW / 1.5f; // imágenes 1536x1024
                drawTexture(pad, padX, padY, padW, padH);
                const PadSpot &sp = padSpots()[acts[mapRow]];
                if (sp.r > 0.0f) {
                    const float s = padW / 1536.0f;
                    const float cx = padX + sp.x * s;
                    const float cy = padY + sp.y * s;
                    const float r = sp.r * s;
                    drawRoundRect(cx - r - 4.0f, cy - r - 4.0f, (r + 4.0f) * 2.0f,
                                  (r + 4.0f) * 2.0f, r + 4.0f, Color{20, 60, 30, 170});
                    drawRoundRect(cx - r, cy - r, r * 2.0f, r * 2.0f, r,
                                  Color{64, 230, 96, 170});
                }
            }
            if (profileMode) {
                drawRect(0.0f, 0.0f, 1280.0f, 720.0f, Color{10, 12, 16, 210});
                drawRoundRect(250.0f, 110.0f, 780.0f, 470.0f, 18.0f, panel);
                text::drawText((std::string("Perfiles: ") +
                                (mapDevice == 0 ? "GamePad" : "Pro / Classic")).c_str(),
                               290.0f, 145.0f, 28, white);
                for (int i = 0; i < 3; ++i) {
                    const float y = 210.0f + i * 90.0f;
                    if (profileSlot == i) drawRoundRect(280.0f, y - 12.0f, 720.0f, 65.0f,
                                                        10.0f, Color{245, 217, 127, 40});
                    struct stat info{};
                    const bool exists = stat(controlProfilePath(library.sd_root, mapDevice, i).c_str(),
                                             &info) == 0;
                    text::drawText(("Perfil " + std::to_string(i + 1) +
                                    (exists ? " · Guardado" : " · Vacío")).c_str(),
                                   310.0f, y, 22, profileSlot == i ? white : muted);
                }
                text::drawText("A  CARGAR    Y  GUARDAR    B  VOLVER",
                               290.0f, 525.0f, 16, accent);
                if (!profileStatus.empty())
                    text::drawText(profileStatus.c_str(), 700.0f, 525.0f, 16, white);
            } else if (mapCapture) {
                // Diálogo de captura con cuenta atrás (5 s), como Our64.
                drawRect(0.0f, 0.0f, 1280.0f, 720.0f, Color{10, 12, 16, 170});
                drawRoundRect(340.0f, 272.0f, 600.0f, 176.0f, 14.0f,
                              Color{40, 44, 52, 255});
                const std::string prompt = mapCapture == 2
                    ? "Pulsa un botón para añadir a " + std::string(retroName(acts[mapRow]))
                    : "Pulsa un botón para " + std::string(retroName(acts[mapRow]));
                text::drawText(prompt.c_str(), 400.0f, 306.0f, 22, white);
                text::drawText("La captura admite GamePad o Pro/Classic según el dispositivo",
                               400.0f, 344.0f, 14, muted);
                const int left = std::max(0, static_cast<int>(mapCaptureUntil -
                                                              SDL_GetTicks() / 1000.0));
                text::drawText(std::to_string(left).c_str(), 620.0f, 376.0f, 40, accent);
            } else {
                text::drawText("A  CAMBIAR    Y  AÑADIR    X  DESASIGNAR    -  REINICIAR    "
                               "L/R  DISPOSITIVO    +  PERFILES    B  VOLVER", 28.0f, 688.0f, 14, muted);
            }
        } else if (orderMode) {
            text::drawText("Orden de mandos", 28.0f, 18.0f, 34, white);
            text::drawText(("Pulsa A en el mando para Mando " + std::to_string(orderPort + 1)).c_str(),
                           28.0f, 70.0f, 22, accent);
            const int count = hasSecondCorePort() ? 2 : 1;
            for (int i = 0; i < count; ++i) {
                const float x = 80.0f + i * 580.0f;
                drawRoundRect(x, 170.0f, 540.0f, 330.0f, 18.0f,
                              i == orderPort ? panel : cardPanel);
                text::drawText(("Mando " + std::to_string(i + 1)).c_str(),
                               x + 35.0f, 205.0f, 28, white);
                const int device = i == 0 ? player1Device : player2Device;
                const auto &labels = i == 0 ? devOpts : p2Opts;
                const std::string label = i < orderPort && device < static_cast<int>(labels.size())
                    ? labels[device] : "Pulsa A";
                text::drawText(label.c_str(), x + 35.0f, 335.0f, 22,
                               i == orderPort ? accent : muted);
            }
            text::drawText("B  CANCELAR", 28.0f, 672.0f, 16, muted);
        } else if (settingsOpen) {
            const char *sections[] = {"PERSONALIZAR", "ASSETS", "CONTROLES", "SONIDO", "VÍDEO"};
            const char *sectionHelp[] = {"Vistas, tema y fondo", "Portadas y datos", "Mandos y botones", "Volumen y efectos", "Imagen del juego"};
            const char *themeOpts[] = {"Gris", "Azul", "Verde", "Dorado", "Rosa"};
            const char *aspectOpts[] = {"Original", "4:3", "16:9"};
            const char *scaleOpts[] = {"Ajustar", "Entero", "Estirar"};
            drawRoundRect(24.0f, 88.0f, 800.0f, 560.0f, 18.0f, panel);
            drawRoundRect(840.0f, 88.0f, 416.0f, 560.0f, 18.0f, cardPanel);
            const int previewSection = settingsSection < 0 ? sectionCursor : settingsSection;
            text::drawText(sections[previewSection], 870.0f, 126.0f, 24, white);
            text::drawText(sectionHelp[previewSection], 870.0f, 178.0f, 16, muted);
            text::drawText(settingsSection < 0 ? "A  Abrir sección" : "IZQ/DER  Cambiar valor",
                           870.0f, 570.0f, 15, accent);

            if (settingsSection == -1) {
                text::drawText("Ajustes", 28.0f, 18.0f, 34, white);
                for (int i = 0; i < 5; ++i) {
                    const float y = 120.0f + i * 80.0f;
                    const bool sel = i == sectionCursor;
                    if (sel) {
                        drawRoundRect(50.0f, y - 8.0f, 520.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                    }
                    text::drawText(sections[i], 60.0f, y, 24, sel ? white : muted);
                }
                text::drawText("A  ABRIR    B  BIBLIOTECA", 28.0f, 672.0f, 15, muted);
            } else {
                text::drawText(sections[settingsSection], 28.0f, 18.0f, 34, white);
                if (settingsSection == 0) {
                    const char *views[] = {"Cards", "Lista", "Carrusel"};
                    const std::string options[5] = {
                        "Tema: " + std::string(themeOpts[themeIndex]),
                        "Columnas: " + std::to_string(columns),
                        "Vista: " + std::string(views[viewMode]),
                        "Fondo dinámico: " + std::string(dynamicBg ? "Sí" : "No"),
                        "Aparición del fondo: " + std::to_string(backdropDelaysMs[backdropDelayIndex] / 1000) + " s"
                    };
                    for (int i = 0; i < 5; ++i) {
                        const float y = 120.0f + i * 70.0f;
                        if (settingsItem == i) drawRoundRect(50.0f, y - 8.0f, 740.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                        text::drawText(options[i].c_str(), 60.0f, y, 22, settingsItem == i ? white : muted);
                        if (i == 3) {
                            drawRoundRect(740.0f, y - 3.0f, 28.0f, 28.0f, 5.0f, dynamicBg ? accent : muted);
                            if (dynamicBg) text::drawText("✓", 746.0f, y - 4.0f, 22, bg);
                        }
                    }
                } else if (settingsSection == 1) {
                    const char *labels[5] = {"Carátulas", "Auto-descarga", "Modo", "Descargar portadas (A)", "Descargar backdrops (A)"};
                    std::string vals[5] = {showCovers ? "Sí" : "No", autoDownload ? "Sí" : "No",
                                           downloadAllMode ? "Descargar todo" : "Solo faltantes", "", ""};
                    for (int i = 0; i < 5; ++i) {
                        const float y = 120.0f + i * 70.0f;
                        if (settingsItem == i) drawRoundRect(50.0f, y - 8.0f, 760.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                        std::string line = labels[i];
                        if (!vals[i].empty()) line += ": " + vals[i];
                        text::drawText(line.c_str(), 60.0f, y, 22, settingsItem == i ? white : muted);
                    }
                    if (g_downloading || (g_downloadDone && g_downloadTotal.load() > 0)) {
                        const int done = g_downloadProgress.load();
                        const int total = g_downloadTotal.load();
                        if (total > 0) {
                            drawRoundRect(60.0f, 495.0f, 600.0f, 16.0f, 8.0f, cardPanel);
                            drawRoundRect(60.0f, 495.0f, 600.0f * done / total, 16.0f, 8.0f, accent);
                            const std::string status = std::string(g_downloading ? "Procesadas " :
                                "Última tanda: ") + std::to_string(done) + "/" +
                                std::to_string(total) + "  Fallos: " +
                                std::to_string(g_downloadFailures.load()) +
                                (g_downloadQueued.load() > 0 ? "  En cola: " +
                                 std::to_string(g_downloadQueued.load()) : "");
                            text::drawText(status.c_str(), 60.0f, 460.0f, 20, accent);
                        } else {
                            text::drawText(g_downloadQueued.load() > 0 ?
                                               "Actualizando datos; descarga en cola" :
                                               "Actualizando datos; no faltan imágenes", 60.0f,
                                           460.0f, 20, accent);
                        }
                    }
                } else if (settingsSection == 2) {
                    const float y0 = 120.0f, y1 = 190.0f, y2 = 260.0f;
                    if (settingsItem == 0) drawRoundRect(50.0f, y0 - 8.0f, 700.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                    text::drawText(("Mando 1: " + devOpts[player1Device]).c_str(),
                                   60.0f, y0, 22, settingsItem == 0 ? white : muted);
                    if (hasSecondCorePort()) {
                        if (settingsItem == 1) drawRoundRect(50.0f, y1 - 8.0f, 700.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                        text::drawText(("Mando 2: " + p2Opts[player2Device]).c_str(),
                                       60.0f, y1, 22, settingsItem == 1 ? white : muted);
                    }
                    const float orderY = hasSecondCorePort() ? y2 : y1;
                    const int orderItem = hasSecondCorePort() ? 2 : 1;
                    if (settingsItem == orderItem) drawRoundRect(50.0f, orderY - 8.0f, 700.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                    text::drawText("Cambiar orden de mandos  (A)", 60.0f, orderY, 22,
                                   settingsItem == orderItem ? white : muted);
                    const float mapY = orderY + 70.0f;
                    const int mapItem = orderItem + 1;
                    if (settingsItem == mapItem) drawRoundRect(50.0f, mapY - 8.0f, 700.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                    text::drawText("Mapear botones  (A)", 60.0f, mapY, 22, settingsItem == mapItem ? white : muted);
                    const char *deviceLabels[3] = {"GamePad", "Pro / Classic", "Wii Remote / Nunchuk"};
                    for (int family = 0; family < 3; ++family) {
                        const int item = mapItem + 1 + family;
                        const float y = mapY + 70.0f * (family + 1);
                        if (settingsItem == item)
                            drawRoundRect(50.0f, y - 8.0f, 760.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                        const std::string label = std::string("Menú ") + deviceLabels[family] +
                            ": " + kMenuShortcutLabels[family][menuShortcuts.choice[family]];
                        text::drawText(label.c_str(), 60.0f, y, 19,
                                       settingsItem == item ? white : muted);
                    }
                    text::drawText("Minus + Plus siempre abre el menú", 870.0f, 290.0f, 15, muted);
                } else if (settingsSection == 4) {
                    const char *labels[2] = {"Aspecto", "Escala"};
                    const char *vals[2] = {aspectOpts[videoAspect], scaleOpts[videoScale]};
                    for (int i = 0; i < 2; ++i) {
                        const float y = 120.0f + i * 70.0f;
                        if (settingsItem == i) {
                            drawRoundRect(50.0f, y - 8.0f, 700.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                        }
                        text::drawText((std::string(labels[i]) + ": " + vals[i]).c_str(),
                                       60.0f, y, 22, settingsItem == i ? white : muted);
                    }
                } else if (settingsSection == 3) {
                    const char *labels[3] = {"Sonido del menú", "Sonido del juego", "Efectos"};
                    std::string vals[3] = {std::to_string(uiVolume), std::to_string(coreVolume),
                                           sfxOn ? "Sí" : "No"};
                    for (int i = 0; i < 3; ++i) {
                        const float y = 120.0f + i * 70.0f;
                        if (settingsItem == i) drawRoundRect(50.0f, y - 8.0f, 700.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                        text::drawText((std::string(labels[i]) + ": " + vals[i]).c_str(), 60.0f, y, 22, settingsItem == i ? white : muted);
                    }
                } else {
                    text::drawText("Pendiente", 60.0f, 120.0f, 22, muted);
                }
                text::drawText("IZQ/DER  CAMBIAR    B  ATRÁS", 28.0f, 672.0f, 15, muted);
            }
        } else {
            text::drawText("Biblioteca", 28.0f, 18.0f, 34, white);
            if (!visible.empty()) {
                const std::string &full = library.roms[visible[focused]].title;
                if (getTitle(visible[focused]).size() < full.size()) {
                    text::drawText(full.c_str(), 400.0f, 26.0f, 20, muted);
                }
            }

            auto drawAlphaBar = [&]() {
                for (int item = 0; item < 29; ++item) {
                    const float x = 40.0f + item * 43.0f;
                    if (item == 1) {
                        drawStar(x + 8.0f, 91.0f, 9.0f, 4.0f,
                                 item == alphaFilter ? accent : muted);
                        continue;
                    }
                    char single[2] = {static_cast<char>('A' + item - 3), 0};
                    const char *label = item == 0 ? "All" : item == 2 ? "#" : single;
                    text::drawText(label, x, 82.0f, 15,
                                   item == alphaFilter ? accent : muted);
                }
            };
            const int cells = columns * rows;
            if (visible.empty()) {
                if (viewMode != 0) drawAlphaBar();
                text::drawText("Sin ROMs", 100.0f, 340.0f, 24, white);
            } else if (viewMode == 0) {
                for (int i = 0; i < cells; ++i) {
                    const int idx = scroll + i;
                    if (idx >= static_cast<int>(visible.size())) break;
                    const int romIndex = visible[idx];
                    const int col = i % columns, row = i / columns;
                    const float x = gridX + col * (cardW + gap);
                    const float y = gridY + row * (cardH + gap);
                    const bool sel = idx == focused;
                    // UI-1 (corregido): bordes superiores rectos, base redondeada.
                    drawRoundRect(x - 3.0f, y - 3.0f, cardW + 6.0f, cardH + 6.0f, 15.0f,
                                  sel ? accent : Color{255, 255, 255, 44});
                    drawRect(x - 3.0f, y - 3.0f, cardW + 6.0f, 15.0f,
                             sel ? accent : Color{255, 255, 255, 44});
                    drawRoundRect(x, y, cardW, cardH, 12.0f, cardPanel);
                    drawRect(x, y, cardW, 12.0f, cardPanel);
                    if (showCovers) {
                        drawTextureCover(getCover(romIndex), x + 2.0f, y + 2.0f, cardW - 4.0f, coverH - 2.0f);
                    }
                    drawRoundRect(x + 2.0f, y + coverH, cardW - 4.0f, cardH - coverH - 2.0f, 10.0f, cardFooter);
                    text::drawText(getTitle(romIndex).c_str(),
                                   x + 12.0f, y + coverH + 12.0f, 15, white);
                    {
                        const std::string &title = library.roms[romIndex].title;
                        std::string meta;
                        auto dit = metaDev.find(title);
                        if (dit != metaDev.end()) meta = dit->second;
                        std::string yr;
                        auto yit = metaYear.find(title);
                        if (yit != metaYear.end()) yr = yit->second;
                        if (yr.empty()) yr = titleYear(title);
                        if (!yr.empty()) {
                            if (!meta.empty()) meta += "  ";
                            meta += yr;
                        }
                        if (meta.empty()) {
                            static const char *regionNames[] = {"", "USA", "EUR", "JAP"};
                            const int r = detectRegion(title);
                            if (r >= 1 && r <= 3) meta = regionNames[r];
                        }
                        text::drawText(meta.c_str(), x + 12.0f, y + coverH + 34.0f, 12, muted);
                    }
                    if (library.roms[romIndex].favorite) {
                        drawStar(x + cardW - 18.0f, y + 18.0f, 9.0f, 4.0f, gold);
                    }
                }
            } else if (viewMode == 1) {
                drawAlphaBar();
                drawRoundRect(42.0f, 130.0f, 425.0f, 470.0f, 16.0f, panel);
                const int selected = visible[focused];
                if (showCovers) drawTextureCover(getCover(selected), 75.0f, 160.0f, 355.0f, 380.0f);
                if (library.roms[selected].favorite)
                    drawStar(441.0f, 161.0f, 12.0f, 5.0f, gold);
                for (int i = 0; i < 5 && scroll + i < static_cast<int>(visible.size()); ++i) {
                    const int index = scroll + i;
                    const float y = 142.0f + i * 90.0f;
                    const bool selectedRow = index == focused;
                    drawRoundRect(500.0f, y, 730.0f, 78.0f, 12.0f,
                                  selectedRow ? Color{245, 217, 127, 60} : cardPanel);
                    const std::string &title = library.roms[visible[index]].title;
                    text::drawText(text::fitText(title.c_str(), 22, 670.0f).c_str(),
                                   522.0f, y + 12.0f, 22, selectedRow ? white : muted);
                    text::drawText(titleYear(title).c_str(), 522.0f, y + 46.0f, 14, muted);
                    if (library.roms[visible[index]].favorite)
                        drawStar(1195.0f, y + 25.0f, 12.0f, 5.0f, gold);
                }
            } else {
                drawAlphaBar();
                const int count = static_cast<int>(visible.size());
                const bool nesCarousel = std::string(RETROU_CORE_SYSTEM) == "nes";
                const bool snesCarousel = std::string(RETROU_CORE_SYSTEM) == "snes";
                const float sideX = 40.0f;
                const float sideY = snesCarousel ? 230.0f : 185.0f;
                const float sideW = snesCarousel ? 270.0f : 230.0f;
                const float sideH = snesCarousel ? 260.0f : 330.0f;
                const float rightX = snesCarousel ? 970.0f : 1010.0f;
                if (focused > 0 && showCovers) {
                    const Texture cover = getCover(visible[focused - 1]);
                    if (nesCarousel || snesCarousel) drawTextureContain(cover, sideX, sideY, sideW, sideH);
                    else drawTextureCover(cover, sideX, sideY, sideW, sideH);
                }
                if (focused + 1 < count && showCovers) {
                    const Texture cover = getCover(visible[focused + 1]);
                    if (nesCarousel || snesCarousel) drawTextureContain(cover, rightX, sideY, sideW, sideH);
                    else drawTextureCover(cover, rightX, sideY, sideW, sideH);
                }
                drawRoundRect(295.0f, 130.0f, 690.0f, 450.0f, 20.0f, panel);
                if (showCovers) {
                    const Texture cover = getCover(visible[focused]);
                    if (nesCarousel)
                        drawTextureContain(cover, 438.0f, 143.0f, 404.0f, 424.0f);
                    else if (snesCarousel)
                        drawTextureContain(cover, 308.0f, 143.0f, 664.0f, 424.0f);
                    else drawTextureCover(cover, 308.0f, 143.0f, 664.0f, 424.0f);
                }
                if (library.roms[visible[focused]].favorite)
                    drawStar(950.0f, 162.0f, 13.0f, 5.5f, gold);
                drawRoundRect(300.0f, 590.0f, 680.0f, 64.0f, 12.0f, cardPanel);
                text::drawText(text::fitText(library.roms[visible[focused]].title.c_str(), 24, 630.0f).c_str(),
                               325.0f, 605.0f, 24, white);
                text::drawText((std::to_string(focused + 1) + " / " + std::to_string(count) + " juegos").c_str(),
                               1020.0f, 610.0f, 17, muted);
            }
            if (sidebarOpen) {
                drawRoundRect(24.0f, 88.0f, 300.0f, 560.0f, 18.0f, panel);
                text::drawText("FILTRAR Y ORGANIZAR", 44.0f, 106.0f, 16, muted);

                const char *regionNames[] = {"Todas", "USA", "Europa", "Japón"};
                const char *sortNames[] = {"Nombre", "Región", "Año", "Desarrollador", "Favoritos"};
                std::string labels[4] = {
                    "Buscar",
                    std::string("Ordenar por: ") + sortNames[sortMode],
                    std::string("Región: ") + regionNames[regionFilter],
                    std::string("Favoritos: ") + (favoritesOnly ? "Sí" : "No"),
                };
                for (int i = 0; i < 4; ++i) {
                    const float y = 150.0f + i * 44.0f;
                    if (i == sidebarIndex) {
                        drawRoundRect(36.0f, y - 8.0f, 276.0f, 34.0f, 8.0f, Color{245, 217, 127, 40});
                    }
                    text::drawText(labels[i].c_str(), 50.0f, y, 15, (i == sidebarIndex) ? white : muted);
                }
            }
            if (sidebarOpen) {
                text::drawText("A  SELECCIONAR    B  CERRAR", 28.0f, 672.0f, 15, muted);
            } else {
                text::drawText(viewMode == 0
                                   ? "A  JUGAR    X  FAVORITO    -  FILTROS    +  AJUSTES"
                                   : "A  JUGAR    X  FAVORITO    Y  FAVORITOS    -  BUSCAR    +  AJUSTES",
                               28.0f, 672.0f, 15, muted);
            }

            if (searchActive) {
                drawRect(0.0f, 0.0f, 1280.0f, 720.0f, Color{0, 0, 0, 180});
                text::drawText(searchQuery.empty() ? "Buscar..." : searchQuery.c_str(),
                               200.0f, 60.0f, 24, white);
                const float kbX = 150.0f, kbY = 130.0f, keyW = 90.0f, keyH = 48.0f, gap = 10.0f;
                for (int i = 0; i < 39; ++i) {
                    const int row = i / 6, col = i % 6;
                    const float x = kbX + col * (keyW + gap);
                    const float y = kbY + row * (keyH + gap);
                    const bool sel = i == searchCursor;
                    drawRoundRect(x, y, keyW, keyH, 8.0f, sel ? accent : cardPanel);
                    char single[2] = {0, 0};
                    const char *lbl;
                    if (i < 36) { single[0] = kbChars[i]; lbl = single; }
                    else if (i == 36) lbl = "ESP";
                    else if (i == 37) lbl = "BORR";
                    else lbl = "OK";
                    text::drawText(lbl, x + 14.0f, y + 13.0f, 16, sel ? white : muted);
                }
                text::drawText("A  TECLA    B  BORRAR / SALIR    X  ACEPTAR    L  CANCELAR",
                               200.0f, 580.0f, 15, muted);
            }
        }

        endFrame();
        if (std::string(RETROU_CORE_SYSTEM) == "gba" && viewMode == 0 &&
            !inGame && drawRom >= 0 && drawRom != lastRenderedGbaRom) {
            const std::string event = "gba_cards_rendered index=" +
                std::to_string(focused) + " title=" + library.roms[drawRom].title;
            bootLog(event.c_str());
            lastRenderedGbaRom = drawRom;
        }
    }

    bootLog("main_exit_clean");
    ProcUIShutdown();
    return 0;
}
