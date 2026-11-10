#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>
#include <curl/curl.h>

#include "rpl_core.hpp"
#include <whb/sdcard.h>
#include <sysapp/launch.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <string>
#include <array>
#include <thread>
#include <vector>

#ifndef RETROU_APP_NAME
#define RETROU_APP_NAME "RetroU"
#endif

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

int title_region(const std::string &title);
int title_year(const std::string &title);
std::string normalize_text(std::string value);

struct ThemePreset {
    const char *id;
    const char *name;
    uint32_t backgroundTop;
    uint32_t backgroundBottom;
    uint32_t panel;
    uint32_t focus;
    uint32_t accent;
    uint32_t text;
    uint32_t muted;
};

const ThemePreset kThemes[] = {
    {"gray",  "Gris",       0x242A30, 0x15191D, 0x2C333B, 0xD8DDE2, 0xAEB7C0, 0xF1F5F9, 0x94A3B8},
    {"blue",  "Azul RetroU",0x080D18, 0x03060D, 0x0F172A, 0x4DA3FF, 0xF5D97F, 0xF8FAFC, 0x94A3B8},
    {"green", "Verde/Teal", 0x0B1F1C, 0x07110F, 0x102A25, 0x2DD4BF, 0x5EEAD4, 0xF0FDFA, 0x99F6E4},
    {"gold",  "Dorado OoT", 0x11160D, 0x070A05, 0x1C2414, 0xD4AF37, 0xE7C85A, 0xFEFCE8, 0xC4B47A},
    {"pink",  "Rosa",       0x1B0F1A, 0x0D060C, 0x2A1426, 0xF472B6, 0xFB7185, 0xFDF2F8, 0xF9A8D4},
};
const int kThemeCount = static_cast<int>(sizeof(kThemes) / sizeof(kThemes[0]));

struct RomEntry {
    std::string title;
    std::string path;
    std::string system;
    std::string developer;
    std::string genre;
    uint32_t color;
    int year = 0;
    int region = 0;
    bool favorite = false;
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
    bool sidebar_open = false;
    bool settings_screen = false;
    bool keyboard_open = false;
    bool dirty = false;
    bool favorites_only = false;
    int settings_index = 0;
    int theme_index = 0;
    int control_profile = 0;
    std::array<int, 8> control_map = {SDL_CONTROLLER_BUTTON_A,
                                     SDL_CONTROLLER_BUTTON_B,
                                     SDL_CONTROLLER_BUTTON_X,
                                     SDL_CONTROLLER_BUTTON_Y,
                                     SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
                                     SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                                     SDL_CONTROLLER_BUTTON_START,
                                     SDL_CONTROLLER_BUTTON_BACK};
    std::array<std::array<int, 8>, 2> control_maps = {control_map, control_map};
    int control_port = 0;
    std::array<int, 2> controller_port = {0, 1};
    int control_option = 0;
    bool control_listening = false;
    bool assets_downloading = false;
    int keyboard_index = 0;
    std::string search_query;
    int tooltip_focus = -1;
    int tooltip_shown_focus = -1;
    Uint32 focus_since = 0;
    int sidebar_index = 0;
    int sort_mode = 0;       /* 0 = name, 1 = region */
    int region_filter = 0;   /* 0 = all, 1 = USA, 2 = Europe, 3 = Japan */
    bool pending_launch = false;
    bool pending_rpl = false;
    std::string launch_core;
    std::string launch_rom;
    std::string rpl_core;
    std::string rpl_rom;
    std::string message;
};

SDL_GameController *g_game_port[2] = {nullptr, nullptr};
std::vector<SDL_GameController *> g_controllers;
void select_game_controllers(const AppState &app);

SDL_Color rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return SDL_Color{r, g, b, a};
}

SDL_Color rgba(uint32_t color, uint8_t alpha = 255)
{
    return SDL_Color{static_cast<uint8_t>((color >> 16) & 0xFF),
                     static_cast<uint8_t>((color >> 8) & 0xFF),
                     static_cast<uint8_t>(color & 0xFF), alpha};
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
            rom.region = title_region(rom.title);
            rom.year = title_year(rom.title);
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

std::string current_system_id()
{
#if defined(RETROU_CORE_SYSTEM)
    return RETROU_CORE_SYSTEM;
#else
    return "test";
#endif
}

std::string system_config_dir(const std::string &sd_root)
{
    return join_path(join_path(join_path(sd_root, "retrou"), "config"),
                     current_system_id());
}

void write_default_file(const std::string &path, const char *contents)
{
    struct stat info;
    if (stat(path.c_str(), &info) == 0) {
        return;
    }
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out) {
        out << contents;
    }
}

void load_system_config(AppState &app)
{
    std::string path = join_path(system_config_dir(app.library.sd_root),
                                 current_system_id() + ".conf");
    std::ifstream in(path);
    if (!in) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        size_t eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (key == "sort") {
            if (value == "region") app.sort_mode = 1;
            else if (value == "year") app.sort_mode = 2;
            else if (value == "category") app.sort_mode = 3;
            else if (value == "developer") app.sort_mode = 4;
            else app.sort_mode = 0;
        }
    }
}

void save_system_config(const AppState &app)
{
    std::string path = join_path(system_config_dir(app.library.sd_root),
                                 current_system_id() + ".conf");
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return;
    }
    static const char *sort_ids[] = {"name", "region", "year", "category", "developer"};
    out << "sort=" << sort_ids[std::max(0, std::min(4, app.sort_mode))] << "\n";
}

void load_theme(AppState &app)
{
    std::string path = join_path(join_path(app.library.sd_root, "retrou"), "config/theme.conf");
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("theme=", 0) != 0) continue;
        std::string id = line.substr(6);
        for (int i = 0; i < kThemeCount; ++i) {
            if (id == kThemes[i].id) {
                app.theme_index = i;
                return;
            }
        }
    }
}

void save_theme(const AppState &app)
{
    std::string path = join_path(join_path(app.library.sd_root, "retrou"), "config/theme.conf");
    std::ofstream out(path, std::ios::trunc);
    if (out) out << "theme=" << kThemes[app.theme_index].id << "\n";
}

const ThemePreset &current_theme(const AppState &app)
{
    return kThemes[std::max(0, std::min(kThemeCount - 1, app.theme_index))];
}

uint32_t theme_color(uint32_t color, uint8_t alpha = 255)
{
    return (color & 0x00FFFFFF) | (static_cast<uint32_t>(alpha) << 24);
}

std::string dat_value(const std::string &line)
{
    size_t first = line.find('"');
    size_t last = line.rfind('"');
    if (first == std::string::npos || last == std::string::npos || last <= first) return "";
    return line.substr(first + 1, last - first - 1);
}

void load_dat_file(AppState &app, const std::string &path, const std::string &kind)
{
    std::ifstream in(path);
    if (!in) return;
    std::vector<std::string> names;
    std::string value;
    std::string line;
    auto flush = [&]() {
        if (value.empty()) {
            names.clear();
            return;
        }
        for (const std::string &name : names) {
            for (RomEntry &rom : app.library.roms) {
                if (rom.title != name) continue;
                if (kind == "developer") rom.developer = normalize_text(value);
                else if (kind == "genre") rom.genre = normalize_text(value);
                else if (kind == "releaseyear") {
                    int year = std::atoi(value.c_str());
                    if (year > 0) rom.year = year;
                }
            }
        }
        names.clear();
        value.clear();
    };
    while (std::getline(in, line)) {
        std::string t = line;
        while (!t.empty() && (t.front() == '\t' || t.front() == ' ')) t.erase(t.begin());
        if (t.rfind("comment \"", 0) == 0 || t.rfind("name \"", 0) == 0) {
            names.push_back(dat_value(t));
        } else if (t.rfind(kind + " \"", 0) == 0) {
            value = dat_value(t);
        } else if (t == ")") {
            flush();
        }
    }
}

void load_libretro_metadata(AppState &app)
{
    std::string dir = join_path(join_path(app.library.sd_root, "retrou"), "metadata");
    std::string system = current_system_id();
    load_dat_file(app, join_path(dir, system + "-developer.dat"), "developer");
    load_dat_file(app, join_path(dir, system + "-genre.dat"), "genre");
    load_dat_file(app, join_path(dir, system + "-releaseyear.dat"), "releaseyear");
}

std::string normalize_text(std::string value)
{
    struct Replacement { const char *from; const char *to; };
    static const Replacement replacements[] = {
        {"\xC3\x83\xC2\xA1", "\xC3\xA1"},
        {"\xC3\x83\xC2\xA9", "\xC3\xA9"},
        {"\xC3\x83\xC2\xAD", "\xC3\xAD"},
        {"\xC3\x83\xC2\xB3", "\xC3\xB3"},
        {"\xC3\x83\xC2\xBA", "\xC3\xBA"},
        {"\xC3\x83\xC2\xB1", "\xC3\xB1"},
        {"\xC3\x83\xC2\xBC", "\xC3\xBC"},
    };
    for (const Replacement &replacement : replacements) {
        std::string from(replacement.from);
        std::string to(replacement.to);
        size_t pos = 0;
        while ((pos = value.find(from, pos)) != std::string::npos) {
            value.replace(pos, from.size(), to);
            pos += to.size();
        }
    }
    return value;
}

