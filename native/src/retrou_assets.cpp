#include "retrou_assets.hpp"
#include "asset_names.hpp"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
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
    return assetFileStem(title);
}

std::string localAssetPath(const std::string &sdRoot, const std::string &system,
                           const std::string &title, const char *kind)
{
    return sdRoot + "/retrou/" + kind + "/" + system + "/" +
           libretroName(title) + ".png";
}

bool validPngFile(const std::string &path)
{
    struct stat info{};
    if (stat(path.c_str(), &info) != 0 || info.st_size < 33) return false;
    std::ifstream input(path, std::ios::binary);
    unsigned char header[33]{};
    input.read(reinterpret_cast<char *>(header), sizeof(header));
    static const unsigned char signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    return input.gcount() == sizeof(header) &&
           std::memcmp(header, signature, sizeof(signature)) == 0 &&
           header[8] == 0 && header[9] == 0 && header[10] == 0 &&
           header[11] == 13 && std::memcmp(header + 12, "IHDR", 4) == 0 &&
           (header[16] | header[17] | header[18] | header[19]) != 0 &&
           (header[20] | header[21] | header[22] | header[23]) != 0;
}

bool assetExists(const std::string &sdRoot, const std::string &system,
                 const std::string &title, const char *kind)
{
    return validPngFile(localAssetPath(sdRoot, system, title, kind));
}

std::string stripLegacyTags(std::string title)
{
    while (!title.empty()) {
        while (!title.empty() && title.back() == ' ') title.pop_back();
        if (title.empty() || title.back() != ']') break;
        const size_t open = title.find_last_of('[');
        if (open == std::string::npos || title.size() - open > 12) break;
        title.erase(open);
    }
    return title;
}

