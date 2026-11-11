#include "gx2/gx2_renderer.hpp"
#include "gx2/gx2_text.hpp"
#include "gx2/gx2_image.hpp"
#include "gx2/retrou_data.hpp"
#include "retrou_assets.hpp"
#include "rpl_core.hpp"
#include "wpad_input.hpp"

#include <SDL.h>
#include <atomic>
#include <coreinit/systeminfo.h>
#include <libretro.h>
#include <thread>
#include <vpad/input.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// GX2 frontend per console: library grid with covers/focus, favorites, and a
// sidebar. A launches, B returns, X toggles favorite, Plus opens the sidebar,
// Minus+Plus exits.

extern const unsigned char _binary_ui_ttf_start[];
extern const unsigned char _binary_ui_ttf_end[];
extern const unsigned char _binary_icon_png_start[];
extern const unsigned char _binary_icon_png_end[];

using namespace retrou;
using namespace retrou::gx2;

#ifndef RETROU_CORE_SYSTEM
#define RETROU_CORE_SYSTEM "nes"
#endif
#ifndef RETROU_CORE_LABEL
#define RETROU_CORE_LABEL "NES"
#endif

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

static std::array<uint32_t, 12> defaultVpadMap()
{
    return {
        VPAD_BUTTON_B, VPAD_BUTTON_Y, VPAD_BUTTON_MINUS, VPAD_BUTTON_PLUS,
        VPAD_BUTTON_UP, VPAD_BUTTON_DOWN, VPAD_BUTTON_LEFT, VPAD_BUTTON_RIGHT,
        VPAD_BUTTON_A, VPAD_BUTTON_X, VPAD_BUTTON_L, VPAD_BUTTON_R
    };
}