void load_metadata(AppState &app)
{
    std::ifstream in(join_path(system_config_dir(app.library.sd_root), "metadata.conf"));
    std::string line;
    while (std::getline(in, line)) {
        size_t a = line.find('|');
        size_t b = a == std::string::npos ? a : line.find('|', a + 1);
        size_t c = b == std::string::npos ? b : line.find('|', b + 1);
        size_t d = c == std::string::npos ? c : line.find('|', c + 1);
        if (a == std::string::npos || b == std::string::npos || c == std::string::npos ||
            d == std::string::npos) {
            continue;
        }
        std::string title = line.substr(0, a);
        for (RomEntry &rom : app.library.roms) {
            if (rom.title != title) continue;
            rom.developer = normalize_text(line.substr(a + 1, b - a - 1));
            rom.genre = normalize_text(line.substr(b + 1, c - b - 1));
            rom.region = std::atoi(line.substr(c + 1, d - c - 1).c_str());
            rom.year = std::atoi(line.substr(d + 1).c_str());
            break;
        }
    }
}

void load_control_config(AppState &app)
{
    std::ifstream in(join_path(system_config_dir(app.library.sd_root), "control-map.conf"));
    std::string line;
    int port = 0;
    while (std::getline(in, line)) {
        if (line == "[p1]") { port = 0; continue; }
        if (line == "[p2]") { port = 1; continue; }
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        int value = std::atoi(line.substr(eq + 1).c_str());
        if (key == "device1") app.controller_port[0] = value;
        else if (key == "device2") app.controller_port[1] = value;
        const char *keys[] = {"a", "b", "x", "y", "l", "r", "start", "select"};
        for (int i = 0; i < 8; ++i) {
            if (key == keys[i]) app.control_maps[port][i] = value;
        }
    }
    for (int port = 0; port < 2; ++port) {
        auto &map = app.control_maps[port];
        if (map[4] == 4 && map[5] == 5 && map[7] == 7) {
            map[4] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
            map[5] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
            map[7] = SDL_CONTROLLER_BUTTON_BACK;
        }
    }
}

void save_control_config(const AppState &app){
    std::string path = join_path(system_config_dir(app.library.sd_root), "control-map.conf");
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;
    const char *keys[] = {"a", "b", "x", "y", "l", "r", "start", "select"};
    for (int port = 0; port < 2; ++port) {
        out << "[p" << (port + 1) << "]\n";
        out << "device" << (port + 1) << "=" << app.controller_port[port] << "\n";
        for (int i = 0; i < 8; ++i) {
            out << keys[i] << "=" << app.control_maps[port][i] << "\n";
        }
    }
}

void load_favorites(AppState &app)
{
    std::ifstream in(join_path(system_config_dir(app.library.sd_root), "favorites.conf"));
    std::string path;
    while (std::getline(in, path)) {
        for (RomEntry &rom : app.library.roms) {
            if (rom.path == path) {
                rom.favorite = true;
            }
        }
    }
}

void save_favorites(const AppState &app)
{
    std::ofstream out(join_path(system_config_dir(app.library.sd_root), "favorites.conf"),
                      std::ios::binary | std::ios::trunc);
    for (const RomEntry &rom : app.library.roms) {
        if (rom.favorite) {
            out << rom.path << "\n";
        }
    }
}

void ensure_system_dirs(const std::string &sd_root)
{
    std::string retro = join_path(sd_root, "retrou");
    std::string system = current_system_id();
    mkdir(join_path(retro, "config").c_str(), 0777);
    mkdir(join_path(join_path(retro, "config"), system).c_str(), 0777);
    mkdir(join_path(retro, "saves").c_str(), 0777);
    mkdir(join_path(join_path(retro, "saves"), system).c_str(), 0777);
    mkdir(join_path(join_path(join_path(retro, "saves"), system), "save").c_str(), 0777);
    mkdir(join_path(retro, "snap").c_str(), 0777);
    mkdir(join_path(join_path(retro, "snap"), system).c_str(), 0777);

    mkdir(join_path(join_path(retro, "config"), "system").c_str(), 0777);
    write_default_file(join_path(join_path(retro, "config"), "theme.conf"),
                       "theme=gray\nstyle=crystal\n");
    std::string dir = join_path(join_path(retro, "config"), system);
    write_default_file(join_path(dir, system + ".conf"),
                       "sort=name\n");
    write_default_file(join_path(dir, "control-map.conf"),
                       "a=0\nb=1\nx=2\ny=3\nl=9\nr=10\nstart=6\nselect=4\n");
    write_default_file(join_path(dir, "video.conf"),
                       "scale=integer\naspect=core\nfilter=linear\n");
}

std::string system_save_directory(const std::string &sd_root)
{
    return join_path(join_path(join_path(join_path(sd_root, "retrou"), "saves"),
                               current_system_id()),
                     "save");
}

Library load_library()
{
    Library library;
    library.sd_root = mount_sd();
    std::string retro_root = join_path(library.sd_root, "retrou");
    scan_roms(join_path(retro_root, "roms"), library);
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

int title_region(const std::string &title)
{
    if (title.find("(USA)") != std::string::npos) {
        return 1;
    }
    if (title.find("(Europe)") != std::string::npos) {
        return 2;
    }
    if (title.find("(Japan)") != std::string::npos) {
        return 3;
    }
    return 0;
}

int title_year(const std::string &title)
{
    size_t open = title.find('(');
    while (open != std::string::npos) {
        if (open + 4 < title.size() &&
            std::isdigit(static_cast<unsigned char>(title[open + 1])) &&
            std::isdigit(static_cast<unsigned char>(title[open + 2])) &&
            std::isdigit(static_cast<unsigned char>(title[open + 3])) &&
            std::isdigit(static_cast<unsigned char>(title[open + 4]))) {
            return std::atoi(title.substr(open + 1, 4).c_str());
        }
        open = title.find('(', open + 1);
    }
    return 0;
}

std::map<std::pair<SDL_Renderer *, std::string>, SDL_Texture *> g_covers;

SDL_Texture *cover_for(SDL_Renderer *renderer, const RomEntry &game)
{
    std::string safe = game.title;
    for (char &c : safe) {
        if (c == '/' || c == '\\') c = '_';
    }
    std::string upper = game.system;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    std::string paths[] = {
        "/vol/external01/retrou/thumbnails/" + game.system + "/" + safe + ".png",
        "/vol/external01/retrou/thumbnails/" + upper + "/" + safe + ".png",
    };
    for (const std::string &path : paths) {
        auto key = std::make_pair(renderer, path);
        auto it = g_covers.find(key);
        if (it != g_covers.end() && it->second) return it->second;
        struct stat info;
        if (stat(path.c_str(), &info) != 0) continue;
        SDL_Surface *loaded = IMG_Load(path.c_str());
        if (!loaded) continue;
        SDL_Surface *surface = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_ARGB8888, 0);
        SDL_FreeSurface(loaded);
        if (!surface) continue;
        const int radius = 20;
        if (surface->w > radius * 2 && surface->h > radius * 2) {
            SDL_LockSurface(surface);
            for (int py = 0; py < radius; ++py) {
                for (int px = 0; px < radius; ++px) {
                    int dx = radius - px;
                    int dy = radius - py;
                    if (dx * dx + dy * dy <= radius * radius) continue;
                    int ys[2] = {py, surface->h - 1 - py};
                    int xs[2] = {px, surface->w - 1 - px};
                    for (int y : ys) for (int x : xs) {
                        uint32_t *pixel = reinterpret_cast<uint32_t *>(
                            static_cast<uint8_t *>(surface->pixels) + y * surface->pitch + x * 4);
                        *pixel = 0;
                    }
                }
            }
            SDL_UnlockSurface(surface);
        }
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        SDL_FreeSurface(surface);
        g_covers[key] = texture;
        if (texture) return texture;    }
    return nullptr;
}

std::string libretroSystemFolder(const std::string &system)
{
    if (system == "nes") return "Nintendo - Nintendo Entertainment System";
    if (system == "snes") return "Nintendo - Super Nintendo Entertainment System";
    if (system == "gba") return "Nintendo - Game Boy Advance";
    return "";
}

size_t curlWriteFile(void *ptr, size_t size, size_t nmemb, void *userdata);
bool download_if_missing(const RomEntry &game, const char *kind);
std::string urlEncode(const std::string &value);

bool download_to_file(const std::string &url, const std::string &path)
{
    FILE *fp = std::fopen(path.c_str(), "wb");
    if (!fp) return false;
    CURL *curl = curl_easy_init();
    bool ok = false;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        ok = curl_easy_perform(curl) == CURLE_OK;
        curl_easy_cleanup(curl);
    }
    std::fclose(fp);
    if (!ok) std::remove(path.c_str());
    return ok;
}

