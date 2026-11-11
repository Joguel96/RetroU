#pragma once

#include <string>
#include <vector>

namespace retrou::assets {

// Maps an internal system id (nes/snes/gba) to the libretro-database folder name.
std::string libretroSystemFolder(const std::string &system);

// Downloads `url` into `path`. Returns true on success (removes partial files).
bool downloadToFile(const std::string &url, const std::string &path);

// Downloads a thumbnail (Named_Boxarts) or backdrop (Named_Snaps) for the title
// if it is not already present. `kind` is "thumbnails" or "backdrops".
bool downloadIfMissing(const std::string &sdRoot, const std::string &system,
                       const std::string &title, const char *kind);

// Downloads the libretro-database metadata .dat files (developer/genre/year).
void downloadMetadata(const std::string &sdRoot, const std::string &system);

// Downloads thumbnails + backdrops + metadata for every given title.
void downloadAll(const std::string &sdRoot, const std::string &system,
                 const std::vector<std::string> &titles);

} // namespace retrou::assets