static std::vector<int> consoleActions()
{
    const std::string s = RETROU_CORE_SYSTEM;
    if (s == "nes") return {0, 8, 2, 3, 4, 5, 6, 7};
    if (s == "snes") return {0, 1, 8, 9, 10, 11, 2, 3, 4, 5, 6, 7};
    return {0, 8, 10, 11, 2, 3, 4, 5, 6, 7};
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

static const char *vpadName(uint32_t mask)
{
    switch (mask) {
        case VPAD_BUTTON_A: return "A";
        case VPAD_BUTTON_B: return "B";
        case VPAD_BUTTON_X: return "X";
        case VPAD_BUTTON_Y: return "Y";
        case VPAD_BUTTON_L: return "L";
        case VPAD_BUTTON_R: return "R";
        case VPAD_BUTTON_ZL: return "ZL";
        case VPAD_BUTTON_ZR: return "ZR";
        case VPAD_BUTTON_PLUS: return "+";
        case VPAD_BUTTON_MINUS: return "-";
        case VPAD_BUTTON_UP: return "D-Pad Arriba";
        case VPAD_BUTTON_DOWN: return "D-Pad Abajo";
        case VPAD_BUTTON_LEFT: return "D-Pad Izq";
        case VPAD_BUTTON_RIGHT: return "D-Pad Der";
        case VPAD_BUTTON_STICK_L: return "Stick L";
        case VPAD_BUTTON_STICK_R: return "Stick R";
    }
    return "-";
}

static void loadControlOptions(const std::string &sdRoot, int &player1, std::array<uint32_t, 12> &map)
{
    map = defaultVpadMap();
    std::ifstream in(sdRoot + "/retrou/config/" RETROU_CORE_SYSTEM "/control-map.conf");
    int p1 = 0;
    if (!(in >> p1)) {
        player1 = 0;
        return;
    }
    player1 = p1;
    for (int i = 0; i < 12; ++i) {
        unsigned m = 0;
        if (!(in >> m)) return;
        map[i] = m;
    }
}

static void saveControlOptions(const std::string &sdRoot, int player1, const std::array<uint32_t, 12> &map)
{
    std::ofstream out(sdRoot + "/retrou/config/" RETROU_CORE_SYSTEM "/control-map.conf", std::ios::trunc);
    if (!out) return;
    out << player1;
    for (int i = 0; i < 12; ++i) out << " " << map[i];
    out << "\n";
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

// ---- Asset download (covers/backdrops/metadata) ----
static std::atomic<bool> g_downloading{false};
static std::atomic<bool> g_downloadDone{false};

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
                               std::vector<std::string> titles)
{
    if (g_downloading.exchange(true)) {
        return;
    }
    g_downloadDone = false;
    std::thread([sdRoot, system, titles]() {
        retrou::assets::downloadAll(sdRoot, system, titles);
        g_downloading = false;
        g_downloadDone = true;
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
    OSEnableHomeButtonMenu(TRUE);
    SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER);
    initUiAudio();

    if (!init()) return 1;
    if (!text::loadFont(_binary_ui_ttf_start,
                        static_cast<size_t>(_binary_ui_ttf_end - _binary_ui_ttf_start))) {
        return 1;
    }

    Library library;
    library.sd_root = mountSd();
    scanRoms(library, RETROU_CORE_SYSTEM);
    loadFavorites(library, RETROU_CORE_SYSTEM);
    int themeIndex = loadThemeIndex(library.sd_root);
    int videoAspect = 0;
    int videoScale = 0;
    int videoFilter = 1;
    loadVideoOptions(library.sd_root, videoAspect, videoScale, videoFilter);
    int player1Device = 0;
    std::array<uint32_t, 12> vpadMap{};
    loadControlOptions(library.sd_root, player1Device, vpadMap);
    bool remapMode = false;
    int remapIndex = 0;
    bool remapListening = false;
    int columns = 5;
    bool showCovers = true;
    int uiVolume = 80;
    bool bgMusicOn = true;
    bool sfxOn = true;
    int coreVolume = 80;
    loadAppSettings(library.sd_root, columns, showCovers, uiVolume, bgMusicOn, sfxOn, coreVolume);
    g_uiVolume = uiVolume;
    g_bgMusicOn = bgMusicOn;
    g_sfxOn = sfxOn;
    bool autoDownload = loadAutoDownload(library.sd_root);

    {
        const std::string countPath =
            library.sd_root + "/retrou/config/" RETROU_CORE_SYSTEM "/rom-count.txt";
        int stored = -1;
        {
            std::ifstream in(countPath);
            in >> stored;
        }
        const int now = static_cast<int>(library.roms.size());
        if (autoDownload && stored != now) {
            std::vector<std::string> titles;
            for (const auto &rom : library.roms) titles.push_back(rom.title);
            startAssetDownload(library.sd_root, RETROU_CORE_SYSTEM, titles);
            std::ofstream out(countPath, std::ios::trunc);
            if (out) out << now << "\n";
        }
    }

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
    int sidebarIndex = 0;
    int focused = 0;
    int sortMode = 0;      // 0 name, 1 region
    int regionFilter = 0;  // 0 all, 1 USA, 2 Europe, 3 Japan

    auto rebuildVisible = [&]() {
        visible.clear();
        const std::string q = toLower(searchQuery);
        for (int i = 0; i < static_cast<int>(library.roms.size()); ++i) {
            if (favoritesOnly && !library.roms[i].favorite) continue;
            if (regionFilter != 0 && detectRegion(library.roms[i].title) != regionFilter) {
                continue;
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
    static Texture thumbTex = -2;
    if (thumbTex == -2) {
        thumbTex = image::loadFromMemory(
            _binary_icon_png_start,
            static_cast<size_t>(_binary_icon_png_end - _binary_icon_png_start));
        if (thumbTex < 0) {
            thumbTex = image::loadFromFile(
                (library.sd_root + "/retrou/thumb_" + RETROU_CORE_SYSTEM + ".png").c_str());
        }
    }
    auto getCover = [&](int romIndex) -> Texture {
        auto it = coverCache.find(romIndex);
        if (it != coverCache.end()) return it->second;
        std::string cp = coverPath(library.sd_root, RETROU_CORE_SYSTEM, library.roms[romIndex].title);
        Texture real = image::loadFromFile(cp.c_str());
        if (real < 0) {
            if (thumbTex >= 0) {
                real = thumbTex;
            } else {
                const uint8_t r = static_cast<uint8_t>(30 + (romIndex * 37) % 160);
                const uint8_t g = static_cast<uint8_t>(60 + (romIndex * 29) % 140);
                const uint8_t b = static_cast<uint8_t>(120 - (romIndex * 23) % 100);
                real = makeCover(r, g, b, 10, 14, 24);
            }
        }
        coverCache[romIndex] = real;
        return real;
    };

    std::map<int, Texture> backdropCache;
    auto getBackdrop = [&](int romIndex) -> Texture {
        auto it = backdropCache.find(romIndex);
        if (it != backdropCache.end()) return it->second;
        std::string safe = library.roms[romIndex].title;
        for (char &c : safe) {
            if (std::string("&*/:`<>?\\|\"").find(c) != std::string::npos) c = '_';
        }
        const std::string path = library.sd_root + "/retrou/backdrops/" +
                                 RETROU_CORE_SYSTEM + "/" + safe + ".png";
        Texture bd = image::loadFromFileBlurred(path.c_str(), 64, 36);
        backdropCache[romIndex] = bd;
        return bd;
    };
    uint32_t focusedTick = SDL_GetTicks();
    int lastFocused = -1;
    int activeRom = -1;

    auto snapPath = [&]() -> std::string {
        std::string safe = activeRom >= 0 ? library.roms[activeRom].title : std::string("state");
        for (char &c : safe) {
            if (std::string("&*/:`<>?\\|\"").find(c) != std::string::npos) c = '_';
        }
        return library.sd_root + "/retrou/snap/" + RETROU_CORE_SYSTEM + "/" + safe + ".state";
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
    text::prewarm("Pro 1", 22);
    text::prewarm("Pro 2", 22);
    text::prewarm("Remapear botones", 34);
    text::prewarm("Remapear botones  (A)", 22);
    text::prewarm("Pulsa un boton...", 18);
    text::prewarm("A  Capturar    B  Volver", 15);
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
    text::prewarm("A  ABRIR    L/B  BIBLIOTECA", 15);
    text::prewarm("IZQ/DER  CAMBIAR    B  ATRÁS    L  BIBLIOTECA", 15);
    text::prewarm("Sin ROMs", 24);
    text::prewarm("A  JUGAR    B  VOLVER    X  FAVORITO    +  FILTROS", 15);
    text::prewarm("MENU", 26);
    text::prewarm("Continuar", 20);
    text::prewarm("Guardar estado", 20);
    text::prewarm("Cargar estado", 20);
    text::prewarm("Cerrar core", 20);
    text::prewarm(RETROU_CORE_LABEL, 12);

    RplCore core;
    std::string error;
    if (!core.loadStaticCore(nullptr, nullptr, error)) {
        return 1;
    }
    core.setSaveDirectory("/vol/external01/retrou/saves/" RETROU_CORE_SYSTEM "/save");
    core.setControllers(nullptr, nullptr);
    core.setVpadEnabled(true);
    core.setVideoOptions(videoAspect, videoScale, videoFilter);
    core.setExternalAudioDevice(g_uiDevice);
    core.setAudioOptions(coreVolume, 0);

    wpad_input_init();
    std::vector<int> proChannels = wpad_input_channels();
    const int pro0 = proChannels.size() > 0 ? proChannels[0] : -1;
    const int pro1 = proChannels.size() > 1 ? proChannels[1] : -1;
    std::vector<std::string> devOpts;
    devOpts.push_back("GamePad");
    for (size_t i = 0; i < proChannels.size() && i < 2; ++i) {
        devOpts.push_back("Pro " + std::to_string(i + 1));
    }
    if (player1Device >= static_cast<int>(devOpts.size())) player1Device = 0;
    auto applyControllers = [&]() {
        int p0 = -1, p1 = -1;
        if (player1Device == 0) { p0 = -1; p1 = pro0; }
        else if (player1Device == 1) { p0 = pro0; p1 = pro1; }
        else { p0 = pro1; p1 = pro0; }
        core.setWpadPorts(p0, p1);
        core.setVpadMap(vpadMap);
    };
    applyControllers();

    bool inGame = false;
    bool coreMenu = false;
    int coreMenuIndex = 0;
    bool waitingRelease = false;
    Texture frameTex = -1;

    const Color white{255, 255, 255, 255};
    const Color muted{148, 163, 184, 255};
    const Color panel{21, 27, 40, 235};
    const Color cardPanel{16, 21, 33, 235};
    const Color cardFooter{10, 14, 24, 235};
    const Color gold{245, 217, 127, 255};

    while (true) {
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
            coverCache.clear();
            metaApplied = true;
        }

        setNearestFilter(videoFilter == 0);

        if (focused != lastFocused) {
            lastFocused = focused;
            focusedTick = SDL_GetTicks();
        }

        VPADStatus vpad{};
        VPADReadError vpadError = VPAD_READ_NO_SAMPLES;
        VPADRead(VPAD_CHAN_0, &vpad, 1, &vpadError);
        if (vpadError == VPAD_READ_SUCCESS) {
            if (inGame) {
                if (coreMenu) {
                    if (vpad.trigger & VPAD_BUTTON_DOWN) {
                        coreMenuIndex = (coreMenuIndex + 1) % 4;
                    } else if (vpad.trigger & VPAD_BUTTON_UP) {
                        coreMenuIndex = (coreMenuIndex + 3) % 4;
                    } else if (vpad.trigger & VPAD_BUTTON_A) {
                        if (coreMenuIndex == 0) {
                            coreMenu = false;
                            core.setInputEnabled(false);
                            waitingRelease = true;
                        } else if (coreMenuIndex == 1) {
                            core.saveState(snapPath());
                        } else if (coreMenuIndex == 2) {
                            core.loadState(snapPath());
                        } else {
                            core.unloadGame();
                            inGame = false;
                            coreMenu = false;
                        }
                    } else if (vpad.trigger & VPAD_BUTTON_B) {
                        coreMenu = false;
                        core.setInputEnabled(false);
                        waitingRelease = true;
                    }
                } else {
                    if ((vpad.trigger & VPAD_BUTTON_MINUS) && (vpad.trigger & VPAD_BUTTON_PLUS)) {
                        coreMenu = true;
                        coreMenuIndex = 0;
                    }
                }
            } else if (remapMode) {
                const std::vector<int> acts = consoleActions();
                const int n = static_cast<int>(acts.size());
                if (remapListening) {
                    if (vpad.trigger != 0) {
                        vpadMap[acts[remapIndex]] = vpad.trigger;
                        remapListening = false;
                        saveControlOptions(library.sd_root, player1Device, vpadMap);
                        core.setVpadMap(vpadMap);
                        navBlip();
                    }
                } else if (vpad.trigger & VPAD_BUTTON_B) {
                    remapMode = false;
                } else if (vpad.trigger & VPAD_BUTTON_UP) {
                    remapIndex = (remapIndex + n - 1) % n;
                    navBlip();
                } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
                    remapIndex = (remapIndex + 1) % n;
                    navBlip();
                } else if (vpad.trigger & VPAD_BUTTON_A) {
                    remapListening = true;
                }
            } else if (settingsOpen) {
                const int sectionCount = 5;
                if (settingsSection == -1) {
                    if (vpad.trigger & VPAD_BUTTON_L) {
                        settingsOpen = false;
                    } else if (vpad.trigger & VPAD_BUTTON_UP) {
                        sectionCursor = (sectionCursor + sectionCount - 1) % sectionCount;
                    } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
                        sectionCursor = (sectionCursor + 1) % sectionCount;
                    } else if (vpad.trigger & VPAD_BUTTON_A) {
                        settingsSection = sectionCursor;
                        settingsItem = 0;
                    }
                } else {
                    int itemCount = 1;
                    if (settingsSection == 0) itemCount = 2;
                    else if (settingsSection == 1) itemCount = 3;
                    else if (settingsSection == 2) itemCount = 2;
                    else if (settingsSection == 3) itemCount = 3;
                    else if (settingsSection == 4) itemCount = 2;

                    if (vpad.trigger & VPAD_BUTTON_B) {
                        settingsSection = -1;
                    } else if (vpad.trigger & VPAD_BUTTON_L) {
                        settingsOpen = false;
                        settingsSection = -1;
                    } else if (vpad.trigger & VPAD_BUTTON_UP) {
                        settingsItem = (settingsItem + itemCount - 1) % itemCount;
                    } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
                        settingsItem = (settingsItem + 1) % itemCount;
                    } else if (vpad.trigger & VPAD_BUTTON_A) {
                        if (settingsSection == 2 && settingsItem == 1) {
                            remapMode = true;
                            remapIndex = 0;
                            remapListening = false;
                        } else if (settingsSection == 1 && settingsItem == 2) {
                            std::vector<std::string> titles;
                            for (const auto &rom : library.roms) titles.push_back(rom.title);
                            startAssetDownload(library.sd_root, RETROU_CORE_SYSTEM, titles);
                        }
                    } else if ((vpad.trigger & VPAD_BUTTON_LEFT) || (vpad.trigger & VPAD_BUTTON_RIGHT)) {
                        const int dir = (vpad.trigger & VPAD_BUTTON_RIGHT) ? 1 : -1;
                        if (settingsSection == 0 && settingsItem == 0) {
                            themeIndex = (themeIndex + dir + 5) % 5;
                            saveThemeIndex(library.sd_root, themeIndex);
                        } else if (settingsSection == 0 && settingsItem == 1) {
                            columns = std::max(3, std::min(6, columns + dir));
                            saveAppSettings(library.sd_root, columns, showCovers, uiVolume, bgMusicOn, sfxOn, coreVolume);
                        } else if (settingsSection == 1 && settingsItem == 0) {
                            showCovers = !showCovers;
                            saveAppSettings(library.sd_root, columns, showCovers, uiVolume, bgMusicOn, sfxOn, coreVolume);
                        } else if (settingsSection == 1 && settingsItem == 1) {
                            autoDownload = !autoDownload;
                            saveAutoDownload(library.sd_root, autoDownload);
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
                            saveControlOptions(library.sd_root, player1Device, vpadMap);
                            applyControllers();
                        }
                    }
                }
            } else if (searchActive) {
                if (vpad.trigger & VPAD_BUTTON_B) {
                    if (!searchQuery.empty()) searchQuery.pop_back();
                    rebuildVisible();
                } else if (vpad.trigger & VPAD_BUTTON_X) {
                    searchActive = false;
                    rebuildVisible();
                } else if (vpad.trigger & VPAD_BUTTON_L) {
                    searchActive = false;
                    searchQuery.clear();
                    rebuildVisible();
                } else if (vpad.trigger & VPAD_BUTTON_LEFT) {
                    searchCursor = (searchCursor + 38) % 39;
                } else if (vpad.trigger & VPAD_BUTTON_RIGHT) {
                    searchCursor = (searchCursor + 1) % 39;
                } else if (vpad.trigger & VPAD_BUTTON_UP) {
                    searchCursor = (searchCursor + 33) % 39;
                } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
                    searchCursor = (searchCursor + 6) % 39;
                } else if (vpad.trigger & VPAD_BUTTON_A) {
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
                if (vpad.trigger & VPAD_BUTTON_B) {
                    sidebarOpen = false;
                } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
                    sidebarIndex = (sidebarIndex + 1) % 4;
                    navBlip();
                } else if (vpad.trigger & VPAD_BUTTON_UP) {
                    sidebarIndex = (sidebarIndex + 3) % 4;
                    navBlip();
                } else if (vpad.trigger & VPAD_BUTTON_A) {
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
                if (vpad.trigger & VPAD_BUTTON_R) {
                    settingsOpen = true;
                } else if (vpad.trigger & VPAD_BUTTON_PLUS) {
                    sidebarOpen = true;
                    sidebarIndex = 0;
                } else if (vpad.trigger & VPAD_BUTTON_MINUS) {
                    sortMode = (sortMode + 1) % 5;
                    rebuildVisible();
                    navBlip();
                } else if (rc > 0) {
                    if (vpad.trigger & VPAD_BUTTON_A) {
                        if (core.loadGame(library.roms[visible[focused]].path, error)) {
                            activeRom = visible[focused];
                            inGame = true;
                        }
                    } else if (vpad.trigger & VPAD_BUTTON_X) {
                        RomEntry &rom = library.roms[visible[focused]];
                        rom.favorite = !rom.favorite;
                        saveFavorites(library, RETROU_CORE_SYSTEM);
                    } else if (vpad.trigger & VPAD_BUTTON_RIGHT) {
                        focused = (focused + 1) % rc;
                        navBlip();
                    } else if (vpad.trigger & VPAD_BUTTON_LEFT) {
                        focused = (focused + rc - 1) % rc;
                        navBlip();
                    } else if (vpad.trigger & VPAD_BUTTON_DOWN) {
                        focused = std::min(rc - 1, focused + columns);
                        navBlip();
                    } else if (vpad.trigger & VPAD_BUTTON_UP) {
                        focused = std::max(0, focused - columns);
                        navBlip();
                    }
                    const int focusedRow = focused / columns;
                    int scrollRow = focusedRow - (rows - 1);
                    if (scrollRow < 0) scrollRow = 0;
                    scroll = scrollRow * columns;
                }
            }
        }

        if (waitingRelease && !coreMenu && vpadError == VPAD_READ_SUCCESS && vpad.hold == 0) {
            core.setInputEnabled(true);
            waitingRelease = false;
        }

        if (inGame && !coreMenu) {
            core.runFrame();
        }

        uiAudioTick();
        beginFrame(bg);
        drawGradient(0.0f, 0.0f, 1280.0f, 720.0f, bgTop, bg);

        if (!inGame && !remapMode && !settingsOpen && !sidebarOpen && !searchActive &&
            !visible.empty() && SDL_GetTicks() - focusedTick >= 3000) {
            Texture bd = getBackdrop(visible[focused]);
            if (bd >= 0) {
                drawTexture(bd, 0.0f, 0.0f, 1280.0f, 720.0f);
                drawRect(0.0f, 0.0f, 1280.0f, 720.0f, Color{8, 11, 18, 170});
            }
        }

        if (inGame) {
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
                drawRoundRect(390.0f, 170.0f, 500.0f, 380.0f, 18.0f, panel);
                text::drawText("MENU", 430.0f, 200.0f, 26, white);
                const char *items[4] = {"Continuar", "Guardar estado", "Cargar estado",
                                        "Salir del juego"};
                for (int i = 0; i < 4; ++i) {
                    text::drawText(items[i], 430.0f, 270.0f + i * 44.0f, 20,
                                   coreMenuIndex == i ? accent : muted);
                }
                text::drawText("A  Aceptar    B  Volver", 430.0f, 500.0f, 15, muted);
            }
        } else if (remapMode) {
            text::drawText("Remapear botones", 28.0f, 18.0f, 34, white);
            const std::vector<int> acts = consoleActions();
            const int n = static_cast<int>(acts.size());
            for (int i = 0; i < n; ++i) {
                const float y = 90.0f + i * 44.0f;
                const bool sel = i == remapIndex;
                if (sel) drawRoundRect(40.0f, y - 8.0f, 760.0f, 36.0f, 8.0f, Color{245, 217, 127, 40});
                std::string line = std::string(retroName(acts[i])) + "  =  " +
                    (remapListening && sel ? "Pulsa un boton..." : vpadName(vpadMap[acts[i]]));
                text::drawText(line.c_str(), 50.0f, y, 18, sel ? white : muted);
            }
            text::drawText("A  Capturar    B  Volver", 28.0f, 672.0f, 15, muted);
        } else if (settingsOpen) {
            const char *sections[] = {"PERSONALIZAR", "ASSETS", "CONTROLES", "SONIDO", "VÍDEO"};
            const char *themeOpts[] = {"Gris", "Azul", "Verde", "Dorado", "Rosa"};
            const char *aspectOpts[] = {"Original", "4:3", "16:9"};
            const char *scaleOpts[] = {"Ajustar", "Entero", "Estirar"};

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
                text::drawText("A  ABRIR    L/B  BIBLIOTECA", 28.0f, 672.0f, 15, muted);
            } else {
                text::drawText(sections[settingsSection], 28.0f, 18.0f, 34, white);
                if (settingsSection == 0) {
                    const float y0 = 120.0f, y1 = 190.0f;
                    if (settingsItem == 0) drawRoundRect(50.0f, y0 - 8.0f, 520.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                    text::drawText(("Tema: " + std::string(themeOpts[themeIndex])).c_str(), 60.0f, y0, 22, settingsItem == 0 ? white : muted);
                    if (settingsItem == 1) drawRoundRect(50.0f, y1 - 8.0f, 520.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                    text::drawText(("Columnas: " + std::to_string(columns)).c_str(), 60.0f, y1, 22, settingsItem == 1 ? white : muted);
                } else if (settingsSection == 1) {
                    const char *labels[3] = {"Carátulas", "Auto-descarga", "Descargar ahora (A)"};
                    std::string vals[3] = {showCovers ? "Sí" : "No", autoDownload ? "Sí" : "No", ""};
                    for (int i = 0; i < 3; ++i) {
                        const float y = 120.0f + i * 70.0f;
                        if (settingsItem == i) drawRoundRect(50.0f, y - 8.0f, 760.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                        std::string line = labels[i];
                        if (!vals[i].empty()) line += ": " + vals[i];
                        text::drawText(line.c_str(), 60.0f, y, 22, settingsItem == i ? white : muted);
                    }
                    if (g_downloading) {
                        text::drawText("Descargando...", 60.0f, 400.0f, 20, accent);
                    }
                } else if (settingsSection == 2) {
                    const float y0 = 120.0f, y1 = 190.0f;
                    if (settingsItem == 0) drawRoundRect(50.0f, y0 - 8.0f, 700.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                    text::drawText(("Mando 1: " + devOpts[player1Device]).c_str(),
                                   60.0f, y0, 22, settingsItem == 0 ? white : muted);
                    if (settingsItem == 1) drawRoundRect(50.0f, y1 - 8.0f, 700.0f, 44.0f, 8.0f, Color{245, 217, 127, 40});
                    text::drawText("Remapear botones  (A)", 60.0f, y1, 22, settingsItem == 1 ? white : muted);
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
                text::drawText("IZQ/DER  CAMBIAR    B  ATRÁS    L  BIBLIOTECA", 28.0f, 672.0f, 15, muted);
            }
        } else {
            text::drawText("Biblioteca", 28.0f, 18.0f, 34, white);
            if (!visible.empty()) {
                const std::string &full = library.roms[visible[focused]].title;
                if (getTitle(visible[focused]).size() < full.size()) {
                    text::drawText(full.c_str(), 400.0f, 26.0f, 20, muted);
                }
            }

            const int cells = columns * rows;
            if (visible.empty()) {
                text::drawText("Sin ROMs", 100.0f, 340.0f, 24, white);
            } else {
                for (int i = 0; i < cells; ++i) {
                    const int idx = scroll + i;
                    if (idx >= static_cast<int>(visible.size())) break;
                    const int romIndex = visible[idx];
                    const int col = i % columns, row = i / columns;
                    const float x = gridX + col * (cardW + gap);
                    const float y = gridY + row * (cardH + gap);
                    const bool sel = idx == focused;
                    drawRoundRect(x - 3.0f, y - 3.0f, cardW + 6.0f, cardH + 6.0f, 15.0f,
                                  sel ? accent : Color{255, 255, 255, 44});
                    drawRoundRect(x, y, cardW, cardH, 12.0f, cardPanel);
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
                text::drawText("A  JUGAR    X  FAVORITO    +  FILTROS", 28.0f, 672.0f, 15, muted);
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
                text::drawText("A  TECLA    B  BORRAR    X  ACEPTAR    L  CANCELAR",
                               200.0f, 580.0f, 15, muted);
            }
        }

        endFrame();
    }

    return 0;
}