void download_metadata_for_system(const std::string &system)
{
    std::string folder = libretroSystemFolder(system);
    if (folder.empty()) return;
    std::string dir = "/vol/external01/retrou/metadata";
    mkdir(dir.c_str(), 0777);
    const char *kinds[] = {"developer", "genre", "releaseyear"};
    for (const char *kind : kinds) {
        std::string path = dir + "/" + system + "-" + kind + ".dat";
        struct stat info;
        if (stat(path.c_str(), &info) == 0) continue;
        std::string url = "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/" +
                          std::string(kind) + "/" + urlEncode(folder) + ".dat";
        download_to_file(url, path);
    }
}

void download_assets_thread(std::vector<RomEntry> games)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    std::string lastMetadata;
    for (const RomEntry &game : games) {
        if (game.system == "nes" || game.system == "snes" || game.system == "gba") {
            if (game.system != lastMetadata) {
                download_metadata_for_system(game.system);
                lastMetadata = game.system;
            }
            download_if_missing(game, "thumbnails");
            download_if_missing(game, "backdrops");
        }
    }
    curl_global_cleanup();
}

std::string urlEncode(const std::string &value)
{
    std::string out;
    for (unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%%%02X", c);
            out += hex;
        }
    }
    return out;
}

std::string libretroName(std::string title)
{
    for (char &c : title) {
        if (std::string("&*/:`<>?\\|\"").find(c) != std::string::npos) c = '_';
    }
    return title;
}

size_t curlWriteFile(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    return std::fwrite(ptr, size, nmemb, static_cast<FILE *>(userdata));
}

bool download_if_missing(const RomEntry &game, const char *kind)
{
    std::string folder = libretroSystemFolder(game.system);
    if (folder.empty()) return false;
    std::string dir = "/vol/external01/retrou/" + std::string(kind) + "/" + game.system;
    mkdir(("/vol/external01/retrou/" + std::string(kind)).c_str(), 0777);
    mkdir(dir.c_str(), 0777);
    std::string safe = libretroName(game.title);
    std::string dst = dir + "/" + safe + ".png";
    struct stat info;
    if (stat(dst.c_str(), &info) == 0) return true;

    const char *type = std::string(kind) == "thumbnails" ? "Named_Boxarts" : "Named_Snaps";
    std::string base = "https://thumbnails.libretro.com/" + urlEncode(folder) + "/" + type + "/";
    const char *suffixes[] = {"", " (USA)", " (Europe)", " (Japan)", " (World)"};
    for (const char *suffix : suffixes) {
        std::string url = base + urlEncode(safe + suffix) + ".png";
        FILE *fp = std::fopen(dst.c_str(), "wb");
        if (!fp) return false;
        CURL *curl = curl_easy_init();
        bool ok = false;
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFile);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
            ok = curl_easy_perform(curl) == CURLE_OK;
            curl_easy_cleanup(curl);
        }
        std::fclose(fp);
        if (ok) return true;
        std::remove(dst.c_str());
    }
    return false;
}

void draw_star(SDL_Renderer *renderer, int cx, int cy, int outer, int inner, SDL_Color color){
    SDL_Vertex vertices[12]{};
    vertices[0].position = {static_cast<float>(cx), static_cast<float>(cy)};
    vertices[0].color = color;
    vertices[0].tex_coord = {0, 0};
    for (int i = 0; i < 10; ++i) {
        float angle = -3.14159265f / 2.0f + i * 3.14159265f / 5.0f;
        int radius = (i % 2 == 0) ? outer : inner;
        vertices[i + 1].position = {
            static_cast<float>(cx + std::cos(angle) * radius),
            static_cast<float>(cy + std::sin(angle) * radius)};
        vertices[i + 1].color = color;
        vertices[i + 1].tex_coord = {0, 0};
    }
    int indices[30];
    for (int i = 0; i < 10; ++i) {
        indices[i * 3] = 0;
        indices[i * 3 + 1] = i + 1;
        indices[i * 3 + 2] = (i + 1) % 10 + 1;
    }
    SDL_RenderGeometry(renderer, nullptr, vertices, 11, indices, 30);
}

void refresh_visible(AppState &app)
{
    app.visible.clear();
    for (const RomEntry &rom : app.library.roms) {
        if (app.system != 0 && rom.system != kConsoles[app.system - 1].id) {
            continue;
        }
        if (!app.search_query.empty()) {
            std::string title = lower_copy(rom.title);
            if (title.find(lower_copy(app.search_query)) == std::string::npos) {
                continue;
            }
        }
        if (app.region_filter != 0 && rom.region != app.region_filter) {
            continue;
        }
        if (app.favorites_only && !rom.favorite) {
            continue;
        }
        app.visible.push_back(&rom);
    }

    if (app.sort_mode == 1) {
        std::sort(app.visible.begin(), app.visible.end(),
                  [](const RomEntry *a, const RomEntry *b) {
                      int ra = a->region;
                      int rb = b->region;
                      if (ra != rb) {
                          if (ra == 0) return false;
                          if (rb == 0) return true;
                          return ra < rb;
                      }
                      return lower_copy(a->title) < lower_copy(b->title);
                  });
    } else if (app.sort_mode == 2) {
        std::sort(app.visible.begin(), app.visible.end(),
                  [](const RomEntry *a, const RomEntry *b) {
                      int ya = a->year;
                      int yb = b->year;
                      if (ya != yb) {
                          if (ya == 0) return false;
                          if (yb == 0) return true;
                          return ya < yb;
                      }
                      return lower_copy(a->title) < lower_copy(b->title);
                  });
    } else if (app.sort_mode == 3 || app.sort_mode == 4) {
        const bool byDeveloper = app.sort_mode == 4;
        std::sort(app.visible.begin(), app.visible.end(),
                  [byDeveloper](const RomEntry *a, const RomEntry *b) {
                      std::string va = lower_copy(byDeveloper ? a->developer : a->genre);
                      std::string vb = lower_copy(byDeveloper ? b->developer : b->genre);
                      if (va.empty() != vb.empty()) {
                          return !va.empty();
                      }
                      if (va != vb) {
                          return va < vb;
                      }
                      return lower_copy(a->title) < lower_copy(b->title);
                  });
    } else {
        std::sort(app.visible.begin(), app.visible.end(),
                  [](const RomEntry *a, const RomEntry *b) {
                      return lower_copy(a->title) < lower_copy(b->title);
                  });
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

void fill_ellipse(SDL_Renderer *renderer, int cx, int cy, int rx, int ry, SDL_Color color)
{
    if (rx <= 0 || ry <= 0) {
        return;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int y = -ry; y <= ry; ++y) {
        float t = 1.0f - static_cast<float>(y * y) / static_cast<float>(ry * ry);
        int span = static_cast<int>(rx * std::sqrt(std::max(0.0f, t)));
        SDL_RenderDrawLine(renderer, cx - span, cy + y, cx + span, cy + y);
    }
}

void fill_rounded_rect(SDL_Renderer *renderer, int x, int y, int w, int h, int radius,
                       SDL_Color color)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    radius = std::max(0, std::min(radius, std::min(w, h) / 2));
    fill_rect(renderer, x + radius, y, w - radius * 2, h, color);
    fill_rect(renderer, x, y + radius, radius, h - radius * 2, color);
    fill_rect(renderer, x + w - radius, y + radius, radius, h - radius * 2, color);
    fill_ellipse(renderer, x + radius, y + radius, radius, radius, color);
    fill_ellipse(renderer, x + w - radius - 1, y + radius, radius, radius, color);
    fill_ellipse(renderer, x + radius, y + h - radius - 1, radius, radius, color);
    fill_ellipse(renderer, x + w - radius - 1, y + h - radius - 1, radius, radius, color);
}

void outline_rounded_rect(SDL_Renderer *renderer, int x, int y, int w, int h, int radius,
                          SDL_Color color)
{
    radius = std::max(0, std::min(radius, std::min(w, h) / 2));
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x + radius, y, x + w - radius, y);
    SDL_RenderDrawLine(renderer, x + radius, y + h - 1, x + w - radius, y + h - 1);
    SDL_RenderDrawLine(renderer, x, y + radius, x, y + h - radius);
    SDL_RenderDrawLine(renderer, x + w - 1, y + radius, x + w - 1, y + h - radius);

    auto arc = [&](int cx, int cy, int start, int end) {
        for (int angle = start; angle <= end; ++angle) {
            float rad = angle * 3.14159265f / 180.0f;
            int px = cx + static_cast<int>(std::cos(rad) * radius);
            int py = cy + static_cast<int>(std::sin(rad) * radius);
            SDL_RenderDrawPoint(renderer, px, py);
        }
    };
    arc(x + radius, y + radius, 180, 270);
    arc(x + w - radius - 1, y + radius, 270, 360);
    arc(x + w - radius - 1, y + h - radius - 1, 0, 90);
    arc(x + radius, y + h - radius - 1, 90, 180);
}

/* SDL has no backdrop-filter. This layered treatment reproduces the RetroUI
 * glass read: soft shadow, translucent navy body, specular top edge, darker
 * bottom half and a crisp inner border. */
