// =============================================================================
// scanner.cpp - Escaneo recursivo de ROMs (coreinit FS real)
// =============================================================================
//
// TODO verificado contra coreinit/filesystem.h (WUT 1.9.2 instalado):
//   FSOpenDir(client, block, path, FSDirectoryHandle*, errorMask)
//   FSReadDir(client, block, handle, FSDirectoryEntry*, errorMask)
//     -> FS_STATUS_OK con entrada; FS_STATUS_END al final del directorio
//   FSCloseDir(client, block, handle, errorMask)
//   FSDirectoryEntry { FSStat info; char name[256]; }  (WUT_CHECK_SIZE 0x164)
//   FSStat.flags con bit FS_STAT_DIRECTORY (0x80000000) para subdirs.
//
// Estrategia de deteccion de sistema (documentada en scanner.hpp):
//   1. extension en kExtMap -> sistema por extension (el requisito)
//   2. extension desconocida/ambigua -> carpeta contenedora
//   3. ambos desconocidos -> el archivo se ignora (covers, .txt, etc.)

#include "fs/scanner.hpp"
#include "fs/fs.hpp"

#include <coreinit/filesystem.h>

#include <algorithm>
#include <cctype>
#include <cstring>

namespace retroU {

namespace {

constexpr int kMaxDepth = 8; // guarda anti-bucles de symlinks raros

// --- Extension -> sistema ---------------------------------------------------
// Solo extensiones INEQUIVOCAS (caso-insensitive). Las ambiguas (.bin, .chd,
// .iso, .rom, .dsk) se resuelven por carpeta en su lugar.
struct ExtMapping {
    const char *ext;
    EmuConsole  system;
};

constexpr ExtMapping kExtMap[] = {
    // Atari
    {"a26", EmuConsole::Atari},   {"a52", EmuConsole::Atari},
    {"a78", EmuConsole::Atari},   {"xex", EmuConsole::Atari},
    {"lnx", EmuConsole::Atari},
    // MSX
    {"msx", EmuConsole::MSX},     {"mx1", EmuConsole::MSX},
    {"mx2", EmuConsole::MSX},     {"cas", EmuConsole::MSX},
    // NES
    {"nes", EmuConsole::NES},     {"fds", EmuConsole::NES},
    {"unf", EmuConsole::NES},     {"unif", EmuConsole::NES},
    {"nsf", EmuConsole::NES},
    // SNES
    {"sfc", EmuConsole::SNES},    {"smc", EmuConsole::SNES},
    {"swc", EmuConsole::SNES},    {"fig", EmuConsole::SNES},
    // Genesis / Mega Drive
    {"md", EmuConsole::Genesis},  {"gen", EmuConsole::Genesis},
    {"smd", EmuConsole::Genesis},
    // GBA
    {"gba", EmuConsole::GBA},
    // PS1 (solo extensiones de disco inequivocas)
    {"cue", EmuConsole::PS1},     {"pbp", EmuConsole::PS1},
    // Arcade
    {"zip", EmuConsole::Arcade},  {"7z", EmuConsole::Arcade},
};

// Extensiones que NUNCA son ROM (covers/readmes/saves) aunque la carpeta
// este mapeada: evita listar world.sfc.png o saves .srm como juegos.
constexpr const char *kBlacklist[] = {
    "txt", "nfo", "diz", "sfv", "md5", "sha1", "log", "ini", "cfg",
    "xml", "json", "html", "htm", "url", "db", "cht", "srm", "bak",
    "jpg", "jpeg", "png", "gif", "bmp", "tga", "pdf", "old", "tmp",
};

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// Ultima extension del nombre ("world.sfc" -> "sfc"; ".hidden" -> "").
std::string extensionOf(const std::string &name)
{
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0 || dot + 1 >= name.size()) {
        return {};
    }
    return toLower(name.substr(dot + 1));
}

std::string baseNameOf(const std::string &name)
{
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        return name;
    }
    return name.substr(0, dot);
}

EmuConsole systemFromExtension(const std::string &ext)
{
    if (ext.empty()) {
        return EmuConsole::Count;
    }
    for (const ExtMapping &m : kExtMap) {
        if (ext == m.ext) {
            return m.system;
        }
    }
    return EmuConsole::Count; // desconocida/ambigua -> usar carpeta
}

bool isBlacklisted(const std::string &ext)
{
    for (const char *b : kBlacklist) {
        if (ext == b) {
            return true;
        }
    }
    return false;
}

