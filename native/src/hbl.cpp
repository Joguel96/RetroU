/*
 * HBL RPX loader for RetroU.
 *
 * This is adapted from RetroArch's os/wiiu/hbl.c, which is itself adapted
 * from dimok789/homebrew_launcher. RetroArch is GPLv3; keep this file under
 * the same terms and attribution.
 */

#include "hbl.hpp"

#include <coreinit/cache.h>
#include <coreinit/memory.h>
#include <coreinit/memorymap.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <malloc.h>
#include <sys/stat.h>
#include <vector>

namespace retrou {

namespace {

constexpr uint32_t kMemBase = 0x00800000;
constexpr uint32_t kArgvPtrAddress = kMemBase + 0x1380;
constexpr uint32_t kAppBaseMem = kMemBase + 0x2000;

#define MEM_AREA_TABLE ((MemoryArea *)(kMemBase + 0x1600))
#define ELF_DATA_ADDR  (*(volatile uint32_t *)(kMemBase + 0x1300 + 0x00))
#define ELF_DATA_SIZE  (*(volatile uint32_t *)(kMemBase + 0x1300 + 0x04))
#define RPX_MAX_SIZE   (*(volatile uint32_t *)(kMemBase + 0x1300 + 0x0C))
#define RPX_MAX_CODE_SIZE (*(volatile uint32_t *)(kMemBase + 0x1300 + 0x10))
#define ARGV_PTR       (*(void *volatile *)(kArgvPtrAddress))

constexpr uint32_t kArgvMagic = 0x5F617267; /* "_arg" */

struct MemoryArea {
    uint32_t address;
    uint32_t size;
    MemoryArea *next;
};

struct MemoryValue {
    uint32_t start;
    uint32_t end;
};

const MemoryValue kMemoryValues[] = {
    {0x2E609EFC, 0x2FF82000},
    {0x29030800, 0x293F6000},
    {0x288EEC30, 0x28B06800},
    {0x2D3B966C, 0x2D894000},
    {0x2CB56370, 0x2D1EF000},
    {0x2D8AD3D8, 0x2E000000},
    {0x2970200C, 0x298B9800},
    {0x2A057B68, 0x2A1B9000},
    {0x2ABBCC4C, 0x2ACB9000},
    {0, 0},
};

struct HblArgs {
    uint32_t magic;
    uint32_t argc;
    char *argv[2];
    char path[256];
};

void *application_end()
{
    /* WUT does not export _end. HBL reserves this area for the app/args. */
    return reinterpret_cast<void *>(kAppBaseMem);
}

void kernel_copy_data(uint32_t destination, uint32_t source, uint32_t length)
{
    register uint32_t r3 __asm__("r3") = destination;
    register uint32_t r4 __asm__("r4") = source;
    register uint32_t r5 __asm__("r5") = length;
    __asm__ volatile("li 0, 0x2500\n"
                     "sc\n"
                     : "+r"(r3)
                     : "r"(r4), "r"(r5)
                     : "r0", "memory");
}

void add_memory_area(int start, int end, int index)
{
    MemoryArea *area = MEM_AREA_TABLE;
    area[index].address = start;
    area[index].size = end - start;
    area[index].next = nullptr;
    if (index > 0) {
        area[index - 1].next = &area[index];
    }
}

void init_memory_table(uint32_t args_size)
{
    uint32_t app_end = static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>(application_end())) + args_size;
    add_memory_area(app_end + 0x30000000, 0x30000000 + 0x01E20000, 0);

    const MemoryValue *values = kMemoryValues;
    for (int i = 0; values[i].start; ++i) {
        add_memory_area(values[i].start, values[i].end, i + 1);
    }
}

int copy_memory(uint8_t *address, uint32_t bytes, uint32_t args_size)
{
    args_size += 0x7;
    args_size &= ~0x7u;
    if (args_size > 0x10000) {
        args_size = 0x10000;
    }

    init_memory_table(args_size);

    RPX_MAX_SIZE = 0x40000000;
    RPX_MAX_CODE_SIZE = 0x03000000;

    if (*(uint16_t *)&address[7] != 0xCAFE) {
        ELF_DATA_ADDR = static_cast<uint32_t>(
            reinterpret_cast<uintptr_t>(application_end())) + args_size;
        if (ELF_DATA_ADDR >= 0x01000000) {
            return -1;
        }
    } else {
        ELF_DATA_ADDR = MEM_AREA_TABLE->address;
    }

    if (ELF_DATA_ADDR < 0x01000000) {
        if (ELF_DATA_ADDR + bytes > 0x01000000) {
            return -1;
        }
        memcpy(reinterpret_cast<void *>(ELF_DATA_ADDR), address, bytes);
        ELF_DATA_SIZE = bytes;
    } else {
        uint32_t done = 0;
        uint32_t map_position = 0;
        DCFlushRange(address, bytes);
        uint32_t physical = OSEffectiveToPhysical(
            reinterpret_cast<uint32_t>(address));
        MemoryArea *area = MEM_AREA_TABLE;

        while (done < bytes && area) {
            if (map_position >= area->size) {
                area = area->next;
                if (!area) {
                    return -1;
                }
                map_position = 0;
            }

            uint32_t block = bytes - done;
            if (map_position + block > area->size) {
                block = area->size - map_position;
            }
            kernel_copy_data(area->address + map_position,
                             physical + done, block);
            map_position += block;
            done += block;
        }
        ELF_DATA_SIZE = done;
    }
    return static_cast<int>(bytes);
}

bool read_file(const std::string &path, std::vector<uint8_t> &data, std::string &error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "No se pudo abrir " + path;
        return false;
    }
    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    if (size <= 0) {
        error = "RPX vacio: " + path;
        return false;
    }
    file.seekg(0, std::ios::beg);
    data.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(data.data()), size);
    if (!file) {
        error = "No se pudo leer " + path;
        return false;
    }
    return true;
}

} // namespace

bool hbl_load_to_memory(const std::string &rpx_path, std::string &error)
{
    std::vector<uint8_t> data;
    if (!read_file(rpx_path, data, error)) {
        return false;
    }

    size_t aligned_size = (data.size() + 0x3f) & ~static_cast<size_t>(0x3f);
    uint8_t *buffer = static_cast<uint8_t *>(memalign(0x40, aligned_size));
    if (!buffer) {
        error = "Sin memoria para leer el RPX";
        return false;
    }
    memset(buffer, 0, aligned_size);
    memcpy(buffer, data.data(), data.size());

    HblArgs *args = static_cast<HblArgs *>(application_end());
    memset(args, 0, sizeof(*args));
    args->magic = kArgvMagic;
    args->argc = 1;
    args->argv[0] = args->path;
    args->argv[1] = nullptr;
    std::snprintf(args->path, sizeof(args->path), "%s", rpx_path.c_str());

    int result = copy_memory(buffer, static_cast<uint32_t>(data.size()),
                             sizeof(*args));
    free(buffer);
    if (result < 0) {
        error = "HBL no pudo copiar el RPX";
        return false;
    }

    ARGV_PTR = args;
    return true;
}

} // namespace retrou