void draw_glass_panel(SDL_Renderer *renderer, int x, int y, int w, int h, int radius,
                      uint8_t alpha = 205)
{
    fill_rounded_rect(renderer, x + 5, y + 9, w, h, radius, rgba(0, 0, 0, 95));
    fill_rounded_rect(renderer, x, y, w, h, radius, rgba(18, 27, 43, alpha));
    fill_rounded_rect(renderer, x + 1, y + 1, w - 2, h / 2, radius,
                      rgba(255, 255, 255, 13));
    fill_rounded_rect(renderer, x + 1, y + h / 2, w - 2, h - h / 2 - 1, radius,
                      rgba(0, 0, 0, 28));
    fill_rect(renderer, x + radius, y + 1, w - radius * 2, 1,
              rgba(255, 255, 255, 90));
    fill_rect(renderer, x + radius, y + h - 2, w - radius * 2, 1,
              rgba(0, 0, 0, 90));
    outline_rounded_rect(renderer, x, y, w, h, radius, rgba(255, 255, 255, 62));
}

void draw_background(SDL_Renderer *renderer, int width, int height, uint32_t accent,
                     uint32_t top_color = 0x080D18, uint32_t bottom_color = 0x03060D)
{
    /* A handful of bands instead of one draw call per scanline. The old
     * 720+ lines per screen per frame were the main cost on Wii U. */
    const int bands = 8;
    for (int band = 0; band < bands; ++band) {
        int y0 = band * height / bands;
        int y1 = (band + 1) * height / bands;
        float t = static_cast<float>(band) / static_cast<float>(bands - 1);
        uint8_t r = static_cast<uint8_t>(((top_color >> 16) & 0xFF) * (1.0f - t) +
                                         ((bottom_color >> 16) & 0xFF) * t);
        uint8_t g = static_cast<uint8_t>(((top_color >> 8) & 0xFF) * (1.0f - t) +
                                         ((bottom_color >> 8) & 0xFF) * t);
        uint8_t b = static_cast<uint8_t>((top_color & 0xFF) * (1.0f - t) +
                                         (bottom_color & 0xFF) * t);
        fill_rect(renderer, 0, y0, width, y1 - y0, rgba(r, g, b));
    }

    uint8_t ar = static_cast<uint8_t>((accent >> 16) & 0xFF);
    uint8_t ag = static_cast<uint8_t>((accent >> 8) & 0xFF);
    uint8_t ab = static_cast<uint8_t>(accent & 0xFF);

    /* Same cinematic language as RetroUI: an accent halo, a cool counter-halo,
     * a faint dot grid and a deep vignette. */
    fill_ellipse(renderer, static_cast<int>(width * 0.78f),
                 static_cast<int>(height * 0.12f),
                 static_cast<int>(width * 0.42f),
                 static_cast<int>(height * 0.48f), rgba(ar, ag, ab, 34));
    fill_ellipse(renderer, static_cast<int>(width * 0.12f),
                 static_cast<int>(height * 0.82f),
                 static_cast<int>(width * 0.36f),
                 static_cast<int>(height * 0.42f), rgba(45, 88, 140, 26));

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 16);
    for (int y = 30; y < height; y += 26) {
        for (int x = 18; x < width; x += 26) {
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }

    fill_rect(renderer, 0, 0, width, 2, rgba(ar, ag, ab, 190));
    fill_rect(renderer, 0, height - 2, width, 2, rgba(0, 0, 0, 180));
    fill_rect(renderer, 0, 0, width, 90, rgba(0, 0, 0, 70));
    fill_rect(renderer, 0, height - 100, width, 100, rgba(0, 0, 0, 100));
}

