#pragma once

#include <string>
#include <vector>

namespace retrou {

struct RomEntry {
    std::string title; // file name without extension
    std::string path;
    std::string system;
    bool favorite = false;
};

struct Library {
    std::string sd_root;
    std::vector<RomEntry> roms;
};

// Mounts the SD card and returns its root path (e.g. "/vol/external01").
std::string mountSd();

// Scans sd:/retrou/roms/<system>/ for ROMs into `library`.
void scanRoms(Library &library, const std::string &system);

// Returns the local cover PNG path for a game title, or empty on no SD.
std::string coverPath(const std::string &sdRoot, const std::string &system,
                      const std::string &title);

// Favorites config path (one ROM path per line).
std::string favoritesPath(const std::string &sdRoot, const std::string &system);

// Marks RomEntry::favorite from the favorites file.
void loadFavorites(Library &library, const std::string &system);

// Writes the current favorites to disk.
void saveFavorites(const Library &library, const std::string &system);

} // namespace retrou
