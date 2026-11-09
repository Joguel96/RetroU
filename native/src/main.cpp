#include <SDL.h>
#include <SDL_ttf.h>

#include "rpl_core.hpp"
#include "hbl.hpp"
#include <whb/sdcard.h>
#include <sysapp/launch.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <string>
#include <vector>

extern "C" {
extern const unsigned char _binary_ui_ttf_start[];
extern const unsigned char _binary_ui_ttf_end[];
extern const unsigned char _binary_ui_bold_ttf_start[];
extern const unsigned char _binary_ui_bold_ttf_end[];
}

namespace {

struct ConsoleDef {
    const char *id;
    const char *name;
    const char *folder;
    uint32_t color;
    const char *core_hints;
};

const ConsoleDef kConsoles[] = {
    {"nes", "NES", "NES", 0xE05A4E, "fceumm|nestopia|mesen|quicknes"},
    {"snes", "SNES", "SNES", 0x8B7CF6, "snes9x|bsnes"},
    {"n64", "N64", "N64", 0x3FB950, "mupen64|our64|parallel"},
    {"genesis", "Genesis", "Genesis", 0x2F81F7, "genesis_plus_gx|picodrive"},
    {"gba", "GBA", "GBA", 0xA371F7, "mgba|vbam|gpsp"},
    {"ps1", "PS1", "PS1", 0xD29922, "pcsx|mednafen_psx|swanstation"},
    {"arcade", "Arcade", "Arcade", 0xE3B341, "fbneo|mame|fbalpha"},
    {"atari", "Atari", "Atari", 0xDB6D28, "stella|a5200|prosystem"},
    {"msx", "MSX", "MSX", 0x39C5CF, "bluemsx|fmsx"},
};

const int kConsoleCount = static_cast<int>(sizeof(kConsoles) / sizeof(kConsoles[0]));

struct RomEntry {
    std::string title;
    std::string path;
    std::string system;
    uint32_t color;
};

struct CoreEntry {
    std::string name;
    std::string path;
    std::string lower;
    bool rpl = false;
};

struct Library {
    std::string sd_root;
    std::vector<RomEntry> roms;
    std::vector<CoreEntry> cores;
};

struct AppState {
    Library library;
    int system = 0; /* 0 = all */
    int game = 0;
    int scroll_row = 0;
    int counts[kConsoleCount + 1] = {};
    std::vector<const RomEntry *> visible;
    bool running = true;
    bool pending_launch = false;
    bool pending_rpl = false;
    std::string launch_core;
    std::string launch_rom;
    std::string rpl_core;
    std::string rpl_rom;
    std::string message;
};

SDL_Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return SDL_Color{r, g, b, a};
}

std::string lower_copy(const std::string &value)
{
    std::string result = value;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool ends_with_nocase(const std::string &value, const char *suffix)
{
    std::string lower = lower_copy(value);
    std::string tail = lower_copy(suffix);
    return lower.size() >= tail.size() &&
           lower.compare(lower.size() - tail.size(), tail.size(), tail) == 0;
}

bool is_rom_extension(const std::string &name)
{
    static const char *extensions[] = {
        ".nes", ".fds", ".unf", ".sfc", ".smc", ".fig", ".n64", ".z64", ".v64",
        ".md", ".gen", ".smd", ".32x", ".gba", ".gb", ".gbc", ".cue", ".chd",
        ".iso", ".pbp", ".bin", ".zip", ".7z", ".atr", ".xex", ".rom", ".mx1",
        ".mx2", ".dsk", ".cas", ".col", ".int", ".vec", ".a26", ".lnx",
    };
    for (const char *extension : extensions) {
        if (ends_with_nocase(name, extension)) {
            return true;
        }
    }
    return false;
}

bool is_directory(const std::string &path)
{
    struct stat info;
    return stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR) != 0;
}

std::string join_path(const std::string &base, const std::string &name)
{
    if (base.empty()) {
        return name;
    }
    if (base.back() == '/' || base.back() == ':') {
        return base + name;
    }
    return base + "/" + name;
}

std::string strip_extension(const std::string &name)
{
    size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

const ConsoleDef *console_by_id(const std::string &id)
{
    for (const ConsoleDef &console : kConsoles) {
        if (id == console.id) {
            return &console;
        }
    }
    return nullptr;
}

const ConsoleDef *console_by_folder(const std::string &folder)
{
    for (const ConsoleDef &console : kConsoles) {
        std::string wanted = lower_copy(console.folder);
        std::string given = lower_copy(folder);
        if (wanted == given || given == console.id) {
            return &console;
        }
    }
    return nullptr;
}

void scan_roms(const std::string &rom_root, Library &library)
{
    DIR *root = opendir(rom_root.c_str());
    if (!root) {
        return;
    }

    struct dirent *folder_entry;
    while ((folder_entry = readdir(root)) != nullptr) {
        std::string folder = folder_entry->d_name;
        if (folder == "." || folder == ".." || folder.empty() || folder[0] == '.') {
            continue;
        }

        std::string folder_path = join_path(rom_root, folder);
        if (!is_directory(folder_path)) {
            continue;
        }

        const ConsoleDef *console = console_by_folder(folder);
        if (!console) {
            continue;
        }

        DIR *dir = opendir(folder_path.c_str());
        if (!dir) {
            continue;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name = entry->d_name;
            if (name == "." || name == ".." || name.empty() || name[0] == '.') {
                continue;
            }
            if (!is_rom_extension(name)) {
                continue;
            }

            RomEntry rom;
            rom.title = strip_extension(name);
            rom.path = join_path(folder_path, name);
            rom.system = console->id;
            rom.color = console->color;
            library.roms.push_back(rom);
        }
        closedir(dir);
    }
    closedir(root);

    std::sort(library.roms.begin(), library.roms.end(),
              [](const RomEntry &a, const RomEntry &b) {
                  if (a.system != b.system) {
                      return a.system < b.system;
                  }
                  return lower_copy(a.title) < lower_copy(b.title);
              });
}

void scan_cores(const std::string &cores_root, Library &library)
{
    DIR *dir = opendir(cores_root.c_str());
    if (!dir) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (!ends_with_nocase(name, ".rpx") && !ends_with_nocase(name, ".rpl")) {
            continue;
        }

        CoreEntry core;
        core.name = strip_extension(name);
        core.path = join_path(cores_root, name);
        core.lower = lower_copy(core.name);
        core.rpl = ends_with_nocase(name, ".rpl");
        library.cores.push_back(core);
    }
    closedir(dir);

    std::sort(library.cores.begin(), library.cores.end(),
              [](const CoreEntry &a, const CoreEntry &b) {
                  if (a.rpl != b.rpl) {
                      return a.rpl;
                  }
                  return a.lower < b.lower;
              });
}