void draw_header(SDL_Renderer *renderer, TextCache &cache, TTF_Font *title_font,
                 TTF_Font *body_font, int width, int height, bool tv, bool connected,
                 const ThemePreset &theme)
{
    (void)height;
    int margin = tv ? 28 : 18;
    int bar_h = tv ? 70 : 54;
    draw_glass_panel(renderer, margin - 10, tv ? 10 : 6,
                     width - (margin - 10) * 2, bar_h, tv ? 18 : 14, 190);

    draw_text(renderer, cache, title_font, RETROU_APP_NAME, margin, tv ? 18 : 10,
              rgba(theme.text));
    draw_text(renderer, cache, body_font,
              std::string("BIBLIOTECA  /  ") + RETROU_CORE_LABEL,
              margin, tv ? 48 : 38, rgba(theme.muted));

    std::string status = connected ? "WII U GAMEPAD" : "CONECTA UN MANDO";
    int status_width = text_width(body_font, status);
    int pill_w = status_width + 24;
    fill_rounded_rect(renderer, width - margin - pill_w, tv ? 22 : 14, pill_w, 28,
                      14, rgba(255, 255, 255, 18));
    outline_rounded_rect(renderer, width - margin - pill_w, tv ? 22 : 14, pill_w, 28,
                         14, rgba(255, 255, 255, 34));
    draw_text(renderer, cache, body_font, status, width - margin - status_width - 12,
              tv ? 27 : 19, rgba(theme.text));
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

void draw_unsaved_banner(SDL_Renderer *renderer, TextCache &cache, TTF_Font *font,
                         int width, bool settings_screen);

void render_screen(SDL_Renderer *renderer, TextCache &cache, TTF_Font *title_font,
                   TTF_Font *heading_font, TTF_Font *body_font, TTF_Font *small_font,
                   AppState &app, int width, int height, bool tv, bool connected)
{
    const int margin = tv ? 28 : 18;
    const int header_h = tv ? 84 : 64;
    const int footer_h = tv ? 62 : 46;
    const int gap = tv ? 14 : 9;
    const int columns = 5;
    const int rows = tv ? 3 : 2;

    const uint32_t accent = current_theme(app).accent;
    const ThemePreset &theme = current_theme(app);
    draw_background(renderer, width, height, accent,
                    current_theme(app).backgroundTop, current_theme(app).backgroundBottom);
    draw_text(renderer, cache, title_font, "Biblioteca", margin, tv ? 18 : 10,
              rgba(current_theme(app).text));

    const int content_x = margin;
    const int content_w = width - margin * 2;
    const int content_y = header_h + 8;
    const int content_h = height - content_y - footer_h - margin;

    const std::vector<const RomEntry *> &games = app.visible;
    if (games.empty()) {
        fill_rounded_rect(renderer, content_x, content_y, content_w, content_h, 16,
                          rgba(15, 23, 42, 150));
        outline_rounded_rect(renderer, content_x, content_y, content_w, content_h, 16,
                             rgba(255, 255, 255, 30));
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

            const ConsoleDef *console = console_by_id(game.system);
            int cover_h = static_cast<int>(card_h * 0.62f);
            int radius = std::max(10, card_w / 16);

            fill_rounded_rect(renderer, x - 5, y - 5, card_w + 10, card_h + 10, radius + 4,
                              rgba(0, 0, 0, 95));
            if (selected) {
                fill_rounded_rect(renderer, x - 3, y - 3, card_w + 6, card_h + 6, radius + 3,
                                  rgba(theme.focus));
            }
            fill_rounded_rect(renderer, x, y, card_w, card_h, radius, rgba(theme.panel, 225));

            SDL_Texture *cover = cover_for(renderer, game);
            if (cover) {
                int tw = 0, th = 0;
                SDL_QueryTexture(cover, nullptr, nullptr, &tw, &th);
                SDL_Rect src{0, 0, tw, th};
                int dstW = card_w - 4, dstH = cover_h - 2;
                float srcAspect = tw > 0 && th > 0 ? static_cast<float>(tw) / th : 1.0f;
                float dstAspect = static_cast<float>(dstW) / std::max(1, dstH);
                if (srcAspect > dstAspect) {
                    src.w = static_cast<int>(th * dstAspect);
                    src.x = (tw - src.w) / 2;
                } else {
                    src.h = static_cast<int>(tw / dstAspect);
                    src.y = (th - src.h) / 2;
                }
                SDL_Rect dst{x + 2, y + 2, dstW, dstH};
                SDL_RenderCopy(renderer, cover, &src, &dst);
            } else {
                uint8_t r = static_cast<uint8_t>((game.color >> 16) & 0xFF);
                uint8_t g = static_cast<uint8_t>((game.color >> 8) & 0xFF);
                uint8_t b = static_cast<uint8_t>(game.color & 0xFF);
                fill_rounded_rect(renderer, x + 2, y + 2, card_w - 4, cover_h - 2, radius - 2,
                                  rgba(r, g, b, selected ? 235 : 205));
                fill_rounded_rect(renderer, x + 2, y + 2, card_w - 4, cover_h / 2, radius - 2,
                                  rgba(255, 255, 255, 18));
                std::string initials = game.title.substr(0, 2);
                std::transform(initials.begin(), initials.end(), initials.begin(),
                               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                draw_text_center(renderer, cache, title_font, initials, x + card_w / 2,
                                 y + cover_h / 2 - 28, rgba(255, 255, 255, 225));
            }

            if (game.favorite) {
                draw_star(renderer, x + card_w - 24, y + 22, 11, 4, rgba(theme.accent));
            }

            fill_rounded_rect(renderer, x + 2, y + cover_h, card_w - 4,
                              card_h - cover_h - 2, radius - 2,
                              rgba(theme.backgroundBottom, 235));
            outline_rounded_rect(renderer, x, y, card_w, card_h, radius,
                                 selected ? rgba(theme.focus) : rgba(255, 255, 255, 44));
            if (selected) {
                outline_rounded_rect(renderer, x - 3, y - 3, card_w + 6, card_h + 6, radius + 3,
                                     rgba(theme.focus, 150));
            }

            int text_y = y + cover_h + 10;
            std::string title = fit_text(body_font, game.title, card_w - 28);
            draw_text(renderer, cache, body_font, title, x + 14, text_y, rgba(theme.text));
            std::string meta = !game.developer.empty() ? game.developer : game.genre;
            if (game.year > 0) {
                if (!meta.empty()) meta += " · ";
                meta += std::to_string(game.year);
            }
            if (!meta.empty()) {
                draw_text(renderer, cache, small_font, meta, x + 14,
                          text_y + (tv ? 24 : 19), rgba(theme.muted));
            }
        }
    }

    /* Footer */
    int footer_y = height - footer_h;
    if (!app.visible.empty()) {
        const std::string full = app.visible[std::max(0, std::min(app.game,
            static_cast<int>(app.visible.size()) - 1))]->title;
        int card_w = (content_w - gap * (columns - 1)) / columns;
        bool truncated = fit_text(body_font, full, card_w - 20) != full;
        if (truncated && app.tooltip_shown_focus == app.game) {
            std::string shown = fit_text(body_font, full, width - 120);
            int tip_w = text_width(body_font, shown) + 30;
            int tip_x = (width - tip_w) / 2;
            int tip_y = footer_y - 44;
            fill_rounded_rect(renderer, tip_x, tip_y, tip_w, 32, 10, rgba(theme.panel, 235));
            outline_rounded_rect(renderer, tip_x, tip_y, tip_w, 32, 10, rgba(theme.focus, 150));
            draw_text(renderer, cache, body_font, shown, tip_x + 15, tip_y + 7, rgba(theme.text));
        }
    }
    fill_rect(renderer, 0, footer_y, width, footer_h, rgba(theme.backgroundBottom, 235));
    outline_rect(renderer, 0, footer_y, width, 1, rgba(255, 255, 255, 30));

    int key_x = margin;
    int key_y = footer_y + (footer_h - 30) / 2;
    fill_rounded_rect(renderer, key_x, key_y, 92, 30, 10, rgba(255, 255, 255, 225));
    draw_text(renderer, cache, small_font, "A  JUGAR", key_x + 13, key_y + 8,
              rgba(12, 19, 31));
    draw_text(renderer, cache, small_font, "D-PAD  NAVEGAR     B  VOLVER",
              key_x + 116, key_y + 8, rgba(203, 213, 225));

    std::string plus = "PLUS  MENÚ";
    int plus_w = text_width(small_font, plus) + 26;
    int plus_x = width - margin - plus_w;
    fill_rounded_rect(renderer, plus_x, key_y, plus_w, 30, 10, rgba(245, 217, 127, 38));
    outline_rounded_rect(renderer, plus_x, key_y, plus_w, 30, 10, rgba(245, 217, 127, 150));
    draw_text(renderer, cache, small_font, plus, plus_x + 13, key_y + 8,
              rgba(255, 246, 212));

    if (app.dirty) {
        draw_unsaved_banner(renderer, cache, small_font, width, false);
    }
}

void render_sidebar(SDL_Renderer *renderer, TextCache &cache, TTF_Font *heading_font,
                    TTF_Font *body_font, TTF_Font *small_font, int width, int height,
                    const AppState &app)
{
    fill_rect(renderer, 0, 0, width, height, rgba(0, 0, 0, 175));

    int panel_w = std::min(width - 40, 360);
    int panel_x = 24;
    int panel_y = 24;
    int panel_h = height - 48;

    fill_rounded_rect(renderer, panel_x, panel_y, panel_w, panel_h, 18,
                      rgba(current_theme(app).panel, 245));
    draw_glass_panel(renderer, panel_x, panel_y, panel_w, panel_h, 18, 225);

    draw_text(renderer, cache, heading_font, "FILTRAR Y ORGANIZAR", panel_x + 20,
              panel_y + 20, rgba(255, 255, 255));
    draw_text(renderer, cache, small_font, "PLUS / B  CERRAR", panel_x + 20, panel_y + 54,
              rgba(148, 163, 184));

    std::string sort_value;
    switch (app.sort_mode) {
        case 1: sort_value = "Región"; break;
        case 2: sort_value = "Año"; break;
        case 3: sort_value = "Categoría"; break;
        case 4: sort_value = "Desarrolladora"; break;
        default: sort_value = "Nombre"; break;
    }
    const char *regions[] = {"Todas", "USA", "Europa", "Japón"};
    std::string region_value = regions[std::max(0, std::min(3, app.region_filter))];

    const std::string labels[] = {
        std::string("Buscar: ") + (app.search_query.empty() ? "Todos" : app.search_query),
        "Ordenar por: " + sort_value,
        "Región: " + region_value,
        std::string("Favoritos: ") + (app.favorites_only ? "Solo" : "Todos"),
    };

    int y = panel_y + 92;
    for (int i = 0; i < 4; ++i) {
        if (i == app.sidebar_index) {
            fill_rounded_rect(renderer, panel_x + 12, y - 9, panel_w - 24, 44, 10,
                              rgba(current_theme(app).accent, 42));
            outline_rounded_rect(renderer, panel_x + 12, y - 9, panel_w - 24, 44, 10,
                                 rgba(current_theme(app).accent, 150));
        }
        draw_text(renderer, cache, body_font, labels[i], panel_x + 26, y,
                  i == app.sidebar_index ? rgba(current_theme(app).text)
                                         : rgba(current_theme(app).muted));
        y += 52;
    }
}

void sidebar_action(AppState &app, int action)
{
    switch (action) {
        case 0:
            app.sidebar_index = (app.sidebar_index + 3) % 4;
            break;
        case 1:
            app.sidebar_index = (app.sidebar_index + 1) % 4;
            break;
        case 9:
            app.sidebar_open = false;
            break;
        case 6:
            if (app.sidebar_index == 0) {
                app.keyboard_open = true;
                app.keyboard_index = 0;
            } else if (app.sidebar_index == 1) {
                app.sort_mode = (app.sort_mode + 1) % 5;
                refresh_visible(app);
                save_system_config(app);
            } else if (app.sidebar_index == 2) {
                app.region_filter = (app.region_filter + 1) % 4;
                refresh_visible(app);
            } else {
                app.favorites_only = !app.favorites_only;
                refresh_visible(app);
            }
            break;
        default:
            break;
    }
}

void draw_unsaved_banner(SDL_Renderer *renderer, TextCache &cache, TTF_Font *font,
                         int width, bool settings_screen)
{
    const std::string text = settings_screen
        ? "Hay cambios sin guardar, presione \"+\" para guardarlos"
        : "Hay cambios sin guardar, diríjase a ajustes para guardarlos";
    int text_w = text_width(font, text);
    int w = text_w + 32;
    int x = (width - w) / 2;
    int y = 84;
    fill_rounded_rect(renderer, x, y, w, 30, 10, rgba(120, 78, 10, 225));
    outline_rounded_rect(renderer, x, y, w, 30, 10, rgba(245, 217, 127, 190));
    draw_text(renderer, cache, font, text, x + 16, y + 7, rgba(255, 246, 212));
}

int count_files(const std::string &path)
{
    DIR *dir = opendir(path.c_str());
    if (!dir) return 0;
    int count = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.size() > 4 && name.substr(name.size() - 4) == ".png") ++count;
    }
    closedir(dir);
    return count;
}