std::string matchKey(const std::string &name, bool catalog)
{
    std::string out;
    const size_t limit = catalog ? name.find(" (") : name.size();
    for (size_t i = 0; i < std::min(limit, name.size()); ++i) {
        unsigned char c = static_cast<unsigned char>(name[i]);
        if (c == 0xC3 && i + 1 < name.size()) {
            const unsigned char next = static_cast<unsigned char>(name[++i]);
            if ((next >= 0x80 && next <= 0x85) || (next >= 0xA0 && next <= 0xA5)) c = 'a';
            else if ((next >= 0x88 && next <= 0x8B) || (next >= 0xA8 && next <= 0xAB)) c = 'e';
            else if ((next >= 0x8C && next <= 0x8F) || (next >= 0xAC && next <= 0xAF)) c = 'i';
            else if ((next >= 0x92 && next <= 0x96) || (next >= 0xB2 && next <= 0xB6)) c = 'o';
            else if ((next >= 0x99 && next <= 0x9C) || (next >= 0xB9 && next <= 0xBC)) c = 'u';
            else if (next == 0x91 || next == 0xB1) c = 'n';
            else continue;
        }
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (!out.empty() && out.back() != ' ') {
            out.push_back(' ');
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string decodeHref(const std::string &href)
{
    std::string out;
    for (size_t i = 0; i < href.size(); ++i) {
        if (href[i] == '%' && i + 2 < href.size() &&
            std::isxdigit(static_cast<unsigned char>(href[i + 1])) &&
            std::isxdigit(static_cast<unsigned char>(href[i + 2]))) {
            unsigned value = 0;
            std::sscanf(href.c_str() + i + 1, "%2x", &value);
            out.push_back(static_cast<char>(value));
            i += 2;
        } else if (href.compare(i, 5, "&amp;") == 0) {
            out.push_back('&');
            i += 4;
        } else {
            out.push_back(href[i]);
        }
    }
    return out;
}

bool readJsonString(const std::string &json, size_t &pos, std::string &out)
{
    if (pos >= json.size() || json[pos++] != '"') return false;
    out.clear();
    while (pos < json.size()) {
        const char c = json[pos++];
        if (c == '"') return true;
        if (c != '\\') { out.push_back(c); continue; }
        if (pos >= json.size()) return false;
        const char escaped = json[pos++];
        if (escaped == 'u') {
            if (pos + 4 > json.size()) return false;
            unsigned codepoint = 0;
            for (int i = 0; i < 4; ++i) {
                const char hex = json[pos++];
                codepoint <<= 4;
                if (hex >= '0' && hex <= '9') codepoint |= hex - '0';
                else if (hex >= 'a' && hex <= 'f') codepoint |= hex - 'a' + 10;
                else if (hex >= 'A' && hex <= 'F') codepoint |= hex - 'A' + 10;
                else return false;
            }
            if (codepoint < 0x80) out.push_back(static_cast<char>(codepoint));
            else if (codepoint < 0x800) {
                out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
                out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            } else {
                out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
                out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
                out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
            }
        } else {
            if (escaped == 'n') out.push_back('\n');
            else if (escaped == 'r') out.push_back('\r');
            else if (escaped == 't') out.push_back('\t');
            else out.push_back(escaped);
        }
    }
    return false;
}

struct CatalogEntry {
    std::string href;
    std::string name;
    std::string key;
};

std::vector<CatalogEntry> loadCatalog(const std::string &sdRoot,
                                      const std::string &system, const char *type,
                                      const std::atomic<bool> *cancel, bool refresh)
{
    const std::string dir = sdRoot + "/retrou/metadata";
    ensureDir(dir);
    const bool github = system == "n64";
    const std::string path = github ? dir + "/n64-libretro-tree.json" :
        dir + "/" + system + "-" + type + "-index.html";
    const std::string temporary = path + ".tmp";
    const std::string url = github ?
        "https://api.github.com/repos/libretro-thumbnails/Nintendo_-_Nintendo_64/git/trees/master?recursive=1" :
        "https://thumbnails.libretro.com/" +
        urlEncode(libretroSystemFolder(system)) + "/" + type + "/";
    std::string error;
    if (refresh) {
        if (downloadToFile(url, temporary, &error, cancel)) {
            if (!replaceDownloadedFile(temporary, path))
                assetLog(sdRoot, "fallo guardar indice " + std::string(type));
        } else {
            assetLog(sdRoot, "fallo indice " + std::string(type) + " " + error);
        }
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    const std::string html((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    std::vector<CatalogEntry> entries;
    if (github) {
        if (html.find("\"truncated\":true") != std::string::npos) {
            assetLog(sdRoot, "indice GitHub incompleto");
            return {};
        }
        size_t pos = 0;
        while ((pos = html.find("\"path\":", pos)) != std::string::npos) {
            pos += 7;
            while (pos < html.size() && std::isspace(static_cast<unsigned char>(html[pos]))) ++pos;
            std::string entryPath;
            if (!readJsonString(html, pos, entryPath)) break;
            const std::string prefix = std::string(type) + "/";
            if (entryPath.compare(0, prefix.size(), prefix) != 0 ||
                entryPath.size() < prefix.size() + 5 ||
                entryPath.compare(entryPath.size() - 4, 4, ".png") != 0 ||
                entryPath.find('/', prefix.size()) != std::string::npos) continue;
            const std::string name = entryPath.substr(prefix.size(),
                                                       entryPath.size() - prefix.size() - 4);
            entries.push_back({urlEncode(name) + ".png", name, matchKey(name, true)});
        }
        assetLog(sdRoot, "indice " + std::string(type) + " entradas=" +
                         std::to_string(entries.size()));
        return entries;
    }
    size_t pos = 0;
    while ((pos = html.find("href=\"", pos)) != std::string::npos) {
        pos += 6;
        const size_t end = html.find('"', pos);
        if (end == std::string::npos) break;
        std::string href = html.substr(pos, end - pos);
        pos = end + 1;
        size_t entity = 0;
        while ((entity = href.find("&amp;", entity)) != std::string::npos)
            href.replace(entity, 5, "&");
        const std::string decoded = decodeHref(href);
        if (decoded.size() < 5 || decoded.compare(decoded.size() - 4, 4, ".png") != 0 ||
            decoded.find('/') != std::string::npos) continue;
        const std::string name = decoded.substr(0, decoded.size() - 4);
        entries.push_back({href, name, matchKey(name, true)});
    }
    assetLog(sdRoot, "indice " + std::string(type) + " entradas=" +
                     std::to_string(entries.size()));
    return entries;
}

std::vector<const CatalogEntry *> findCatalogMatches(const std::vector<CatalogEntry> &entries,
                                                      const std::string &title)
{
    std::string clean = stripLegacyTags(title);
    // The user's GoldenEye dump uses the cartridge's alternate title order.
    if (clean == "007 - GoldenEye") clean = "GoldenEye 007";
    const std::string key = matchKey(clean, true);
    if (key.size() < 4) return {};
    const bool europe = title.find("[E]") != std::string::npos ||
                        title.find("(E)") != std::string::npos ||
                        title.find("(Europe)") != std::string::npos;
    const bool usa = title.find("[U]") != std::string::npos ||
                     title.find("(U)") != std::string::npos ||
                     title.find("(USA)") != std::string::npos;
    std::vector<std::pair<int, const CatalogEntry *>> scored;
    for (const CatalogEntry &entry : entries) {
        int score = 0;
        if (entry.key == key) score = 1000;
        else if (key.size() >= 7 && entry.key.size() > key.size() &&
                 entry.key.compare(0, key.size(), key) == 0 &&
                 entry.key[key.size()] == ' ') score = 500;
        if (!score) continue;
        if (europe && entry.name.find("(Europe)") != std::string::npos) score += 100;
        if (usa && entry.name.find("(USA)") != std::string::npos) score += 100;
        if (entry.name == clean) score += 500;
        if (entry.name.find("Virtual Console") != std::string::npos) score -= 100;
        for (const char *variant : {"(Beta", "(Proto", "(Demo", "(Rev", "(Sample"})
            if (entry.name.find(variant) != std::string::npos &&
                clean.find(variant) == std::string::npos) score -= 150;
        scored.push_back({score, &entry});
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto &a, const auto &b) { return a.first > b.first; });
    std::vector<const CatalogEntry *> matches;
    for (const auto &candidate : scored) matches.push_back(candidate.second);
    return matches;
}

size_t curlWriteFile(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    return std::fwrite(ptr, size, nmemb, static_cast<FILE *>(userdata));
}

int curlCancelTransfer(void *userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
    return static_cast<const std::atomic<bool> *>(userdata)->load() ? 1 : 0;
}

} // namespace

std::string libretroSystemFolder(const std::string &system)
{
    if (system == "nes") return "Nintendo - Nintendo Entertainment System";
    if (system == "snes") return "Nintendo - Super Nintendo Entertainment System";
    if (system == "gba") return "Nintendo - Game Boy Advance";
    if (system == "n64") return "Nintendo - Nintendo 64";
    return "";
}

bool downloadToFile(const std::string &url, const std::string &path,
                    std::string *error, const std::atomic<bool> *cancel)
{
    if (cancel && cancel->load()) {
        if (error) *error = "cancelado";
        return false;
    }
    FILE *fp = std::fopen(path.c_str(), "wb");
    if (!fp) {
        if (error) *error = std::strerror(errno);
        return false;
    }
    CURL *curl = curl_easy_init();
    bool ok = false;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "RetroU/1.0");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 45L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteFile);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        if (cancel) {
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlCancelTransfer);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cancel);
        }
        const CURLcode result = curl_easy_perform(curl);
        long http = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
        ok = result == CURLE_OK;
        if (!ok && error) {
            *error = std::string(curl_easy_strerror(result)) +
                     " http=" + std::to_string(http);
        }
        curl_easy_cleanup(curl);
    } else if (error) {
        *error = "curl_easy_init";
    }
    std::fclose(fp);
    if (!ok) {
        std::remove(path.c_str());
    }
    return ok;
}

