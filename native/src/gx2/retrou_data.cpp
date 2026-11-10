#include "retrou_data.hpp"

#include <whb/sdcard.h>

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <fstream>

namespace retrou {

namespace {

std::string joinPath(const std::string &base, const std::string &name)
{
    if (base.empty()) {
        return name;
    }
    if (base.back() == '/' || base.back() == ':') {
        return base + name;
    }
    return base + "/" + name;
}

bool endsWithNoCase(const std::string &value, const std::string &suffix)
{
    if (value.size() < suffix.size()) {
        return false;
    }
    std::string tail = value.substr(value.size() - suffix.size());
    for (char &c : tail) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return tail == suffix;
}

bool isRomExtension(const std::string &name)
{
    static const char *extensions[] = {
        ".nes", ".fds", ".unf", ".sfc", ".smc", ".fig", ".n64", ".z64",
        ".v64", ".md", ".gen", ".smd", ".32x", ".gba", ".gb", ".gbc",
    };
    for (const char *extension : extensions) {
        if (endsWithNoCase(name, extension)) {
            return true;
        }
    }
    return false;
}

std::string stripExtension(const std::string &name)
{
    const size_t dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

} // namespace

std::string mountSd()
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

void scanRoms(Library &library, const std::string &system)
{
    library.roms.clear();
    const std::string romRoot =
        joinPath(joinPath(joinPath(library.sd_root, "retrou"), "roms"), system);

    DIR *dir = opendir(romRoot.c_str());
    if (!dir) {
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name = entry->d_name;
        if (name.empty() || name[0] == '.') {
            continue;
        }
        if (!isRomExtension(name)) {
            continue;
        }
        RomEntry rom;
        rom.title = stripExtension(name);
        rom.path = joinPath(romRoot, name);
        rom.system = system;
        library.roms.push_back(rom);
    }
    closedir(dir);

    std::sort(library.roms.begin(), library.roms.end(),
              [](const RomEntry &a, const RomEntry &b) {
                  return a.title < b.title;
              });
}

std::string coverPath(const std::string &sdRoot, const std::string &system,
                      const std::string &title)
{
    if (sdRoot.empty()) {
        return {};
    }
    std::string safe = title;
    for (char &c : safe) {
        if (c == '/' || c == '\\') {
            c = '_';
        }
    }
    return joinPath(joinPath(joinPath(sdRoot, "retrou"), "thumbnails"), system) +
           "/" + safe + ".png";
}

std::string favoritesPath(const std::string &sdRoot, const std::string &system)
{
    return joinPath(joinPath(joinPath(sdRoot, "retrou"), "config"), system) +
           "/favorites.conf";
}

void loadFavorites(Library &library, const std::string &system)
{
    std::ifstream in(favoritesPath(library.sd_root, system));
    std::string path;
    while (std::getline(in, path)) {
        for (RomEntry &rom : library.roms) {
            if (rom.path == path) {
                rom.favorite = true;
            }
        }
    }
}

void saveFavorites(const Library &library, const std::string &system)
{
    std::ofstream out(favoritesPath(library.sd_root, system), std::ios::trunc);
    if (!out) {
        return;
    }
    for (const RomEntry &rom : library.roms) {
        if (rom.favorite) {
            out << rom.path << "\n";
        }
    }
}

} // namespace retrou