std::string mount_sd()
{
    if (WHBMountSdCard()) {
        char *path = WHBGetSdCardMountPath();
        if (path && *path) {
            return path;
        }
    }

    if (DIR *dir = opendir("/vol/external01/retrou")) {
        closedir(dir);
        return "/vol/external01";
    }
    return "sd:";
}

Library load_library()
{
    Library library;
    library.sd_root = mount_sd();
    std::string retro_root = join_path(library.sd_root, "retrou");
    scan_roms(join_path(retro_root, "roms"), library);
    scan_cores(join_path(retro_root, "cores"), library);
    return library;
}

const CoreEntry *find_core(const Library &library, const std::string &system)
{
    const ConsoleDef *console = console_by_id(system);
    if (!console) {
        return library.cores.empty() ? nullptr : &library.cores.front();
    }

    std::string hints = console->core_hints;
    size_t start = 0;
    while (start <= hints.size()) {
        size_t end = hints.find('|', start);
        std::string hint = hints.substr(start, end == std::string::npos ? std::string::npos : end - start);
        for (const CoreEntry &core : library.cores) {
            if (core.lower.find(hint) != std::string::npos) {
                return &core;
            }
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    return nullptr;
}

int console_index_by_id(const std::string &id)
{
    for (int i = 0; i < kConsoleCount; ++i) {
        if (id == kConsoles[i].id) {
            return i + 1;
        }
    }
    return 0;
}

void refresh_visible(AppState &app)
{
    app.visible.clear();
    for (const RomEntry &rom : app.library.roms) {
        if (app.system == 0 || rom.system == kConsoles[app.system - 1].id) {
            app.visible.push_back(&rom);
        }
    }

    if (app.visible.empty()) {
        app.game = 0;
        app.scroll_row = 0;
        return;
    }
    app.game = std::max(0, std::min(app.game, static_cast<int>(app.visible.size()) - 1));
}

void rebuild_indexes(AppState &app)
{
    for (int &count : app.counts) {
        count = 0;
    }

    for (const RomEntry &rom : app.library.roms) {
        app.counts[0]++;
        int index = console_index_by_id(rom.system);
        if (index > 0) {
            app.counts[index]++;
        }
    }

    refresh_visible(app);
}

bool launch_core(const std::string &sd_root, const std::string &core_path,
                 const std::string &rom_path, std::string &error)
{
    std::string launch_dir = join_path(join_path(sd_root, "retrou"), "launch");
    mkdir(launch_dir.c_str(), 0777);

    std::string launch_file = join_path(launch_dir, "last.txt");
    std::ofstream out(launch_file, std::ios::binary | std::ios::trunc);
    if (!out) {
        error = "No se pudo escribir launch/last.txt";
        return false;
    }
    out << rom_path << "\n" << core_path << "\n";
    out.close();

    /* sysapp wants the IOSU path, not the stdio devoptab prefix. */
    std::string path = core_path;
    if (path.rfind("sd:/", 0) == 0) {
        path = "/vol/external01/" + path.substr(4);
    }

    std::ofstream log("/vol/external01/retrou/launcher.log", std::ios::app);
    log << std::unitbuf;
    if (log) {
        log << "HBL load " << path << std::endl;
    }
    if (!retrou::hbl_load_to_memory(path, error)) {
        if (log) {
            log << "HBL failed: " << error << std::endl;
        }
        return false;
    }
    if (log) {
        log << "HBL ok" << std::endl;
    }
    return true;
}

struct TextCache {
    std::map<std::string, SDL_Texture *> items;

    ~TextCache()
    {
        clear();
    }

    void clear()
    {
        for (auto &pair : items) {
            SDL_DestroyTexture(pair.second);
        }
        items.clear();
    }
};

SDL_Texture *text_texture(SDL_Renderer *renderer, TextCache &cache, TTF_Font *font,
                          const std::string &text, SDL_Color color)
{
    if (!renderer || !font || text.empty()) {
        return nullptr;
    }

    char key[64];
    std::snprintf(key, sizeof(key), "%p|%02X%02X%02X%02X|",
                  static_cast<void *>(font), color.r, color.g, color.b, color.a);
    std::string cache_key = key + text;

    auto found = cache.items.find(cache_key);
    if (found != cache.items.end()) {
        return found->second;
    }

    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text.c_str(), color);
    if (!surface) {
        return nullptr;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (texture) {
        cache.items[cache_key] = texture;
    }
    return texture;
}

int text_width(TTF_Font *font, const std::string &text)
{
    if (!font || text.empty()) {
        return 0;
    }
    int width = 0;
    int height = 0;
    TTF_SizeUTF8(font, text.c_str(), &width, &height);
    return width;
}

std::string fit_text(TTF_Font *font, const std::string &text, int max_width)
{
    if (text_width(font, text) <= max_width) {
        return text;
    }

    std::string result = text;
    while (!result.empty() && text_width(font, result + "...") > max_width) {
        result.pop_back();
    }
    return result + "...";
}

void draw_text(SDL_Renderer *renderer, TextCache &cache, TTF_Font *font,
               const std::string &text, int x, int y, SDL_Color color)
{
    SDL_Texture *texture = text_texture(renderer, cache, font, text, color);
    if (!texture) {
        return;
    }
    int width = 0;
    int height = 0;
    SDL_QueryTexture(texture, nullptr, nullptr, &width, &height);
    SDL_Rect dest{x, y, width, height};
    SDL_RenderCopy(renderer, texture, nullptr, &dest);
}

void draw_text_center(SDL_Renderer *renderer, TextCache &cache, TTF_Font *font,
                      const std::string &text, int center_x, int y, SDL_Color color)
{
    draw_text(renderer, cache, font, text, center_x - text_width(font, text) / 2, y, color);
}

void fill_rect(SDL_Renderer *renderer, int x, int y, int w, int h, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderFillRect(renderer, &rect);
}

void outline_rect(SDL_Renderer *renderer, int x, int y, int w, int h, SDL_Color color)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderDrawRect(renderer, &rect);
}

void draw_background(SDL_Renderer *renderer, int width, int height, uint32_t accent)
{
    /* A handful of bands instead of one draw call per scanline. The old
     * 720+ lines per screen per frame were the main cost on Wii U. */
    const int bands = 8;
    for (int band = 0; band < bands; ++band) {
        int y0 = band * height / bands;
        int y1 = (band + 1) * height / bands;
        float t = static_cast<float>(band) / static_cast<float>(bands - 1);
        uint8_t r = static_cast<uint8_t>(8 + 8 * t);
        uint8_t g = static_cast<uint8_t>(13 + 8 * t);
        uint8_t b = static_cast<uint8_t>(24 + 14 * t);
        fill_rect(renderer, 0, y0, width, y1 - y0, rgba(r, g, b));
    }

    fill_rect(renderer, 0, 0, width, 2,
              rgba(static_cast<uint8_t>((accent >> 16) & 0xFF),
                   static_cast<uint8_t>((accent >> 8) & 0xFF),
                   static_cast<uint8_t>(accent & 0xFF), 180));
}

void draw_header(SDL_Renderer *renderer, TextCache &cache, TTF_Font *title_font,
                 TTF_Font *body_font, int width, int height, bool tv, bool connected)
{
    (void)height;
    int margin = tv ? 28 : 18;
    draw_text(renderer, cache, title_font, "RetroU", margin, tv ? 16 : 10, rgba(255, 255, 255));
    draw_text(renderer, cache, body_font, "GLASS OS  /  WII U", margin, tv ? 56 : 44, rgba(148, 163, 184));

    std::string status = connected ? "WII U GAMEPAD" : "CONECTA UN MANDO";
    int status_width = text_width(body_font, status);
    fill_rect(renderer, width - margin - status_width - 24, tv ? 22 : 14,
              status_width + 24, 28, rgba(255, 255, 255, 18));
    draw_text(renderer, cache, body_font, status, width - margin - status_width - 12,
              tv ? 27 : 19, rgba(226, 232, 240));
}

int console_count(const AppState &app, int system_index)
{
    if (system_index < 0 || system_index > kConsoleCount) {
        return 0;
    }
    return app.counts[system_index];
}

void set_system(AppState &app, int system)
{
    int count = kConsoleCount + 1;
    app.system = (system % count + count) % count;
    app.game = 0;
    app.scroll_row = 0;
    app.message.clear();
    refresh_visible(app);
}

void move_selection(AppState &app, int dx, int dy, int columns, int count)
{
    if (count <= 0) {
        return;
    }
    int index = app.game + dx + dy * columns;
    app.game = std::max(0, std::min(count - 1, index));
    app.message.clear();
}

void render_screen(SDL_Renderer *renderer, TextCache &cache, TTF_Font *title_font,
                   TTF_Font *heading_font, TTF_Font *body_font, TTF_Font *small_font,
                   AppState &app, int width, int height, bool tv, bool connected)
{
    const int margin = tv ? 28 : 18;
    const int header_h = tv ? 84 : 64;
    const int footer_h = tv ? 62 : 46;
    const int sidebar_w = tv ? 230 : 158;
    const int gap = tv ? 14 : 9;
    const int columns = tv ? 4 : 3;
    const int rows = tv ? 3 : 2;

    const uint32_t accent = app.system == 0 ? 0xF5D97F : kConsoles[app.system - 1].color;
    draw_background(renderer, width, height, accent);
    draw_header(renderer, cache, title_font, small_font, width, height, tv, connected);

    /* Sidebar */
    int sidebar_x = margin;
    int sidebar_y = header_h + 8;
    int sidebar_h = height - sidebar_y - footer_h - margin;
    fill_rect(renderer, sidebar_x, sidebar_y, sidebar_w, sidebar_h, rgba(15, 23, 42, 170));
    outline_rect(renderer, sidebar_x, sidebar_y, sidebar_w, sidebar_h, rgba(255, 255, 255, 38));

    draw_text(renderer, cache, small_font, "CONSOLAS", sidebar_x + 14, sidebar_y + 12,
              rgba(148, 163, 184));
    int row_y = sidebar_y + 38;
    int row_h = tv ? 40 : 31;
    for (int i = 0; i <= kConsoleCount; ++i) {
        const char *name = i == 0 ? "Todas" : kConsoles[i - 1].name;
        int count = console_count(app, i);
        bool selected = app.system == i;
        if (selected) {
            fill_rect(renderer, sidebar_x + 8, row_y - 3, sidebar_w - 16, row_h - 3,
                      rgba(255, 255, 255, 24));
            outline_rect(renderer, sidebar_x + 8, row_y - 3, sidebar_w - 16, row_h - 3,
                         rgba(255, 255, 255, 80));
        }
        draw_text(renderer, cache, body_font, name, sidebar_x + 18, row_y, rgba(241, 245, 249));
        std::string count_text = std::to_string(count);
        draw_text(renderer, cache, small_font, count_text,
                  sidebar_x + sidebar_w - 18 - text_width(small_font, count_text), row_y + 2,
                  rgba(148, 163, 184));
        row_y += row_h;
    }

    /* Content */
    int content_x = sidebar_x + sidebar_w + (tv ? 18 : 10);
    int content_w = width - content_x - margin;
    int content_y = header_h + 8;
    int content_h = height - content_y - footer_h - margin;

    const std::vector<const RomEntry *> &games = app.visible;
    if (games.empty()) {
        fill_rect(renderer, content_x, content_y, content_w, content_h, rgba(15, 23, 42, 150));
        outline_rect(renderer, content_x, content_y, content_w, content_h, rgba(255, 255, 255, 30));
        draw_text_center(renderer, cache, heading_font, "Sin ROMs en esta consola",
                         content_x + content_w / 2, content_y + content_h / 2 - 30, rgba(226, 232, 240));
        draw_text_center(renderer, cache, small_font, "Copia ROMs a sd:/retrou/roms/<Consola>/",
                         content_x + content_w / 2, content_y + content_h / 2 + 10, rgba(148, 163, 184));
    } else {
        int card_w = (content_w - gap * (columns - 1)) / columns;
        int card_h = (content_h - gap * (rows - 1)) / rows;
        int selected_row = app.game / columns;
        if (selected_row < app.scroll_row) {
            app.scroll_row = selected_row;
        }
        if (selected_row >= app.scroll_row + rows) {
            app.scroll_row = selected_row - rows + 1;
        }

        for (int i = 0; i < columns * rows; ++i) {
            int index = app.scroll_row * columns + i;
            if (index >= static_cast<int>(games.size())) {
                break;
            }
            int row = i / columns;
            int col = i % columns;
            int x = content_x + col * (card_w + gap);
            int y = content_y + row * (card_h + gap);
            const RomEntry &game = *games[index];
            bool selected = index == app.game;

            uint8_t r = static_cast<uint8_t>((game.color >> 16) & 0xFF);
            uint8_t g = static_cast<uint8_t>((game.color >> 8) & 0xFF);
            uint8_t b = static_cast<uint8_t>(game.color & 0xFF);

            fill_rect(renderer, x, y, card_w, card_h, rgba(15, 23, 42, 190));
            fill_rect(renderer, x + 1, y + 1, card_w - 2, static_cast<int>(card_h * 0.56f),
                      rgba(r, g, b, selected ? 210 : 160));
            outline_rect(renderer, x, y, card_w, card_h,
                         selected ? rgba(255, 255, 255, 230) : rgba(255, 255, 255, 36));

            const ConsoleDef *console = console_by_id(game.system);
            draw_text(renderer, cache, small_font, console ? console->name : game.system,
                      x + 10, y + 10, rgba(255, 255, 255, 220));

            std::string initials = game.title.substr(0, 2);
            std::transform(initials.begin(), initials.end(), initials.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            draw_text_center(renderer, cache, title_font, initials,
                             x + card_w / 2, y + static_cast<int>(card_h * 0.20f),
                             rgba(255, 255, 255, 210));

            std::string title = fit_text(body_font, game.title, card_w - 20);
            draw_text(renderer, cache, body_font, title, x + 10,
                      y + static_cast<int>(card_h * 0.64f), rgba(241, 245, 249));
            draw_text(renderer, cache, small_font, "ROM", x + 10,
                      y + static_cast<int>(card_h * 0.80f), rgba(148, 163, 184));
        }
    }

    /* Footer */
    int footer_y = height - footer_h;
    fill_rect(renderer, 0, footer_y, width, footer_h, rgba(8, 13, 24, 210));
    outline_rect(renderer, 0, footer_y, width, 1, rgba(255, 255, 255, 30));
    draw_text(renderer, cache, small_font,
              "L/R  CONSOLA     D-PAD  NAVEGAR     A  JUGAR     B  VOLVER     X  DETALLE",
              margin, footer_y + (footer_h - 18) / 2, rgba(203, 213, 225));

    if (!app.message.empty()) {
        int message_w = text_width(small_font, app.message) + 28;
        fill_rect(renderer, (width - message_w) / 2, footer_y - 46, message_w, 34,
                  rgba(15, 23, 42, 230));
        outline_rect(renderer, (width - message_w) / 2, footer_y - 46, message_w, 34,
                     rgba(255, 255, 255, 60));
        draw_text_center(renderer, cache, small_font, app.message, width / 2,
                         footer_y - 37, rgba(255, 255, 255));
    }
}

SDL_Renderer *make_renderer(SDL_Window *window)
{
    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, 0);
    }
    return renderer;
}

