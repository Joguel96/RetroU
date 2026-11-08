#pragma once

// =============================================================================
// fs.hpp - Acceso al filesystem de la SD (coreinit FS) - instancia unica
// =============================================================================
//
// Wrappers sobre la API real de coreinit/filesystem.h (verificada en el
// header instalado de WUT 1.9.2). FSInit/FSAddClient son globales: se hacen
// UNA vez y se comparten entre Scanner y TextureLoader.
//
// Paths en Wii U retail/Aroma: el volumen SD se monta en
//   fs:/vol/external01/...
// (convencion del homebrew moderno con coreinit FS; no existe "sd:/" en
// Cafe OS - eso es de otras consolas).

#include <cstdint>
#include <string>
#include <vector>

#include <coreinit/filesystem.h> // FSClient/FSCmdBlock (tipos para acceso directo)

namespace retroU {
namespace fs {

// Inicializa FS (FSInit + FSAddClient + FSInitCmdBlock) si no lo estaba.
// Idempotente. Devuelve false si el cliente no se pudo anadir (SD ausente).
bool ensureInit();

// Clientes compartidos para llamadas directas de alto volumen
// (FSOpenDir/FSReadDir del scanner, FSOpenFile de texture_loader).
// Validos tras ensureInit() == true.
FSClient   *client();
FSCmdBlock *cmdBlock();

// Ruta base de ROMs (constante del proyecto, ver AGENTS.md/dist).
const char *romsBaseDir();   // "fs:/vol/external01/retroU/roms"
const char *coversBaseDir(); // "fs:/vol/external01/retroU/covers"

// Lee un archivo entero en 'out' (limitado a maxBytes para no reventar la
// RAM). false = no existe / error de lectura.
bool readFile(const std::string &path, std::vector<uint8_t> &out,
              uint32_t maxBytes = 16u * 1024u * 1024u);

} // namespace fs
} // namespace retroU