// Nombre de carpeta -> sistema ("snes" o "SNES" -> SNES). Count si el nombre
// no corresponde a un sistema del proyecto.
EmuConsole systemFromFolderName(const std::string &rawName)
{
    const std::string n = toLower(rawName);

    if (n == "atari") return EmuConsole::Atari;
    if (n == "msx") return EmuConsole::MSX;
    if (n == "nes" || n == "fc" || n == "famicom") return EmuConsole::NES;
    if (n == "snes" || n == "super nintendo") return EmuConsole::SNES;
    if (n == "genesis" || n == "megadrive" || n == "mega drive" ||
        n == "md")
        return EmuConsole::Genesis;
    if (n == "gba" || n == "game boy advance") return EmuConsole::GBA;
    if (n == "ps1" || n == "psx" || n == "playstation" ||
        n == "playstation 1")
        return EmuConsole::PS1;
    if (n == "arcade" || n == "mame" || n == "fbneo" || n == "fba")
        return EmuConsole::Arcade;

    return EmuConsole::Count;
}

} // namespace

bool Scanner::scan()
{
    m_roms.clear();

    if (!fs::ensureInit()) {
        return false; // sin FS/SD no hay nada que escanear
    }

    // La raiz roms/ no es un sistema: folderSystem = Count. Cada
    // subcarpeta ("snes", "PS1"...) aportara su sistema como fallback.
    scanDir(fs::romsBaseDir(), EmuConsole::Count, 0);

    // Orden estable por titulo para que la UI sea predecible.
    std::sort(m_roms.begin(), m_roms.end(),
              [](const RomInfo &a, const RomInfo &b) {
                  return a.title < b.title;
              });
    return true;
}

std::vector<const RomInfo *> Scanner::romsFor(EmuConsole system) const
{
    std::vector<const RomInfo *> out;
    for (const RomInfo &rom : m_roms) {
        if (rom.system == system) {
            out.push_back(&rom);
        }
    }
    return out;
}

size_t Scanner::countFor(EmuConsole system) const
{
    size_t n = 0;
    for (const RomInfo &rom : m_roms) {
        if (rom.system == system) {
            ++n;
        }
    }
    return n;
}

void Scanner::scanDir(const std::string &dirPath, EmuConsole folderSystem,
                      int depth)
{
    if (depth > kMaxDepth) {
        return;
    }

    // Clientes FS compartidos del singleton fs (ya inicializado en scan()).
    FSClient *client = fs::client();
    FSCmdBlock *block = fs::cmdBlock();
    if (!client || !block) {
        return;
    }

    FSDirectoryHandle handle = 0;
    if (FSOpenDir(client, block, dirPath.c_str(), &handle,
                  FS_ERROR_FLAG_NONE) != FS_STATUS_OK) {
        return; // no existe (o permisos): simplemente no hay nada aqui
    }

    // Guard: limite de entradas por directorio. Protege contra un directorio
    // corrupto/sin terminacion que haga bucle infinito de FSReadDir (eso
    // bloquearia el main loop y "congelaria" la app en el escaneo).
    constexpr size_t kMaxEntriesPerDir = 4096;
    size_t entries = 0;

    FSDirectoryEntry entry;
    while (entries < kMaxEntriesPerDir) {
        // Zeroing por seguridad: FSReadDir rellena la estructura, pero
        // garantizamos 'name' terminado en '\0' y el resto a cero aunque el
        // OS deje bytes sin tocar (evita strlen/std::string sobre basura).
        std::memset(&entry, 0, sizeof(entry));

        if (FSReadDir(client, block, handle, &entry,
                      FS_ERROR_FLAG_NONE) != FS_STATUS_OK) {
            break; // FS_STATUS_END (o error): fin del directorio
        }
        ++entries;

        entry.name[sizeof(entry.name) - 1] = '\0';

        if (std::strcmp(entry.name, ".") == 0 ||
            std::strcmp(entry.name, "..") == 0) {
            continue;
        }

        const std::string fullPath = dirPath + "/" + entry.name;

        if (entry.info.flags & FS_STAT_DIRECTORY) {
            // Subcarpeta: si su nombre es un sistema, lo adopta; si no,
            // hereda el de sus padres (permite roms/snes/juegos/...).
            EmuConsole subSystem = systemFromFolderName(entry.name);
            if (subSystem == EmuConsole::Count) {
                subSystem = folderSystem;
            }
            scanDir(fullPath, subSystem, depth + 1);
            continue;
        }

        // --- Archivo: clasificar por extension (fallback: carpeta) ---
        const std::string ext = extensionOf(entry.name);
        if (isBlacklisted(ext)) {
            continue; // cover/readme/save: nunca es ROM
        }

        const EmuConsole extSystem = systemFromExtension(ext);
        EmuConsole system = (extSystem != EmuConsole::Count) ? extSystem
                                                            : folderSystem;
        if (system == EmuConsole::Count) {
            continue; // no clasificable en ningun eje
        }

        RomInfo rom;
        rom.title  = baseNameOf(entry.name);
        rom.path   = fullPath;
        rom.system = system;
        rom.size   = entry.info.size;
        m_roms.push_back(std::move(rom));
    }

    FSCloseDir(client, block, handle, FS_ERROR_FLAG_NONE);
}

} // namespace retroU