SDL_GameController *open_first_controller()
{
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (SDL_IsGameController(i)) {
            SDL_GameController *controller = SDL_GameControllerOpen(i);
            if (controller) {
                return controller;
            }
        }
    }
    return nullptr;
}

TTF_Font *open_font(bool bold, int size)
{
    const unsigned char *begin = bold ? _binary_ui_bold_ttf_start : _binary_ui_ttf_start;
    const unsigned char *end = bold ? _binary_ui_bold_ttf_end : _binary_ui_ttf_end;
    size_t length = static_cast<size_t>(end - begin);
    if (length > 0) {
        SDL_RWops *rw = SDL_RWFromConstMem(begin, static_cast<int>(length));
        if (rw) {
            TTF_Font *font = TTF_OpenFontRW(rw, 1, size);
            if (font) {
                return font;
            }
        }
    }

    const char *primary = bold ? "/vol/content/ui-bold.ttf" : "/vol/content/ui.ttf";
    const char *fallback = bold ? "/vol/external01/retrou/assets/ui-bold.ttf"
                                : "/vol/external01/retrou/assets/ui.ttf";
    const char *paths[] = {primary, fallback};

    for (const char *path : paths) {
        TTF_Font *font = TTF_OpenFont(path, size);
        if (font) {
            return font;
        }
    }
    return nullptr;
}