static bool downloadWithCatalog(const std::string &sdRoot, const std::string &system,
                                const std::string &title, const char *kind, bool force,
                                const std::vector<CatalogEntry> *catalog,
                                const std::atomic<bool> *cancel)
{
    if (cancel && cancel->load()) return false;
    const std::string folder = libretroSystemFolder(system);
    if (folder.empty()) {
        return false;
    }
    const std::string root = sdRoot + "/retrou/" + kind;
    ensureDir(root);
    const std::string dir = root + "/" + system;
    ensureDir(dir);
    const std::string safe = libretroName(title);
    const std::string dst = localAssetPath(sdRoot, system, title, kind);
    if (!force && assetExists(sdRoot, system, title, kind)) {
        return true;
    }
    const std::string temporary = dst + ".tmp";

    const char *type = std::string(kind) == "thumbnails" ? "Named_Boxarts" : "Named_Snaps";
    const std::string base = system == "n64" ?
        "https://raw.githubusercontent.com/libretro-thumbnails/Nintendo_-_Nintendo_64/master/" +
            std::string(type) + "/" :
        "https://thumbnails.libretro.com/" + urlEncode(folder) + "/" + type + "/";
    std::string error;
    if (catalog && !catalog->empty()) {
        const auto matches = findCatalogMatches(*catalog, title);
        if (matches.empty()) {
            assetLog(sdRoot, "sin coincidencia " + std::string(kind) + " " + system +
                             " / " + title);
            return false;
        }
        // GitHub's tree also lists text placeholders with .png names. Try the
        // plain release first, then a small number of matching variants.
        const size_t attempts = std::min(matches.size(), size_t(6));
        for (size_t i = 0; i < attempts; ++i) {
            if (cancel && cancel->load()) return false;
            const CatalogEntry *match = matches[i];
            const bool fetched = downloadToFile(base + match->href, temporary, &error, cancel);
            const bool valid = fetched && validPngFile(temporary);
            if (fetched && !valid) {
                std::remove(temporary.c_str());
                error = "PNG invalido";
            }
            if (valid && replaceDownloadedFile(temporary, dst)) {
                assetLog(sdRoot, "descargado " + std::string(kind) + " " + system +
                                 " / " + title + " => " + match->name);
                return true;
            }
            assetLog(sdRoot, "fallo " + std::string(kind) + " " + system + " / " +
                             title + " => " + match->name + " " + error);
        }
        return false;
    }
    const char *suffixes[] = {"", " (USA)", " (Europe)", " (Japan)", " (World)"};
    for (const char *suffix : suffixes) {
        if (cancel && cancel->load()) return false;
        const std::string url = base + urlEncode(safe + suffix) + ".png";
        if (downloadToFile(url, temporary, &error, cancel)) {
            if (validPngFile(temporary)) {
                if (replaceDownloadedFile(temporary, dst)) return true;
            } else {
                std::remove(temporary.c_str());
                error = "PNG invalido";
            }
        }
    }
    assetLog(sdRoot, "fallo " + std::string(kind) + " " + system + " / " +
                     title + " " + error);
    return false;
}