void render_settings(SDL_Renderer *renderer, TextCache &cache, TTF_Font *title_font,
                     TTF_Font *heading_font, TTF_Font *body_font, TTF_Font *small_font,
                     AppState &app, int width, int height, bool tv, bool connected)
{
    const int margin = tv ? 28 : 18;
    draw_background(renderer, width, height, current_theme(app).accent,
                    current_theme(app).backgroundTop, current_theme(app).backgroundBottom);
    draw_text(renderer, cache, title_font, "AJUSTES", margin, tv ? 18 : 10,
              rgba(current_theme(app).text));

    const int panel_x = margin;
    const int panel_y = tv ? 100 : 78;
    const int panel_w = tv ? 300 : 220;
    const int panel_h = height - panel_y - (tv ? 78 : 58);
    draw_glass_panel(renderer, panel_x, panel_y, panel_w, panel_h, 18, 225);

    const char *sections[] = {"PERSONALIZAR", "ASSETS", "CONTROLES", "SONIDO", "VÍDEO"};
    int y = panel_y + 24;
    for (int i = 0; i < 5; ++i) {
        bool selected = i == app.settings_index;
        if (selected) {
            fill_rounded_rect(renderer, panel_x + 10, y - 8, panel_w - 20, 38, 9,
                              rgba(current_theme(app).accent, 30));
            outline_rounded_rect(renderer, panel_x + 10, y - 8, panel_w - 20, 38, 9,
                                 rgba(current_theme(app).accent, 140));
        }
        draw_text(renderer, cache, body_font, sections[i], panel_x + 22, y,
                  selected ? rgba(current_theme(app).text) : rgba(current_theme(app).muted));
        y += 48;
    }

    const int content_x = panel_x + panel_w + 24;
    const int content_w = width - content_x - margin;
    const int content_y = panel_y;
    const int content_h = panel_h;
    draw_glass_panel(renderer, content_x, content_y, content_w, content_h, 18, 210);
    draw_text(renderer, cache, heading_font, sections[app.settings_index],
              content_x + 24, content_y + 24, rgba(255, 255, 255));
    draw_text(renderer, cache, body_font,
              "Los cambios se aplican al instante. Pulsa + para guardarlos.",
              content_x + 24, content_y + 64, rgba(148, 163, 184));

    const char *hints[] = {
        "Tema, fondos y blur",
        "Carátulas y backdrops",
        "Perfiles y mapeo",
        "Efectos del menú y volumen",
        "Escalado, filtro y aspecto",
    };
    draw_text(renderer, cache, body_font, hints[app.settings_index],
              content_x + 24, content_y + 108, rgba(203, 213, 225));
    if (app.settings_index == 0) {
        draw_text(renderer, cache, body_font,
                  std::string("Tema actual: ") + current_theme(app).name,
                  content_x + 24, content_y + 148, rgba(current_theme(app).accent));
        draw_text(renderer, cache, small_font, "A  CAMBIAR TEMA",
                  content_x + 24, content_y + 176, rgba(current_theme(app).muted));
    } else if (app.settings_index == 1) {
        std::string base = join_path(join_path(app.library.sd_root, "retrou"), "thumbnails");
        std::string system = current_system_id();
        int covers = count_files(join_path(base, system));
        int backdrops = count_files(join_path(join_path(join_path(app.library.sd_root, "retrou"),
                                                        "backdrops"), system));
        draw_text(renderer, cache, body_font,
                  "Carátulas: " + std::to_string(covers),
                  content_x + 24, content_y + 148, rgba(current_theme(app).text));
        draw_text(renderer, cache, body_font,
                  "Backdrops: " + std::to_string(backdrops),
                  content_x + 24, content_y + 176, rgba(current_theme(app).text));
        draw_text(renderer, cache, small_font,
                  "Se descargan automáticamente desde Libretro.",
                  content_x + 24, content_y + 204, rgba(current_theme(app).muted));
    } else if (app.settings_index == 2) {
        const char *actionNames[] = {"A", "B", "X", "Y", "L", "R", "START", "SELECT"};
        draw_text(renderer, cache, body_font,
                  app.control_port == 0 ? "CONTROLLER 1" : "CONTROLLER 2",
                  content_x + 24, content_y + 148, rgba(current_theme(app).accent));
        std::string device = "Sin mando";
        int index = app.controller_port[app.control_port];
        if (index >= 0 && index < static_cast<int>(g_controllers.size())) {
            const char *name = SDL_GameControllerName(g_controllers[index]);
            device = name ? name : "Mando";
        }
        draw_text(renderer, cache, small_font, "Mando: " + device,
                  content_x + 24, content_y + 170, rgba(current_theme(app).text));
        int y = content_y + 194;
        int first = std::max(0, app.control_option - 5);
        for (int i = first; i < std::min(8, first + 6); ++i) {
            bool selected = i == app.control_option;
            if (selected) {
                fill_rounded_rect(renderer, content_x + 18, y - 4, content_w - 60, 26, 6,
                                  rgba(current_theme(app).accent, 40));
            }
            const char *mappedName = SDL_GameControllerGetStringForButton(
                static_cast<SDL_GameControllerButton>(app.control_maps[app.control_port][i]));
            std::string value = mappedName ? mappedName : "?";
            if (app.control_listening && selected) value = "PULSA UN BOTÓN";
            draw_text(renderer, cache, small_font,
                      std::string(actionNames[i]) + "  →  " + value,
                      content_x + 28, y, rgba(current_theme(app).text));
            y += 17;
        }
        draw_text(renderer, cache, small_font, "IZQ/DER PUERTO  Y MANDO  A REMAPEAR  B CANCELAR",
                  content_x + 24, content_y + content_h - 34, rgba(current_theme(app).muted));
    }

    draw_text(renderer, cache, small_font, "A  MODIFICAR     B / L  VOLVER     +  GUARDAR",
              content_x + 24, content_y + content_h - 34, rgba(148, 163, 184));

    if (app.dirty) {
        draw_unsaved_banner(renderer, cache, small_font, width, true);
    }
}

void settings_action(AppState &app, int action)
{
    if (action == 0) {
        if (app.settings_index == 2) {
            app.control_option = (app.control_option + 7) % 8;
            app.control_listening = false;
        } else {
            app.settings_index = (app.settings_index + 4) % 5;
        }
    } else if (action == 1) {
        if (app.settings_index == 2) {
            app.control_option = (app.control_option + 1) % 8;
            app.control_listening = false;
        } else {
            app.settings_index = (app.settings_index + 1) % 5;
        }
    } else if (action == 2 || action == 3) {
        if (app.settings_index == 2) {
            app.control_port = action == 2 ? 0 : 1;
            app.control_option = 0;
            app.control_listening = false;
        }
    } else if (action == 6) {
        if (app.settings_index == 0) {
            app.theme_index = (app.theme_index + 1) % kThemeCount;
            app.dirty = true;
            app.message = std::string("Tema: ") + current_theme(app).name;
        } else if (app.settings_index == 1) {
            if (!app.assets_downloading) {
                std::vector<RomEntry> assets;
                for (const RomEntry &rom : app.library.roms) {
                    if (rom.system == current_system_id()) assets.push_back(rom);
                }
                app.assets_downloading = true;
                std::thread(download_assets_thread, assets).detach();
                app.message = "Descargando assets en segundo plano";
            } else {
                app.message = "Descarga de assets en curso";
            }
        } else if (app.settings_index == 2) {
            app.control_listening = true;
            app.message = "Pulsa el botón físico para asignar";
        } else {
            app.dirty = true;
            app.message = "Cambio temporal: pulsa + para guardar";
        }
    } else if (action == 8) {
        if (app.settings_index == 2 && !g_controllers.empty()) {
            app.controller_port[app.control_port] =
                (app.controller_port[app.control_port] + 1) % g_controllers.size();
            select_game_controllers(app);
            app.dirty = true;
            app.message = "Mando del juego cambiado";
        }
    } else if (action == 9) {
        app.settings_screen = false;
    }
}

void keyboard_action(AppState &app, int action)
{
    static const char *keys[] = {
        "A","B","C","D","E","F","G","H","I","J",
        "K","L","M","N","O","P","Q","R","S","T",
        "U","V","W","X","Y","Z","0","1","2","3",
        "4","5","6","7","8","9"," ","DEL","OK","CANCEL"
    };
    const int cols = 10;
    const int count = 40;
    if (action == 0) app.keyboard_index = (app.keyboard_index + count - cols) % count;
    else if (action == 1) app.keyboard_index = (app.keyboard_index + cols) % count;
    else if (action == 2) app.keyboard_index = (app.keyboard_index + count - 1) % count;
    else if (action == 3) app.keyboard_index = (app.keyboard_index + 1) % count;
    else if (action == 9) app.keyboard_open = false;
    else if (action == 6) {
        int i = app.keyboard_index;
        if (i < 37) app.search_query += keys[i];
        else if (i == 37 && !app.search_query.empty()) app.search_query.pop_back();
        else if (i == 38) app.keyboard_open = false;
        else if (i == 39) {
            app.search_query.clear();
            app.keyboard_open = false;
        }
        refresh_visible(app);
    }
}

void render_keyboard(SDL_Renderer *renderer, TextCache &cache, TTF_Font *heading_font,
                     TTF_Font *body_font, TTF_Font *small_font, int width, int height,
                     const AppState &app)
{
    fill_rect(renderer, 0, 0, width, height, rgba(0, 0, 0, 210));
    int panel_w = std::min(width - 40, 760);
    int panel_x = (width - panel_w) / 2;
    int panel_y = 30;
    int panel_h = height - 60;
    draw_glass_panel(renderer, panel_x, panel_y, panel_w, panel_h, 18, 240);
    draw_text(renderer, cache, heading_font, "BUSCAR POR NOMBRE",
              panel_x + 24, panel_y + 20, rgba(current_theme(app).text));
    std::string query = app.search_query.empty() ? "_" : app.search_query + "_";
    draw_text(renderer, cache, body_font, query, panel_x + 24, panel_y + 58,
              rgba(current_theme(app).accent));

    static const char *keys[] = {
        "A","B","C","D","E","F","G","H","I","J",
        "K","L","M","N","O","P","Q","R","S","T",
        "U","V","W","X","Y","Z","0","1","2","3",
        "4","5","6","7","8","9"," ","DEL","OK","CANCEL"
    };
    int cols = 10;
    int key_w = (panel_w - 48 - (cols - 1) * 8) / cols;
    int key_h = 34;
    int start_y = panel_y + 100;
    for (int i = 0; i < 40; ++i) {
        int row = i / cols, col = i % cols;
        int x = panel_x + 24 + col * (key_w + 8);
        int y = start_y + row * (key_h + 8);
        bool selected = i == app.keyboard_index;
        fill_rounded_rect(renderer, x, y, key_w, key_h, 7,
                          selected ? rgba(current_theme(app).focus, 150)
                                   : rgba(255, 255, 255, 24));
        draw_text(renderer, cache, small_font, keys[i], x + 8, y + 9,
                  rgba(current_theme(app).text));
    }
    draw_text(renderer, cache, small_font, "A  SELECCIONAR     B  CERRAR",
              panel_x + 24, panel_y + panel_h - 28, rgba(current_theme(app).muted));
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

void open_all_controllers()
{
    g_controllers.clear();
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        if (!SDL_IsGameController(i)) continue;
        SDL_GameController *controller = SDL_GameControllerOpen(i);
        if (controller) g_controllers.push_back(controller);
    }
}

