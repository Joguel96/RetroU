#include "retrou_assets.hpp"

#include <curl/curl.h>

#include <cctype>
#include <cstdio>
#include <sys/stat.h>

namespace retrou::assets {

namespace {

void ensureDir(const std::string &dir)
{
    mkdir(dir.c_str(), 0777);
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
            if (std::rename(temporary.c_str(), dst.c_str()) == 0) return true;
            std::remove(temporary.c_str());
        }
    }
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
        if (downloadToFile(url, temporary)) {
            if (std::rename(temporary.c_str(), path.c_str()) != 0)
                std::remove(temporary.c_str());
        }
        if (progress) ++(*progress);
    }
}

void downloadAll(const std::string &sdRoot, const std::string &system,
                 const std::vector<std::string> &titles, bool covers, bool backdrops,
                 bool force, std::atomic<int> *progress)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    downloadMetadata(sdRoot, system, progress);
    for (const std::string &title : titles) {
        if (covers) {
            downloadIfMissing(sdRoot, system, title, "thumbnails", force);
            if (progress) ++(*progress);
        }
        if (backdrops) {
            downloadIfMissing(sdRoot, system, title, "backdrops", force);
            if (progress) ++(*progress);
        }
    }
    curl_global_cleanup();
}

} // namespace retrou::assets