void handle_action(AppState &app, int action)
{
    const std::vector<const RomEntry *> &games = app.visible;

    switch (action) {
        case 0: /* up */
            move_selection(app, 0, -1, 4, static_cast<int>(games.size()));
            break;
        case 1: /* down */
            move_selection(app, 0, 1, 4, static_cast<int>(games.size()));
            break;
        case 2: /* left */
            move_selection(app, -1, 0, 4, static_cast<int>(games.size()));
            break;
        case 3: /* right */
            move_selection(app, 1, 0, 4, static_cast<int>(games.size()));
            break;
        case 4: /* system previous */
            set_system(app, app.system - 1);
            break;
        case 5: /* system next */
            set_system(app, app.system + 1);
            break;
        case 6: /* launch */
            if (games.empty()) {
                app.message = "No hay ROMs";
                break;
            }
            if (app.game < 0 || app.game >= static_cast<int>(games.size())) {
                app.game = 0;
            }
            {
                const RomEntry &rom = *games[app.game];
#if defined(RETROU_LAUNCHER)
                std::string label;
                if (rom.system == "nes") {
                    label = "NES";
                } else if (rom.system == "snes") {
                    label = "SNES";
                } else if (rom.system == "gba") {
                    label = "GBA";
                } else {
                    app.message = "No hay RPX para " + rom.system;
                    break;
                }

                std::string rpx_path = join_path(
                    join_path(app.library.sd_root, "retrou"),
                    "cores/RetroU-" + label + ".rpx");
                struct stat info;
                if (stat(rpx_path.c_str(), &info) != 0) {
                    app.message = "Falta retrou/cores/RetroU-" + label + ".rpx";
                    break;
                }
                app.launch_core = rpx_path;
                app.launch_rom = rom.path;
                app.pending_launch = true;
                app.running = false;
#elif defined(RETROU_STATIC_CORE) && defined(RETROU_CORE_SYSTEM)
                if (rom.system != RETROU_CORE_SYSTEM) {
                    app.message = std::string("Esta build es ") + RETROU_CORE_LABEL;
                    break;
                }
                app.rpl_rom = rom.path;
                app.pending_rpl = true;
                app.message = "Cargando " RETROU_CORE_LABEL "...";
#elif defined(RETROU_STATIC_CORE)
                app.message = "Este build es solo de prueba";
#else
                const CoreEntry *core = find_core(app.library, rom.system);
                if (!core) {
                    app.message = "No hay core .rpl/.rpx para " + rom.system;
                    break;
                }
                if (core->rpl) {
                    app.rpl_core = core->path;
                    app.rpl_rom = rom.path;
                    app.pending_rpl = true;
                    app.message = "Cargando core en RetroU...";
                    break;
                }
                app.launch_core = core->path;
                app.launch_rom = rom.path;
                app.pending_launch = true;
                app.running = false;
#endif
            }
            break;
        case 7: /* detail */
            app.message = "Detalle: proxima fase";
            break;
        case 8: /* favorite */
            app.message = "Favoritos: proxima fase";
            break;
        case 9: /* back to all */
            set_system(app, 0);
            break;
        default:
            break;
    }
}