SDL_GameController *menu_controller()
{
    for (SDL_GameController *controller : g_controllers) {
        const char *name = SDL_GameControllerName(controller);
        if (name && (std::strstr(name, "GamePad") || std::strstr(name, "Gamepad"))) {
            return controller;
        }
    }
    return g_controllers.empty() ? nullptr : g_controllers[0];
}

void select_game_controllers(const AppState &app)
{
    for (int port = 0; port < 2; ++port) {
        int index = app.controller_port[port];
        g_game_port[port] = (index >= 0 && index < static_cast<int>(g_controllers.size()))
                                ? g_controllers[index] : nullptr;
    }
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
            move_selection(app, 0, -1, 5, static_cast<int>(games.size()));
            break;
        case 1: /* down */
            move_selection(app, 0, 1, 5, static_cast<int>(games.size()));
            break;
        case 2: /* left */
            move_selection(app, -1, 0, 5, static_cast<int>(games.size()));
            break;
        case 3: /* right */
            move_selection(app, 1, 0, 5, static_cast<int>(games.size()));
            break;
        case 4: /* system previous */
        case 5: /* system next */
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
#if defined(RETROU_STATIC_CORE) && defined(RETROU_CORE_SYSTEM)
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
                app.message = "Esta build no tiene core";
#endif
            }
            break;
        case 7: /* detail */
            app.message = "Detalle: proxima fase";
            break;
        case 8: /* favorite */
            if (!app.visible.empty()) {
                RomEntry *rom = const_cast<RomEntry *>(app.visible[app.game]);
                rom->favorite = !rom->favorite;
                save_favorites(app);
                refresh_visible(app);
                app.message = rom->favorite ? "Añadido a favoritos" : "Quitado de favoritos";
            }
            break;
        case 9: /* back / close sidebar */
            app.sidebar_open = false;
            break;
        default:
            break;
    }
}

struct CoreMenuState {
    bool open = false;
    bool minus = false;
    bool plus = false;
    bool latched = false;
    int selected = 0;
};

void core_menu_event(CoreMenuState &menu, const SDL_Event &event, bool &close_game)
{
    if (event.type == SDL_CONTROLLERBUTTONDOWN) {
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
            menu.minus = true;
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
            menu.plus = true;
        }

        if (menu.minus && menu.plus && !menu.open && !menu.latched) {
            menu.open = true;
            menu.selected = 0;
            menu.latched = true;
        }

        if (!menu.open) {
            return;
        }

        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_UP ||
            event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_DOWN ||
            event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_LEFT ||
            event.cbutton.button == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) {
            menu.selected = (menu.selected + 1) % 2;
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_A) {
            if (menu.selected == 1) {
                close_game = true;
            } else {
                menu.open = false;
                menu.latched = false;
            }
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
            menu.open = false;
            menu.latched = false;
        }
    } else if (event.type == SDL_CONTROLLERBUTTONUP) {
        if (event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
            menu.minus = false;
        } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_START) {
            menu.plus = false;
        }
        if (!menu.minus || !menu.plus) {
            menu.latched = false;
        }
    } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
        menu.open = !menu.open;
        menu.selected = 0;
        menu.latched = false;
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

    draw_text(renderer, cache, heading_font, std::string("MENÚ ") + RETROU_APP_NAME,
              panel_x + 24, panel_y + 20,
              rgba(255, 255, 255));
    draw_text(renderer, cache, small_font, "MINUS + PLUS  CERRAR MENÚ     A  SELECCIONAR",
              panel_x + 24, panel_y + 58, rgba(148, 163, 184));

    const char *options[] = {
        "Continuar",
        "Cerrar juego y volver a " RETROU_APP_NAME,
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
    core.setSaveDirectory(system_save_directory(app.library.sd_root));
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

    CoreMenuState menu;
    bool close_game = false;
    TextCache tv_menu_text;
    TextCache drc_menu_text;

    while (app.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                app.running = false;
                break;
            }
            core_menu_event(menu, event, close_game);
        }

        if (close_game) {
            core.unload();
            app.message = "Juego cerrado";
            return;
        }

        if (!app.running) {
            break;
        }

        if (!menu.open) {
            core.runFrame();
        }

        if (tv) {
            SDL_SetRenderDrawColor(tv, 0, 0, 0, 255);
            SDL_RenderClear(tv);
            core.present(tv, 1280, 720);
            if (menu.open) {
                render_core_menu(tv, tv_menu_text, heading_font, body_font, small_font,
                                 1280, 720, menu.selected);
            }
            SDL_RenderPresent(tv);
        }
        if (drc) {
            SDL_SetRenderDrawColor(drc, 0, 0, 0, 255);
            SDL_RenderClear(drc);
            core.present(drc, 854, 480);
            if (menu.open) {
                render_core_menu(drc, drc_menu_text, heading_font, body_font, small_font,
                                 854, 480, menu.selected);
            }
            SDL_RenderPresent(drc);
        }
        SDL_Delay(16);
    }

    core.unload();
}

