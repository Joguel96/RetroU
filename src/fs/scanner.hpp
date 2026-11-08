#pragma once

// =============================================================================
// scanner.hpp - Escaneo de ROMs en sd:/retroU/roms/[sistema]/
// =============================================================================
//
// Escaneo RECURSIVO de la estructura del proyecto (ver AGENTS.md):
//   fs:/vol/external01/retroU/roms/Atari/
//   fs:/vol/external01/retroU/roms/MSX/
//   ... (NES, SNES, Genesis, GBA, PS1, Arcade)
//
// Por cada archivo ROM valido produce un RomInfo con:
//   - title: nombre base del archivo SIN extension
//   - path : ruta completa en la SD
//   - system: detectado por EXTENSION (tabla kExtMap); si la extension es
//     desconocida o ambigua (.bin, .chd, .iso...), cae al sistema de la
//     CARPETA contenedora. Si ambos son desconocidos, el archivo se ignora.
//   - size : tamano en bytes (FSStat)
//
// API real usada (coreinit/filesystem.h, verificada): FSInit, FSAddClient,
// FSInitCmdBlock, FSOpenDir, FSReadDir, FSCloseDir, FS_STAT_DIRECTORY.
// NOTA: FSClient (0x1700) + FSCmdBlock (0xA80) son estaticos en el .cpp:
// demasiado grandes para el stack.

#include <cstdint>
#include <string>
#include <vector>

#include "core/systems.hpp"

namespace retroU {

struct RomInfo {
    std::string title;   // "Super Mario World" (nombre sin extension)
    std::string path;    // "fs:/vol/external01/retroU/roms/snes/world.sfc"
    EmuConsole  system;  // detectado por extension (fallback: carpeta)
    uint32_t    size = 0; // bytes (FSStat.size)
};

class Scanner {
public:
    // Escanea recursivamente romsBaseDir(). Devuelve false si el FS no
    // respondio (no se pudo crear el cliente). El listado anterior se
    // descarta siempre (re-escaneo completo).
    bool scan();

    void clear() { m_roms.clear(); }

    const std::vector<RomInfo> &roms() const { return m_roms; }

    // Sublista de juegos de un sistema (punteros validos mientras este
    // Scanner viva y no se vuelva a escanear). Para la vista grid.
    std::vector<const RomInfo *> romsFor(EmuConsole system) const;

    // Cantidad de juegos de un sistema (para la lista de consolas).
    size_t countFor(EmuConsole system) const;

private:
    // dirPath: ruta absoluta; folderSystem: sistema de la carpeta contenedora
    // (EmuConsole::Count si no esta mapeada); depth: guarda anti-bucles.
    void scanDir(const std::string &dirPath, EmuConsole folderSystem,
                 int depth);

    std::vector<RomInfo> m_roms;
};

} // namespace retroU