void render_core_menu(SDL_Renderer *renderer, TextCache &cache, TTF_Font *heading_font,
                      TTF_Font *body_font, TTF_Font *small_font, int width, int height,
                      int selected)
{
    fill_rect(renderer, 0, 0, width, height, rgba(0, 0, 0, 165));

    int panel_w = std::min(width - 48, 560);
    int panel_h = 286;
    int panel_x = (width - panel_w) / 2;
    int panel_y = (height - panel_h) / 2;

    fill_rect(renderer, panel_x, panel_y, panel_w, panel_h, rgba(15, 23, 42, 245));
    outline_rect(renderer, panel_x, panel_y, panel_w, panel_h, rgba(255, 255, 255, 80));

    draw_text(renderer, cache, heading_font, "MENÚ RETROU", panel_x + 24, panel_y + 20,
              rgba(255, 255, 255));
    draw_text(renderer, cache, small_font, "MINUS  CERRAR     A  SELECCIONAR",
              panel_x + 24, panel_y + 58, rgba(148, 163, 184));

    const char *options[] = {
        "Continuar",
        "Cerrar juego y volver a RetroU",
    };

    for (int i = 0; i < 2; ++i) {
        int y = panel_y + 92 + i * 52;
        if (i == selected) {
            fill_rect(renderer, panel_x + 14, y - 8, panel_w - 28, 40,
                      rgba(245, 217, 127, 45));
            outline_rect(renderer, panel_x + 14, y - 8, panel_w - 28, 40,
                         rgba(245, 217, 127, 150));
        }
        draw_text(renderer, cache, body_font, options[i], panel_x + 30, y,
                  i == selected ? rgba(255, 255, 255) : rgba(203, 213, 225));
    }

    draw_text(renderer, cache, small_font, "Savestates y remapeo: próximamente",
              panel_x + 24, panel_y + panel_h - 48, rgba(148, 163, 184));
    draw_text(renderer, cache, small_font, "B  CERRAR MENÚ",
              panel_x + 24, panel_y + panel_h - 26, rgba(148, 163, 184));
}