#ifdef RETROU_STATIC_CORE
void run_static_core(AppState &app, const std::string &rom_path,
                     SDL_GameController *controller,
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
    core.setSaveDirectory(system_save_directory(app.library.sd_root));
    core.setController(controller);
    core.setControllers(g_game_port[0], g_game_port[1]);
    core.setControlMaps(app.control_maps);
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

    CoreMenuState menu;
    bool close_game = false;
    TextCache tv_menu_text;
    TextCache drc_menu_text;

    while (app.running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                app.running = false;
                break;
            }
            core_menu_event(menu, event, close_game);
        }

        if (close_game) {
            core.unload();
            app.message = "Juego cerrado";
            return;
        }

        if (!app.running) {
            break;
        }
        if (!menu.open) {
            core.runFrame();
        }

        if (tv) {
            SDL_SetRenderDrawColor(tv, 0, 0, 0, 255);
            SDL_RenderClear(tv);
            core.present(tv, 1280, 720);
            if (menu.open) {
                render_core_menu(tv, tv_menu_text, heading_font, body_font, small_font,
                                 1280, 720, menu.selected);
            }
            SDL_RenderPresent(tv);
        }
        if (drc) {
            SDL_SetRenderDrawColor(drc, 0, 0, 0, 255);
            SDL_RenderClear(drc);
            core.present(drc, 854, 480);
            if (menu.open) {
                render_core_menu(drc, drc_menu_text, heading_font, body_font, small_font,
                                 854, 480, menu.selected);
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
    run_static_core(app, "", nullptr, tv, drc, heading_font, body_font, small_font);
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

    auto boot_log = [](const char *stage) {
        std::ofstream out("/vol/external01/retrou/boot.log", std::ios::app);
        if (out) out << stage << "\n";
    };
    boot_log("main start");

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS) != 0) {
        boot_log("SDL_Init failed");
        return 1;
    }
    boot_log("SDL_Init ok");
    SDL_GameControllerEventState(SDL_ENABLE);

    SDL_Window *tv_window = SDL_CreateWindow(
        RETROU_APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_WIIU_TV_ONLY);
    SDL_Window *drc_window = SDL_CreateWindow(
        RETROU_APP_NAME, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 854, 480,
        SDL_WINDOW_SHOWN | SDL_WINDOW_WIIU_GAMEPAD_ONLY);

    SDL_Renderer *tv_renderer = tv_window ? make_renderer(tv_window) : nullptr;
    SDL_Renderer *drc_renderer = drc_window ? make_renderer(drc_window) : nullptr;

    if (!tv_renderer && !drc_renderer) {
        boot_log("no renderers");
        SDL_Quit();
        return 1;
    }
    boot_log("windows/renderers ok");

    if (TTF_Init() != 0) {
        SDL_Quit();
        return 1;
    }

    TTF_Font *title_font = open_font(true, 34);
    TTF_Font *heading_font = open_font(true, 24);
    TTF_Font *body_font = open_font(false, 18);
    TTF_Font *small_font = open_font(false, 13);

    if (!title_font || !heading_font || !body_font || !small_font) {
        boot_log("font load failed");
        if (title_font) TTF_CloseFont(title_font);
        if (heading_font) TTF_CloseFont(heading_font);
        if (body_font) TTF_CloseFont(body_font);
        if (small_font) TTF_CloseFont(small_font);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    AppState app;
    boot_log("before load_library");
    app.library = load_library();
    boot_log("after load_library");
    ensure_system_dirs(app.library.sd_root);
    boot_log("after ensure_dirs");
#if defined(RETROU_CORE_SYSTEM)
    app.system = console_index_by_id(RETROU_CORE_SYSTEM);
#endif
    rebuild_indexes(app);
    load_system_config(app);
    load_theme(app);
    load_control_config(app);
    load_favorites(app);
    load_libretro_metadata(app);
    boot_log("after libretro_metadata");
    load_metadata(app);
    boot_log("after load_metadata");
    refresh_visible(app);
    boot_log("after refresh_visible");
    if (app.library.roms.empty()) {
        app.message = "Copia ROMs a sd:/retrou/roms/<consola>/";
    }

    SDL_GameController *controller = nullptr;
    open_all_controllers();
    controller = menu_controller();
    select_game_controllers(app);
    boot_log("after open controller");
    bool connected = controller != nullptr;

    TextCache tv_text;
    TextCache drc_text;
    boot_log("after text caches");
    bool ui_dirty = true;
    std::string last_message;
    Uint32 message_set_at = 0;

    auto touch_grid = [&](int tx, int ty, int w, int h) {
        if (app.settings_screen || app.sidebar_open || app.keyboard_open || app.visible.empty()) {
            return;
        }
        const int margin = w >= 1000 ? 28 : 18;
        const int header_h = w >= 1000 ? 84 : 64;
        const int footer_h = w >= 1000 ? 62 : 46;
        const int gap = w >= 1000 ? 14 : 9;
        const int columns = 5;
        const int rows = w >= 1000 ? 3 : 2;
        const int content_x = margin;
        const int content_y = header_h + 8;
        const int content_w = w - margin * 2;
        const int content_h = h - content_y - footer_h - margin;
        int card_w = (content_w - gap * (columns - 1)) / columns;
        int card_h = (content_h - gap * (rows - 1)) / rows;
        if (tx < content_x || ty < content_y) return;
        int col = (tx - content_x) / (card_w + gap);
        int row = (ty - content_y) / (card_h + gap);
        if (col < 0 || col >= columns || row < 0 || row >= rows) return;
        int index = (app.scroll_row + row) * columns + col;
        if (index < 0 || index >= static_cast<int>(app.visible.size())) return;
        if (index == app.game) {
            handle_action(app, 6);
        } else {
            app.game = index;
            app.tooltip_shown_focus = -1;
            handle_action(app, 6);
        }
    };
    boot_log("before main loop");

    while (app.running) {
        if (!controller) {
            bool was_connected = connected;
            open_all_controllers();
            controller = menu_controller();
            select_game_controllers(app);
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
                SDL_JoystickID menu_id = controller
                    ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) : -1;
                SDL_JoystickID pad_id = g_game_port[0]
                    ? SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(g_game_port[0])) : -1;
                if (event.cbutton.which != menu_id && event.cbutton.which != pad_id) {
                    continue;
                }
                if (app.control_listening && app.settings_screen) {
                    if (event.cbutton.button == SDL_CONTROLLER_BUTTON_B) {
                        app.control_listening = false;
                        app.message = "Remapeo cancelado";
                    } else {
                        app.control_maps[app.control_port][app.control_option] = event.cbutton.button;
                        app.control_listening = false;
                        app.dirty = true;
                        app.message = "Botón asignado";
                    }
                    ui_dirty = true;
                    continue;
                }
                auto dispatch = [&](int action) {
                    if (app.keyboard_open) {
                        keyboard_action(app, action);
                    } else if (app.settings_screen) {
                        settings_action(app, action);
                    } else if (app.sidebar_open) {
                        sidebar_action(app, action);
                    } else {
                        handle_action(app, action);
                    }
                };
                switch (event.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: dispatch(0); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: dispatch(1); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: dispatch(2); break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: dispatch(3); break;
                    case SDL_CONTROLLER_BUTTON_A: dispatch(6); break;
                    case SDL_CONTROLLER_BUTTON_B:
                        if (app.control_listening) app.control_listening = false;
                        else if (app.keyboard_open) app.keyboard_open = false;
                        else if (app.settings_screen) {
                            app.settings_screen = false;
                            app.message.clear();
                        } else {
                            dispatch(9);
                        }
                        break;
                    case SDL_CONTROLLER_BUTTON_X: dispatch(7); break;
                    case SDL_CONTROLLER_BUTTON_Y: dispatch(8); break;
                    case SDL_CONTROLLER_BUTTON_BACK:
                        if (!app.settings_screen && !app.sidebar_open) {
                            app.sort_mode = (app.sort_mode + 1) % 5;
                            refresh_visible(app);
                            save_system_config(app);
                        }
                        break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                        if (app.settings_screen) {
                            app.settings_screen = false;
                            app.message.clear();
                        }
                        break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                        if (!app.settings_screen) {
                            app.sidebar_open = false;
                            app.settings_screen = true;
                            app.message.clear();
                        }
                        break;
                    case SDL_CONTROLLER_BUTTON_START:
                        if (app.settings_screen) {
                            app.dirty = false;
                            save_theme(app);
                            save_control_config(app);
                            app.message = "Cambios guardados";
                        } else {
                            app.sidebar_open = !app.sidebar_open;
                            app.sidebar_index = 0;
                        }
                        break;
                    default: break;
                }
            }
            if (event.type == SDL_KEYDOWN) {
                auto dispatch = [&](int action) {
                    if (app.keyboard_open) {
                        keyboard_action(app, action);
                    } else if (app.settings_screen) {
                        settings_action(app, action);
                    } else if (app.sidebar_open) {
                        sidebar_action(app, action);
                    } else {
                        handle_action(app, action);
                    }
                };
                switch (event.key.keysym.sym) {
                    case SDLK_UP: dispatch(0); break;
                    case SDLK_DOWN: dispatch(1); break;
                    case SDLK_LEFT: dispatch(2); break;
                    case SDLK_RIGHT: dispatch(3); break;
                    case SDLK_RETURN: dispatch(6); break;
                    case SDLK_q:
                        if (app.settings_screen) app.settings_screen = false;
                        break;
                    case SDLK_e:
                        if (!app.settings_screen) {
                            app.sidebar_open = false;
                            app.settings_screen = true;
                        }
                        break;
                    case SDLK_ESCAPE: app.running = false; break;
                    default: break;
                }
            }
            if (event.type == SDL_FINGERDOWN) {
                int w = 1280, h = 720;
                if (event.tfinger.windowID == SDL_GetWindowID(drc_window)) {
                    w = 854; h = 480;
                }
                touch_grid(static_cast<int>(event.tfinger.x * w),
                           static_cast<int>(event.tfinger.y * h), w, h);
            }
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                touch_grid(event.button.x, event.button.y, 1280, 720);
            }
        }

        Uint32 now = SDL_GetTicks();
        if (app.game != app.tooltip_focus) {
            app.tooltip_focus = app.game;
            app.tooltip_shown_focus = -1;
            app.focus_since = now;
            ui_dirty = true;
        }
        if (!app.visible.empty() && app.tooltip_shown_focus != app.game &&
            now - app.focus_since >= 5000) {
            app.tooltip_shown_focus = app.game;
            ui_dirty = true;
        }
        if (app.message != last_message) {
            last_message = app.message;
            message_set_at = now;
        }
        if (!app.message.empty() && now - message_set_at > 2500) {
            app.message.clear();
            last_message.clear();
            ui_dirty = true;
        }

        if (app.pending_rpl) {
            app.pending_rpl = false;
            std::string core_path = app.rpl_core;
            std::string rom_path = app.rpl_rom;
            app.rpl_core.clear();
            app.rpl_rom.clear();
#ifdef RETROU_STATIC_CORE
            run_static_core(app, rom_path, controller, tv_renderer, drc_renderer,
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
            if (app.settings_screen) {
                render_settings(tv_renderer, tv_text, title_font, heading_font, body_font,
                                small_font, app, 1280, 720, true, connected);
            } else {
                render_screen(tv_renderer, tv_text, title_font, heading_font, body_font, small_font,
                              app, 1280, 720, true, connected);
                if (app.sidebar_open) {
                    render_sidebar(tv_renderer, tv_text, heading_font, body_font, small_font,
                                   1280, 720, app);
                }
                if (app.keyboard_open) {
                    render_keyboard(tv_renderer, tv_text, heading_font, body_font, small_font,
                                    1280, 720, app);
                }
            }
            SDL_RenderPresent(tv_renderer);
        }
        if (drc_renderer) {
            SDL_SetRenderDrawColor(drc_renderer, 8, 13, 24, 255);
            SDL_RenderClear(drc_renderer);
            if (app.settings_screen) {
                render_settings(drc_renderer, drc_text, title_font, heading_font, body_font,
                                small_font, app, 854, 480, false, connected);
            } else {
                render_screen(drc_renderer, drc_text, title_font, heading_font, body_font, small_font,
                              app, 854, 480, false, connected);
                if (app.sidebar_open) {
                    render_sidebar(drc_renderer, drc_text, heading_font, body_font, small_font,
                                   854, 480, app);
                }
                if (app.keyboard_open) {
                    render_keyboard(drc_renderer, drc_text, heading_font, body_font, small_font,
                                    854, 480, app);
                }
            }
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

    SDL_Quit();
    return 0;
}
