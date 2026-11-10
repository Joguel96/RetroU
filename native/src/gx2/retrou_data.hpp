#pragma once

#include <string>
#include <vector>

namespace retrou {

struct RomEntry {
    std::string title; // file name without extension
    std::string path;
    std::string system;
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

} // namespace retrou