void run_rpl_core(AppState &app, const std::string &core_path, const std::string &rom_path,
                  SDL_Renderer *tv, SDL_Renderer *drc, TTF_Font *heading_font,
                  TTF_Font *body_font, TTF_Font *small_font)
{
    {
        std::ofstream log("/vol/external01/retrou/retrou_rpl.log", std::ios::trunc);
        if (log) {
            log << "=== run_rpl_core " << core_path << " rom=" << rom_path << " ===\n";
        }
    }

    retrou::RplCore core;
    std::string error;

    TextCache loading_tv;
    TextCache loading_drc;
    if (tv) {
        SDL_SetRenderDrawColor(tv, 0, 0, 0, 255);
        SDL_RenderClear(tv);
        draw_text_center(tv, loading_tv, heading_font, "Cargando core...", 640, 344,
                         rgba(255, 255, 255));
        SDL_RenderPresent(tv);
    }
    if (drc) {
        SDL_SetRenderDrawColor(drc, 0, 0, 0, 255);
        SDL_RenderClear(drc);
        draw_text_center(drc, loading_drc, heading_font, "Cargando core...", 427, 224,
                         rgba(255, 255, 255));
        SDL_RenderPresent(drc);
    }

    std::string dyn_path = core_path;

    if (!core.load(dyn_path, tv, drc, error)) {
        app.message = "RPL: " + error;
        return;
    }

    if (!core.loadGame(rom_path, error)) {
        app.message = "RPL juego: " + error;
        core.unload();
        return;
    }

    bool menu_open = false;
    int menu_selected = 0;
    TextCache tv_menu_text;
    TextCache drc_menu_text;

    while (app.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                app.running = false;
                break;
            }

            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                unsigned button = event.cbutton.button;

                if (button == SDL_CONTROLLER_BUTTON_BACK) {
                    menu_open = !menu_open;
                    menu_selected = 0;
                    continue;
                }

                if (!menu_open) {
                    /* B no longer closes the core; only the menu does. */
                    continue;
                }

                if (button == SDL_CONTROLLER_BUTTON_DPAD_UP ||
                    button == SDL_CONTROLLER_BUTTON_DPAD_LEFT) {
                    menu_selected = (menu_selected + 1) % 2;
                } else if (button == SDL_CONTROLLER_BUTTON_DPAD_DOWN ||
                           button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    menu_selected = (menu_selected + 1) % 2;
                } else if (button == SDL_CONTROLLER_BUTTON_A) {
                    if (menu_selected == 1) {
                        core.unload();
                        app.message = "Juego cerrado";
                        return;
                    }
                    menu_open = false;
                } else if (button == SDL_CONTROLLER_BUTTON_B) {
                    menu_open = false;
                }
            }

            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                if (menu_open) {
                    menu_open = false;
                } else {
                    menu_open = true;
                    menu_selected = 0;
                }
            }
        }

        if (!app.running) {
            break;
        }

        if (!menu_open) {
            core.runFrame();
        }

        if (tv) {
            SDL_SetRenderDrawColor(tv, 0, 0, 0, 255);
            SDL_RenderClear(tv);
            core.present(tv, 1280, 720);
            if (menu_open) {
                render_core_menu(tv, tv_menu_text, heading_font, body_font, small_font,
                                 1280, 720, menu_selected);
            }
            SDL_RenderPresent(tv);
        }
        if (drc) {
            SDL_SetRenderDrawColor(drc, 0, 0, 0, 255);
            SDL_RenderClear(drc);
            core.present(drc, 854, 480);
            if (menu_open) {
                render_core_menu(drc, drc_menu_text, heading_font, body_font, small_font,
                                 854, 480, menu_selected);
            }
            SDL_RenderPresent(drc);
        }
        SDL_Delay(16);
    }

    core.unload();
}

