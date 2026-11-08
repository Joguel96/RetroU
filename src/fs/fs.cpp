// =============================================================================
// fs.cpp - Nucleo de acceso FS (init unico + lectura de archivos)
// =============================================================================
//
// TODO lo de aqui esta verificado contra coreinit/filesystem.h (WUT 1.9.2
// instalado): FSInit/FSShutdown, FSAddClient/FSDelClient, FSInitCmdBlock,
// FSOpenFile/FSReadFile/FSCloseFile, FSGetStat. Firmas copiadas tal cual.

#include "fs/fs.hpp"

#include <coreinit/filesystem.h>

#include <cstring>

namespace retroU {
namespace fs {

namespace {

// FSClient = 0x1700 bytes y FSCmdBlock = 0xA80 bytes (WUT_CHECK_SIZE del
// header): estaticos, nunca en el stack del main (el stack de un titulo
// no da tanto margen).
FSClient   s_client;
FSCmdBlock s_cmdBlock;
bool       s_ready = false;

} // namespace

bool ensureInit()
{
    if (s_ready) {
        return true; // idempotente
    }

    FSInit();

    if (FSAddClient(&s_client, FS_ERROR_FLAG_NONE) != FS_STATUS_OK) {
        return false; // sin cliente FS no hay SD
    }

    FSInitCmdBlock(&s_cmdBlock);
    s_ready = true;
    return true;
}

FSClient *client()
{
    return ensureInit() ? &s_client : nullptr;
}

FSCmdBlock *cmdBlock()
{
    return ensureInit() ? &s_cmdBlock : nullptr;
}

const char *romsBaseDir()
{
    // Volumen SD en retail/Aroma: fs:/vol/external01/ (convencion del
    // homebrew moderno; "sd:/" NO existe en Cafe OS).
    return "fs:/vol/external01/retroU/roms";
}

const char *coversBaseDir()
{
    return "fs:/vol/external01/retroU/covers";
}

bool readFile(const std::string &path, std::vector<uint8_t> &out,
              uint32_t maxBytes)
{
    out.clear();

    if (!ensureInit()) {
        return false;
    }

    FSFileHandle handle = 0;
    if (FSOpenFile(&s_client, &s_cmdBlock, path.c_str(), "r", &handle,
                   FS_ERROR_FLAG_NONE) != FS_STATUS_OK) {
        return false;
    }

    // Tamano via FSGetStat en la ruta (el handle tambien lo permite con
    // FSGetStatFile; usamos la ruta para no depender del orden de ops).
    FSStat stat;
    std::memset(&stat, 0, sizeof(stat));
    bool ok = false;
    if (FSGetStat(&s_client, &s_cmdBlock, path.c_str(), &stat,
                  FS_ERROR_FLAG_NONE) == FS_STATUS_OK &&
        stat.size > 0 && stat.size <= maxBytes) {
        out.resize(stat.size);

        // FSReadFile(client, block, buf, size, count, handle, unk1, mask):
        // size = bytes por elemento, count = elementos; unk1 = 0 (parametro
        // sin documentar en el header, los homebrew pasan 0).
        const int32_t read = FSReadFile(&s_client, &s_cmdBlock, out.data(),
                                        stat.size, 1, handle, 0,
                                        FS_ERROR_FLAG_NONE);
        if (read >= 0 && static_cast<uint32_t>(read) == stat.size) {
            ok = true;
        } else {
            out.clear();
        }
    }

    FSCloseFile(&s_client, &s_cmdBlock, handle, FS_ERROR_FLAG_NONE);
    return ok;
}

} // namespace fs
} // namespace retroU
