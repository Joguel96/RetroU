#include "retrou_assets.hpp"

#include <curl/curl.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <sys/stat.h>

namespace retrou::assets {

namespace {

void ensureDir(const std::string &dir)
{
    mkdir(dir.c_str(), 0777);
}

void assetLog(const std::string &sdRoot, const std::string &message)
{
    std::ofstream out(sdRoot + "/retrou/retrou_rpx.log", std::ios::app);
    if (out) out << "GX2 asset: " << message << "\n";
}

bool replaceDownloadedFile(const std::string &temporary, const std::string &destination)
{
    if (std::rename(temporary.c_str(), destination.c_str()) == 0) return true;
    struct stat existing{};
    if (stat(destination.c_str(), &existing) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    const std::string backup = destination + ".bak";
    struct stat priorBackup{};
    if (stat(backup.c_str(), &priorBackup) == 0) {
        std::remove(temporary.c_str());
        return false;
    }
    if (std::rename(destination.c_str(), backup.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    if (std::rename(temporary.c_str(), destination.c_str()) == 0) {
        std::remove(backup.c_str());
        return true;
    }
    std::rename(backup.c_str(), destination.c_str());
    std::remove(temporary.c_str());
    return false;
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
        if (std::string("&*/:`<>?\\|\"").find(c) != std::string::npos) {
            c = '_';
        }
    }
    return title;
}

size_t curlWriteFile(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    return std::fwrite(ptr, size, nmemb, static_cast<FILE *>(userdata));
}

} // namespace

std::string libretroSystemFolder(const std::string &system)
{
    if (system == "nes") return "Nintendo - Nintendo Entertainment System";
    if (system == "snes") return "Nintendo - Super Nintendo Entertainment System";
    if (system == "gba") return "Nintendo - Game Boy Advance";
    return "";
}

bool downloadToFile(const std::string &url, const std::string &path)
{
    FILE *fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        return false;
    }
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
    if (!ok) {
        std::remove(path.c_str());
    }
    return ok;
}

bool downloadIfMissing(const std::string &sdRoot, const std::string &system,
                       const std::string &title, const char *kind, bool force)
{
    const std::string folder = libretroSystemFolder(system);
    if (folder.empty()) {
        return false;
    }
    const std::string root = sdRoot + "/retrou/" + kind;
    ensureDir(root);
    const std::string dir = root + "/" + system;
    ensureDir(dir);
    const std::string safe = libretroName(title);
    const std::string dst = dir + "/" + safe + ".png";
    struct stat info;
    if (!force && stat(dst.c_str(), &info) == 0) {
        return true;
    }
    const std::string temporary = dst + ".tmp";

    const char *type = std::string(kind) == "thumbnails" ? "Named_Boxarts" : "Named_Snaps";
    const std::string base = "https://thumbnails.libretro.com/" + urlEncode(folder) +
                             "/" + type + "/";
    const char *suffixes[] = {"", " (USA)", " (Europe)", " (Japan)", " (World)"};
    for (const char *suffix : suffixes) {
        const std::string url = base + urlEncode(safe + suffix) + ".png";
        if (downloadToFile(url, temporary)) {
            if (replaceDownloadedFile(temporary, dst)) return true;
        }
    }
    assetLog(sdRoot, "fallo " + std::string(kind) + " " + system + " / " + title);
    return false;
}

void downloadMetadata(const std::string &sdRoot, const std::string &system,
                      std::atomic<int> *progress)
{
    const std::string folder = libretroSystemFolder(system);
    if (folder.empty()) {
        return;
    }
    const std::string dir = sdRoot + "/retrou/metadata";
    ensureDir(dir);
    const char *kinds[] = {"developer", "genre", "releaseyear"};
    for (const char *kind : kinds) {
        const std::string path = dir + "/" + system + "-" + kind + ".dat";
        const std::string url =
            "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/" +
            std::string(kind) + "/" + urlEncode(folder) + ".dat";
        const std::string temporary = path + ".tmp";
        if (!downloadToFile(url, temporary) || !replaceDownloadedFile(temporary, path))
            assetLog(sdRoot, "fallo metadata " + system + " / " + kind);
        if (progress) ++(*progress);
    }
}

void downloadAll(const std::string &sdRoot, const std::string &system,
                 const std::vector<std::string> &titles, bool covers, bool backdrops,
                 bool force, std::atomic<int> *progress)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    assetLog(sdRoot, "inicio " + system + " roms=" + std::to_string(titles.size()) +
                     " portadas=" + std::to_string(covers) +
                     " fondos=" + std::to_string(backdrops) +
                     " forzar=" + std::to_string(force));
    downloadMetadata(sdRoot, system, progress);
    int failures = 0;
    for (const std::string &title : titles) {
        if (covers) {
            if (!downloadIfMissing(sdRoot, system, title, "thumbnails", force)) ++failures;
            if (progress) ++(*progress);
        }
        if (backdrops) {
            if (!downloadIfMissing(sdRoot, system, title, "backdrops", force)) ++failures;
            if (progress) ++(*progress);
        }
    }
    assetLog(sdRoot, "fin " + system + " fallos=" + std::to_string(failures));
    curl_global_cleanup();
}

} // namespace retrou::assets