#ifdef RETROU_STATIC_CORE
void run_static_core(AppState &app, const std::string &rom_path,
                     SDL_Renderer *tv, SDL_Renderer *drc,
                     TTF_Font *heading_font, TTF_Font *body_font,
                     TTF_Font *small_font)
{
    {
        std::ofstream log("/vol/external01/retrou/retrou_rpx.log", std::ios::trunc);
        if (log) {
            log << "=== run_static_core rom=" << rom_path << " ===\n";
        }
    }

    retrou::RplCore core;
    std::string error;

    TextCache loading_tv;
    TextCache loading_drc;
    if (tv) {
        SDL_SetRenderDrawColor(tv, 0, 0, 0, 255);
        SDL_RenderClear(tv);
        draw_text_center(tv, loading_tv, heading_font, "Cargando core...", 640, 344,
                         rgba(255, 255, 255));
        SDL_RenderPresent(tv);
    }
    if (drc) {
        SDL_SetRenderDrawColor(drc, 0, 0, 0, 255);
        SDL_RenderClear(drc);
        draw_text_center(drc, loading_drc, heading_font, "Cargando core...", 427, 224,
                         rgba(255, 255, 255));
        SDL_RenderPresent(drc);
    }

    if (!core.loadStaticCore(tv, drc, error)) {
        app.message = "RPX: " + error;
        return;
    }
    if (!core.loadGame(rom_path, error)) {
        app.message = "RPX juego: " + error;
        core.unload();
        return;
    }

    bool menu_open = false;
    int menu_selected = 0;
    TextCache tv_menu_text;
    TextCache drc_menu_text;

    while (app.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                app.running = false;
                break;
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                unsigned button = event.cbutton.button;
                if (button == SDL_CONTROLLER_BUTTON_BACK) {
                    menu_open = !menu_open;
                    menu_selected = 0;
                    continue;
                }
                if (!menu_open) {
                    continue;
                }
                if (button == SDL_CONTROLLER_BUTTON_DPAD_UP ||
                    button == SDL_CONTROLLER_BUTTON_DPAD_LEFT ||
                    button == SDL_CONTROLLER_BUTTON_DPAD_DOWN ||
                    button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
                    menu_selected = (menu_selected + 1) % 2;
                } else if (button == SDL_CONTROLLER_BUTTON_A) {
                    if (menu_selected == 1) {
                        core.unload();
                        app.message = "Test RPX cerrado";
                        return;
                    }
                    menu_open = false;
                } else if (button == SDL_CONTROLLER_BUTTON_B) {
                    menu_open = false;
                }
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                menu_open = !menu_open;
                menu_selected = 0;
            }
        }

        if (!app.running) {
            break;
        }
        if (!menu_open) {
            core.runFrame();
        }

        if (tv) {
            SDL_SetRenderDrawColor(tv, 0, 0, 0, 255);
            SDL_RenderClear(tv);
            core.present(tv, 1280, 720);
            if (menu_open) {
                render_core_menu(tv, tv_menu_text, heading_font, body_font, small_font,
                                 1280, 720, menu_selected);
            }
            SDL_RenderPresent(tv);
        }
        if (drc) {
            SDL_SetRenderDrawColor(drc, 0, 0, 0, 255);
            SDL_RenderClear(drc);
            core.present(drc, 854, 480);
            if (menu_open) {
                render_core_menu(drc, drc_menu_text, heading_font, body_font, small_font,
                                 854, 480, menu_selected);
            }
            SDL_RenderPresent(drc);
        }
        SDL_Delay(16);
    }

    core.unload();
}
#endif

void read_last_launch(AppState &app)
{
    std::string path = join_path(join_path(app.library.sd_root, "retrou"), "launch/last.txt");
    std::ifstream in(path);
    if (!in) {
        return;
    }

    std::string rom_path;
    std::getline(in, rom_path);
    in.close();

    if (!rom_path.empty()) {
        struct stat info;
        if (stat(rom_path.c_str(), &info) == 0) {
            app.rpl_rom = rom_path;
            app.pending_rpl = true;
        }
    }
    std::remove(path.c_str());
}