bool downloadIfMissing(const std::string &sdRoot, const std::string &system,
                       const std::string &title, const char *kind, bool force)
{
    return downloadWithCatalog(sdRoot, system, title, kind, force, nullptr, nullptr);
}

int pendingAssetCount(const std::string &sdRoot, const std::string &system,
                      const std::vector<std::string> &titles, bool covers,
                      bool backdrops, bool force)
{
    if (libretroSystemFolder(system).empty()) return 0;
    int pending = 0;
    for (const std::string &title : titles) {
        if (covers && (force || !assetExists(sdRoot, system, title, "thumbnails")))
            ++pending;
        if (backdrops && (force || !assetExists(sdRoot, system, title, "backdrops")))
            ++pending;
    }
    return pending;
}

void downloadMetadata(const std::string &sdRoot, const std::string &system,
                      std::atomic<int> *progress, const std::atomic<bool> *cancel)
{
    const std::string folder = libretroSystemFolder(system);
    if (folder.empty()) {
        return;
    }
    const std::string dir = sdRoot + "/retrou/metadata";
    ensureDir(dir);
    const char *kinds[] = {"developer", "genre", "releaseyear"};
    for (const char *kind : kinds) {
        if (cancel && cancel->load()) break;
        const std::string path = dir + "/" + system + "-" + kind + ".dat";
        const std::string url =
            "https://raw.githubusercontent.com/libretro/libretro-database/master/metadat/" +
            std::string(kind) + "/" + urlEncode(folder) + ".dat";
        const std::string temporary = path + ".tmp";
        std::string error;
        if (!downloadToFile(url, temporary, &error, cancel) || !replaceDownloadedFile(temporary, path))
            assetLog(sdRoot, "fallo metadata " + system + " / " + kind + " " + error);
        if (progress) ++(*progress);
    }
}

void downloadAll(const std::string &sdRoot, const std::string &system,
                 const std::vector<std::string> &titles, bool covers, bool backdrops,
                 bool force, std::atomic<int> *progress, std::atomic<int> *failed,
                 const std::atomic<bool> *cancel)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
    assetLog(sdRoot, "inicio " + system + " roms=" + std::to_string(titles.size()) +
                     " portadas=" + std::to_string(covers) +
                     " fondos=" + std::to_string(backdrops) +
                     " forzar=" + std::to_string(force) +
                     " pendientes=" + std::to_string(pendingAssetCount(
                         sdRoot, system, titles, covers, backdrops, force)));
    const std::vector<CatalogEntry> coverCatalog = covers ?
        loadCatalog(sdRoot, system, "Named_Boxarts", cancel, true) : std::vector<CatalogEntry>{};
    const std::vector<CatalogEntry> backdropCatalog = backdrops ?
        loadCatalog(sdRoot, system, "Named_Snaps", cancel, !covers || system != "n64") :
        std::vector<CatalogEntry>{};
    int failures = 0;
    for (const std::string &title : titles) {
        if (cancel && cancel->load()) break;
        if (covers && (force || !assetExists(sdRoot, system, title, "thumbnails"))) {
            if (!downloadWithCatalog(sdRoot, system, title, "thumbnails", force,
                                     &coverCatalog, cancel)) {
                ++failures;
                if (failed) ++(*failed);
            }
            if (progress) ++(*progress);
        }
        if (cancel && cancel->load()) break;
        if (backdrops && (force || !assetExists(sdRoot, system, title, "backdrops"))) {
            if (!downloadWithCatalog(sdRoot, system, title, "backdrops", force,
                                     &backdropCatalog, cancel)) {
                ++failures;
                if (failed) ++(*failed);
            }
            if (progress) ++(*progress);
        }
    }
    if (!cancel || !cancel->load()) downloadMetadata(sdRoot, system, nullptr, cancel);
    assetLog(sdRoot, std::string(cancel && cancel->load() ? "cancelado " : "fin ") +
                     system + " fallos=" + std::to_string(failures));
    curl_global_cleanup();
}

} // namespace retrou::assets
