#pragma once

#include <string>
#include <vector>
#include <atomic>

namespace retrou::assets {

// Maps an internal system id (nes/snes/gba) to the libretro-database folder name.
std::string libretroSystemFolder(const std::string &system);

// Downloads `url` into `path`. Returns true on success (removes partial files).
bool downloadToFile(const std::string &url, const std::string &path,
                    std::string *error = nullptr,
                    const std::atomic<bool> *cancel = nullptr);

// Downloads a thumbnail (Named_Boxarts) or backdrop (Named_Snaps) for the title
// if it is not already present. `kind` is "thumbnails" or "backdrops".
bool downloadIfMissing(const std::string &sdRoot, const std::string &system,
                       const std::string &title, const char *kind, bool force = false);

// Number of images that this job still needs to request. Metadata is excluded.
int pendingAssetCount(const std::string &sdRoot, const std::string &system,
                      const std::vector<std::string> &titles, bool covers,
                      bool backdrops, bool force);

// Downloads the libretro-database metadata .dat files (developer/genre/year).
void downloadMetadata(const std::string &sdRoot, const std::string &system,
                      std::atomic<int> *progress = nullptr,
                      const std::atomic<bool> *cancel = nullptr);

// Downloads missing thumbnails/backdrops (or all when forced) plus metadata.
// Progress counts image requests only, matching pendingAssetCount().
void downloadAll(const std::string &sdRoot, const std::string &system,
                 const std::vector<std::string> &titles, bool covers, bool backdrops,
                 bool force, std::atomic<int> *progress = nullptr,
                 std::atomic<int> *failed = nullptr,
                 const std::atomic<bool> *cancel = nullptr);

} // namespace retrou::assets