void run_test_core(AppState &app, SDL_Renderer *tv, SDL_Renderer *drc,
                   TTF_Font *heading_font, TTF_Font *body_font, TTF_Font *small_font)
{
#if defined(RETROU_LAUNCHER)
    (void)tv;
    (void)drc;
    (void)heading_font;
    (void)body_font;
    (void)small_font;
    app.message = "Usa RetroU-Test.wuhb";
#elif defined(RETROU_STATIC_CORE) && defined(RETROU_TEST_CORE)
    run_static_core(app, "", tv, drc, heading_font, body_font, small_font);
#elif defined(RETROU_STATIC_CORE)
    (void)tv;
    (void)drc;
    (void)heading_font;
    (void)body_font;
    (void)small_font;
    app.message = "Usa A sobre una ROM";
#else
    std::string test_path = join_path(join_path(app.library.sd_root, "retrou"),
                                      "cores/test.rpl");
    run_rpl_core(app, test_path, "", tv, drc,
                 heading_font, body_font, small_font);
#endif
}

} // namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        return 1;
    }
    SDL_GameControllerEventState(SDL_ENABLE);

    SDL_Window *tv_window = SDL_CreateWindow(
        "RetroU", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_WIIU_TV_ONLY);
    SDL_Window *drc_window = SDL_CreateWindow(
        "RetroU", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 854, 480,
        SDL_WINDOW_SHOWN | SDL_WINDOW_WIIU_GAMEPAD_ONLY);

    SDL_Renderer *tv_renderer = tv_window ? make_renderer(tv_window) : nullptr;
    SDL_Renderer *drc_renderer = drc_window ? make_renderer(drc_window) : nullptr;

    if (!tv_renderer && !drc_renderer) {
        SDL_Quit();
        return 1;
    }

    if (TTF_Init() != 0) {
        SDL_Quit();
        return 1;
    }

    TTF_Font *title_font = open_font(true, 34);
    TTF_Font *heading_font = open_font(true, 24);
    TTF_Font *body_font = open_font(false, 18);
    TTF_Font *small_font = open_font(false, 13);

    if (!title_font || !heading_font || !body_font || !small_font) {
        if (title_font) TTF_CloseFont(title_font);
        if (heading_font) TTF_CloseFont(heading_font);
        if (body_font) TTF_CloseFont(body_font);
        if (small_font) TTF_CloseFont(small_font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    AppState app;
    app.library = load_library();
    rebuild_indexes(app);
#if defined(RETROU_STATIC_CORE) && !defined(RETROU_TEST_CORE)
    read_last_launch(app);
#endif
    if (app.library.roms.empty()) {
        app.message = "Copia ROMs a sd:/retrou/roms/<Consola>/";
    } else if (app.library.cores.empty()) {
        app.message = "Copia cores .rpx a sd:/retrou/cores/";
    }

    SDL_GameController *controller = open_first_controller();
    bool connected = controller != nullptr;

    TextCache tv_text;
    TextCache drc_text;
    bool ui_dirty = true;

    while (app.running) {
        if (!controller) {
            bool was_connected = connected;
            controller = open_first_controller();
            connected = controller != nullptr;
            if (was_connected != connected) {
                ui_dirty = true;
            }
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ui_dirty = true;
            if (event.type == SDL_QUIT) {
                app.running = false;
                break;
            }
            if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
                if (controller &&
                    SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) ==
                        event.cdevice.which) {
                    SDL_GameControllerClose(controller);
                    controller = nullptr;
                    connected = false;
                }
            }
            if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: handle_action(app, 0); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: handle_action(app, 1); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: handle_action(app, 2); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: handle_action(app, 3); break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: handle_action(app, 4); break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: handle_action(app, 5); break;
                    case SDL_CONTROLLER_BUTTON_A: handle_action(app, 6); break;
                    case SDL_CONTROLLER_BUTTON_B: handle_action(app, 9); break;
                    case SDL_CONTROLLER_BUTTON_X: handle_action(app, 7); break;
                    case SDL_CONTROLLER_BUTTON_Y: handle_action(app, 8); break;
                    case SDL_CONTROLLER_BUTTON_START: run_test_core(app, tv_renderer, drc_renderer, heading_font, body_font, small_font); break;
                    default: break;
                }
            }
            if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_UP: handle_action(app, 0); break;
                    case SDLK_DOWN: handle_action(app, 1); break;
                    case SDLK_LEFT: handle_action(app, 2); break;
                    case SDLK_RIGHT: handle_action(app, 3); break;
                    case SDLK_RETURN: handle_action(app, 6); break;
                    case SDLK_ESCAPE: app.running = false; break;
                    default: break;
                }
            }
        }

        if (app.pending_rpl) {
            app.pending_rpl = false;
            std::string core_path = app.rpl_core;
            std::string rom_path = app.rpl_rom;
            app.rpl_core.clear();
            app.rpl_rom.clear();
#ifdef RETROU_STATIC_CORE
            run_static_core(app, rom_path, tv_renderer, drc_renderer,
                            heading_font, body_font, small_font);
#else
            run_rpl_core(app, core_path, rom_path, tv_renderer, drc_renderer,
                         heading_font, body_font, small_font);
#endif
            ui_dirty = true;
            continue;
        }

        if (!ui_dirty) {
            SDL_Delay(16);
            continue;
        }

        if (tv_renderer) {
            SDL_SetRenderDrawColor(tv_renderer, 8, 13, 24, 255);
            SDL_RenderClear(tv_renderer);
            render_screen(tv_renderer, tv_text, title_font, heading_font, body_font, small_font,
                          app, 1280, 720, true, connected);
            SDL_RenderPresent(tv_renderer);
        }
        if (drc_renderer) {
            SDL_SetRenderDrawColor(drc_renderer, 8, 13, 24, 255);
            SDL_RenderClear(drc_renderer);
            render_screen(drc_renderer, drc_text, title_font, heading_font, body_font, small_font,
                          app, 854, 480, false, connected);
            SDL_RenderPresent(drc_renderer);
        }
        ui_dirty = false;
    }

    tv_text.clear();
    drc_text.clear();
    if (controller) {
        SDL_GameControllerClose(controller);
    }
    TTF_CloseFont(title_font);
    TTF_CloseFont(heading_font);
    TTF_CloseFont(body_font);
    TTF_CloseFont(small_font);
    TTF_Quit();
    if (tv_renderer) SDL_DestroyRenderer(tv_renderer);
    if (drc_renderer) SDL_DestroyRenderer(drc_renderer);
    if (tv_window) SDL_DestroyWindow(tv_window);
    if (drc_window) SDL_DestroyWindow(drc_window);

    /* Ask sysapp to switch titles while ProcUI is still alive. SDL_Quit
     * then releases the SDL port; the OS loads the core after we return. */
    if (app.pending_launch) {
        std::string error;
        if (!launch_core(app.library.sd_root, app.launch_core, app.launch_rom, error)) {
            SDL_Quit();
            return 1;
        }
    }

    SDL_Quit();
    return 0;
}
