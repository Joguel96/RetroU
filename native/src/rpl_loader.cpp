#include "rpl_loader.hpp"

#include <coreinit/cache.h>
#include <coreinit/codegen.h>
#include <coreinit/core.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memory.h>
#include <coreinit/thread.h>

#include <reent.h>
#include <unistd.h>
#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace retrou {

namespace {

const char *kLogPath = "/vol/external01/retrou/retrou_rpl.log";

void rpl_log(const std::string &message)
{
    std::ofstream out(kLogPath, std::ios::app);
    if (out) {
        out << message << "\n";
    }
}

constexpr uint32_t kShfAlloc = 0x2;
constexpr uint32_t kShfExec = 0x4;
constexpr uint32_t kShfDeflated = 0x08000000;
constexpr uint32_t kShtNobits = 8;
constexpr uint32_t kShtRela = 4;
constexpr uint32_t kShtSymtab = 2;
constexpr uint32_t kShtRplExports = 0x80000001;

constexpr uint32_t kRppcNone = 0;
constexpr uint32_t kRppcAddr32 = 1;
constexpr uint32_t kRppcAddr16Lo = 4;
constexpr uint32_t kRppcAddr16Hi = 5;
constexpr uint32_t kRppcAddr16Ha = 6;
constexpr uint32_t kRppcRel24 = 10;
constexpr uint32_t kRppcGhsRel16Ha = 251;
constexpr uint32_t kRppcGhsRel16Hi = 252;
constexpr uint32_t kRppcGhsRel16Lo = 253;

constexpr uint16_t kShnUndef = 0;
constexpr uint16_t kShnAbs = 0xfff1;

uint16_t read_be16(const std::vector<uint8_t> &data, size_t offset)
{
    if (offset + 2 > data.size()) {
        return 0;
    }
    return static_cast<uint16_t>((data[offset] << 8) | data[offset + 1]);
}

uint32_t read_be32(const std::vector<uint8_t> &data, size_t offset)
{
    if (offset + 4 > data.size()) {
        return 0;
    }
    return (static_cast<uint32_t>(data[offset]) << 24) |
           (static_cast<uint32_t>(data[offset + 1]) << 16) |
           (static_cast<uint32_t>(data[offset + 2]) << 8) |
           static_cast<uint32_t>(data[offset + 3]);
}

uint32_t align_up(uint32_t value, uint32_t alignment)
{
    if (alignment < 4) {
        alignment = 4;
    }
    return (value + alignment - 1) & ~(alignment - 1);
}

void noop_function()
{
}

void exit_thunk(int status)
{
    _exit(status);
}

void abort_thunk()
{
    abort();
}

struct _reent *getreent_thunk()
{
    return _impure_ptr;
}

void *function_address(void (*fn)())
{
    return reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(fn));
}

} // namespace

RplLoader::~RplLoader()
{
    unload();
}

bool RplLoader::read_file(const std::string &path, std::vector<uint8_t> &out, std::string &error)
{
    /* stdio on Wii U uses the mounted path, not the sd: devoptab alias. */
    std::string actual_path = path;
    if (actual_path.rfind("sd:/", 0) == 0) {
        actual_path = "/vol/external01/" + actual_path.substr(4);
    }

    std::ifstream file(actual_path, std::ios::binary);
    if (!file) {
        error = "No se pudo abrir " + actual_path;
        return false;
    }
    file.seekg(0, std::ios::end);
    std::streamoff size = file.tellg();
    if (size <= 0) {
        error = "RPL vacio: " + path;
        return false;
    }
    file.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(out.data()), size);
    if (!file) {
        error = "No se pudo leer " + actual_path;
        return false;
    }
    rpl_log("read_file: " + actual_path + " size=" + std::to_string(out.size()));
    return true;
}

bool RplLoader::decode_section(Section &section, const std::vector<uint8_t> &file, std::string &error)
{
    if (section.type == kShtNobits) {
        section.data.assign(section.size, 0);
        return true;
    }

    if (section.size == 0) {
        section.data.clear();
        return true;
    }

    if (section.offset > file.size() || section.size > file.size() - section.offset) {
        error = "Seccion " + section.name + " fuera del RPL";
        return false;
    }

    if ((section.flags & kShfDeflated) == 0) {
        section.data.assign(file.begin() + section.offset,
                            file.begin() + section.offset + section.size);
        return true;
    }

    if (section.size < 4) {
        error = "Seccion " + section.name + " comprimida invalida";
        return false;
    }

    uint32_t raw_size = read_be32(file, section.offset);
    if (raw_size == 0 || raw_size > 32 * 1024 * 1024) {
        error = "Tamano comprimido invalido en " + section.name;
        return false;
    }

    section.data.assign(raw_size, 0);
    z_stream stream{};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    if (inflateInit(&stream) != Z_OK) {
        error = "inflateInit fallo en " + section.name;
        return false;
    }

    stream.next_in = const_cast<Bytef *>(file.data() + section.offset + 4);
    stream.avail_in = section.size - 4;
    stream.next_out = section.data.data();
    stream.avail_out = raw_size;

    int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if (result != Z_OK && result != Z_STREAM_END) {
        error = "inflate fallo en " + section.name;
        return false;
    }
    return true;
}

bool RplLoader::load_sections(const std::vector<uint8_t> &file, std::string &error)
{
    if (file.size() < 52 || read_be32(file, 0) != 0x7f454c46) {
        error = "No es un ELF/RPL";
        return false;
    }

    uint32_t shoff = read_be32(file, 32);
    uint16_t shentsize = read_be16(file, 46);
    uint16_t shnum = read_be16(file, 48);
    uint16_t shstrndx = read_be16(file, 50);

    if (shnum == 0 || shentsize < 40 || shoff > file.size()) {
        error = "Tabla de secciones invalida";
        return false;
    }

    sections_.resize(shnum);
    for (uint16_t i = 0; i < shnum; ++i) {
        size_t offset = shoff + static_cast<size_t>(i) * shentsize;
        if (offset + 40 > file.size()) {
            error = "Cabecera de seccion fuera del RPL";
            return false;
        }
        Section &section = sections_[i];
        section.name_offset = read_be32(file, offset);
        section.type = read_be32(file, offset + 4);
        section.flags = read_be32(file, offset + 8);
        section.addr = read_be32(file, offset + 12);
        section.offset = read_be32(file, offset + 16);
        section.size = read_be32(file, offset + 20);
        section.link = read_be32(file, offset + 24);
        section.info = read_be32(file, offset + 28);
        section.addralign = read_be32(file, offset + 32);
    }

    if (shstrndx >= sections_.size()) {
        error = "shstrndx invalido";
        return false;
    }

    Section &name_table = sections_[shstrndx];
    if (!decode_section(name_table, file, error)) {
        return false;
    }

    for (Section &section : sections_) {
        if (section.name_offset < name_table.data.size()) {
            section.name = reinterpret_cast<const char *>(name_table.data.data() + section.name_offset);
        }
    }

    for (Section &section : sections_) {
        if (!decode_section(section, file, error)) {
            return false;
        }
    }

    int text_index = -1;
    uint32_t total = 0;
    for (size_t i = 0; i < sections_.size(); ++i) {
        Section &section = sections_[i];
        bool keep = (section.flags & kShfAlloc) != 0 ||
                    section.type == kShtRplExports ||
                    section.name == ".bss";
        if (!keep || section.type == kShtRela || section.type == kShtSymtab ||
            section.name == ".shstrtab" || section.name == ".strtab" ||
            section.name == ".eh_frame") {
            continue;
        }
        if ((section.flags & kShfExec) != 0 || section.name == ".text") {
            if (text_index >= 0) {
                error = "RPL con mas de una seccion ejecutable";
                return false;
            }
            text_index = static_cast<int>(i);
            continue;
        }
        total = align_up(total, section.addralign);
        section.new_addr = total;
        section.loaded = true;
        total += static_cast<uint32_t>(section.data.size());
    }

    if (text_index < 0) {
        error = "RPL sin seccion .text";
        return false;
    }
    Section &text = sections_[text_index];
    if (text.data.empty()) {
        error = "RPL con .text vacio";
        return false;
    }

    void *codegen_address = nullptr;
    uint32_t codegen_size = 0;
    OSGetCodegenVirtAddrRange(&codegen_address, &codegen_size);
    rpl_log("codegen=" + std::to_string(reinterpret_cast<uintptr_t>(codegen_address)) +
            " size=" + std::to_string(codegen_size));

    if (!codegen_address || codegen_size < text.data.size()) {
        error = "Codegen insuficiente para el .text del RPL";
        return false;
    }
    codegen_base_ = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(codegen_address));
    codegen_size_ = codegen_size;

    uint32_t data_size = total == 0 ? 1 : total;
    allocation_ = MEMAllocFromDefaultHeapEx(data_size, 0x1000);
    if (allocation_) {
        system_allocation_ = false;
    } else {
        allocation_ = OSAllocFromSystem(data_size, 0x1000);
        system_allocation_ = allocation_ != nullptr;
    }
    if (!allocation_) {
        error = "Sin memoria para los datos del RPL";
        return false;
    }
    allocation_size_ = data_size;
    memset(allocation_, 0, data_size);

    uint32_t data_base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(allocation_));
    for (Section &section : sections_) {
        if (!section.loaded || section.name == ".text") {
            continue;
        }
        section.new_addr += data_base;
        section.runtime = reinterpret_cast<uint8_t *>(section.new_addr);
        if (!section.data.empty()) {
            memcpy(section.runtime, section.data.data(), section.data.size());
        }
    }

    text_buffer_ = text.data;
    text.loaded = true;
    text.runtime = text_buffer_.data();
    text.new_addr = codegen_base_;

    rpl_log("load_sections: text=" + std::to_string(text.data.size()) +
            " data=" + std::to_string(total) +
            " base=0x" + std::to_string(data_base));

    DCFlushRange(allocation_, data_size);
    return true;
}

RplLoader::Section *RplLoader::section_for_address(uint32_t address, uint32_t &offset)
{
    for (Section &section : sections_) {
        if (!section.loaded || section.data.empty()) {
            continue;
        }
        if (address >= section.addr && address < section.addr + section.data.size()) {
            offset = address - section.addr;
            return &section;
        }
    }
    return nullptr;
}

void *RplLoader::resolve_external(const std::string &name, bool weak)
{
    if (name == "__syscall_lock_acquire" ||
        name == "__syscall_lock_release" ||
        name == "__syscall_lock_acquire_recursive" ||
        name == "__syscall_lock_release_recursive" ||
        name == "__syscall_lock_close_recursive" ||
        name == "__syscall_malloc_lock" ||
        name == "__syscall_malloc_unlock" ||
        name == "__syscall_assert_func" ||
        name == "__register_frame_info" ||
        name == "__deregister_frame_info") {
        return function_address(noop_function);
    }
    if (name == "__syscall_getreent") {
        return function_address(reinterpret_cast<void (*)()>(getreent_thunk));
    }
    if (name == "__syscall_exit") {
        return function_address(reinterpret_cast<void (*)()>(exit_thunk));
    }
    if (name == "__syscall_abort") {
        return function_address(reinterpret_cast<void (*)()>(abort_thunk));
    }
    if (weak) {
        return nullptr;
    }
    return nullptr;
}

bool RplLoader::apply_relocations(std::string &error)
{
    rpl_log("apply_relocations: start");
    for (Section &rela : sections_) {
        if (rela.type != kShtRela || rela.data.empty()) {
            continue;
        }
        if (rela.info >= sections_.size() || !sections_[rela.info].loaded) {
            continue;
        }
        if (rela.link >= sections_.size() || sections_[rela.link].type != kShtSymtab) {
            continue;
        }

        Section &target_section = sections_[rela.info];
        (void)target_section;
        Section &symtab = sections_[rela.link];
        if (symtab.data.empty()) {
            continue;
        }

        uint32_t strtab_index = symtab.link;
        if (strtab_index >= sections_.size()) {
            error = "Symtab sin strtab";
            return false;
        }
        const std::vector<uint8_t> &strtab = sections_[strtab_index].data;

        size_t count = rela.data.size() / 12;
        for (size_t i = 0; i < count; ++i) {
            size_t entry = i * 12;
            uint32_t r_offset = read_be32(rela.data, entry);
            uint32_t r_info = read_be32(rela.data, entry + 4);
            int32_t r_addend = static_cast<int32_t>(read_be32(rela.data, entry + 8));
            uint32_t type = r_info & 0xff;
            uint32_t sym_index = r_info >> 8;

            if (type == kRppcNone) {
                continue;
            }

            if (sym_index * 16 + 16 > symtab.data.size()) {
                error = "Simbolo fuera de symtab";
                return false;
            }
            size_t sym_offset = sym_index * 16;
            Symbol symbol;
            symbol.name = read_be32(symtab.data, sym_offset);
            symbol.value = read_be32(symtab.data, sym_offset + 4);
            symbol.size = read_be32(symtab.data, sym_offset + 8);
            symbol.info = symtab.data[sym_offset + 12];
            symbol.other = symtab.data[sym_offset + 13];
            symbol.shndx = read_be16(symtab.data, sym_offset + 14);

            std::string symbol_name;
            if (symbol.name < strtab.size()) {
                symbol_name = reinterpret_cast<const char *>(strtab.data() + symbol.name);
            }

            bool weak = ((symbol.info >> 4) == 2);
            uint32_t symbol_address = 0;
            if (symbol.shndx == kShnUndef) {
                void *external = resolve_external(symbol_name, weak);
                if (!external && !weak) {
                    error = "Import RPL no soportado: " + symbol_name;
                    return false;
                }
                symbol_address = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(external));
            } else if (symbol.shndx == kShnAbs) {
                symbol_address = symbol.value;
            } else if (symbol.shndx < sections_.size()) {
                Section &symbol_section = sections_[symbol.shndx];
                if (!symbol_section.loaded) {
                    /* Relocation belongs to a section we deliberately do not
                     * load (for example .eh_frame). */
                    continue;
                }
                symbol_address = symbol_section.new_addr +
                                 (symbol.value - symbol_section.addr);
            } else {
                error = "Simbolo con seccion invalida";
                return false;
            }

            uint32_t target_offset = 0;
            Section *target = section_for_address(r_offset, target_offset);
            if (!target || !target->loaded) {
                continue;
            }
            if (target_offset + 4 > target->data.size()) {
                error = "Relocacion fuera de seccion";
                return false;
            }

            uint8_t *destination = target->runtime + target_offset;
            uint32_t value = symbol_address + static_cast<uint32_t>(r_addend);
            uint32_t target_address = target->new_addr + target_offset;
            int32_t relative = static_cast<int32_t>(value - target_address);

            switch (type) {
                case kRppcAddr32:
                    *reinterpret_cast<uint32_t *>(destination) = value;
                    break;
                case kRppcAddr16Lo:
                    *reinterpret_cast<uint16_t *>(destination) = static_cast<uint16_t>(value & 0xffff);
                    break;
                case kRppcAddr16Hi:
                    *reinterpret_cast<uint16_t *>(destination) = static_cast<uint16_t>((value >> 16) & 0xffff);
                    break;
                case kRppcAddr16Ha:
                    *reinterpret_cast<uint16_t *>(destination) = static_cast<uint16_t>(((value + 0x8000) >> 16) & 0xffff);
                    break;
                case kRppcRel24: {
                    uint32_t instruction = *reinterpret_cast<uint32_t *>(destination);
                    instruction = (instruction & 0xfc000003) | (static_cast<uint32_t>(relative) & 0x03fffffc);
                    *reinterpret_cast<uint32_t *>(destination) = instruction;
                    break;
                }
                case kRppcGhsRel16Lo:
                    *reinterpret_cast<uint16_t *>(destination) = static_cast<uint16_t>(relative & 0xffff);
                    break;
                case kRppcGhsRel16Hi:
                    *reinterpret_cast<uint16_t *>(destination) = static_cast<uint16_t>((relative >> 16) & 0xffff);
                    break;
                case kRppcGhsRel16Ha:
                    *reinterpret_cast<uint16_t *>(destination) = static_cast<uint16_t>(((relative + 0x8000) >> 16) & 0xffff);
                    break;
                default:
                    error = "Relocacion no soportada: " + std::to_string(type);
                    return false;
            }
        }
    }

    DCFlushRange(allocation_, allocation_size_);
    if (!text_buffer_.empty()) {
        DCFlushRange(text_buffer_.data(), text_buffer_.size());
    }

    uint32_t codegen_core = OSGetCodegenCore();
    uint32_t current_core = OSGetCoreId();
    rpl_log("codegen core=" + std::to_string(codegen_core) +
            " current=" + std::to_string(current_core));

    if (current_core != codegen_core) {
        OSThread *thread = OSGetCurrentThread();
        if (!OSSetThreadAffinity(thread, 1u << codegen_core)) {
            rpl_log("OSSetThreadAffinity failed");
        }
    }

    if (!OSCodegenCopy(reinterpret_cast<void *>(codegen_base_),
                       text_buffer_.data(), text_buffer_.size())) {
        error = "OSCodegenCopy fallo";
        return false;
    }

    rpl_log("OSCodegenCopy ok");
    rpl_log("apply_relocations: done");
    return true;
}

bool RplLoader::parse_exports(std::string &error)
{
    for (Section &section : sections_) {
        if (section.type != kShtRplExports || section.data.empty()) {
            continue;
        }
        const std::vector<uint8_t> &data = section.data;
        const uint8_t *runtime = static_cast<const uint8_t *>(allocation_) + section.new_addr;
        uint32_t count = read_be32(data, 0);
        if (count == 0 || 8 + count * 8 > data.size()) {
            error = "Tabla de exports invalida";
            return false;
        }

        for (uint32_t i = 0; i < count; ++i) {
            uint32_t value = (static_cast<uint32_t>(runtime[8 + i * 8]) << 24) |                             (static_cast<uint32_t>(runtime[8 + i * 8 + 1]) << 16) |
                             (static_cast<uint32_t>(runtime[8 + i * 8 + 2]) << 8) |
                             static_cast<uint32_t>(runtime[8 + i * 8 + 3]);
            uint32_t name_offset = (static_cast<uint32_t>(runtime[8 + i * 8 + 4]) << 24) |
                                   (static_cast<uint32_t>(runtime[8 + i * 8 + 5]) << 16) |
                                   (static_cast<uint32_t>(runtime[8 + i * 8 + 6]) << 8) |
                                   static_cast<uint32_t>(runtime[8 + i * 8 + 7]);
            if (name_offset >= data.size()) {
                continue;
            }
            std::string name = reinterpret_cast<const char *>(runtime + name_offset);
            if (!name.empty()) {
                exports_[name] = reinterpret_cast<void *>(static_cast<uintptr_t>(value));
            }
        }
        rpl_log("exports=" + std::to_string(exports_.size()));
        return !exports_.empty();
    }

    error = "RPL sin .fexports";
    return false;
}

bool RplLoader::load(const std::string &path, std::string &error)
{
    unload();

    std::vector<uint8_t> file;
    if (!read_file(path, file, error)) {
        return false;
    }
    if (!load_sections(file, error)) {
        unload();
        return false;
    }
    if (!apply_relocations(error)) {
        unload();
        return false;
    }
    if (!parse_exports(error)) {
        unload();
        return false;
    }
    return true;
}

void *RplLoader::findExport(const std::string &name) const
{
    auto found = exports_.find(name);
    return found == exports_.end() ? nullptr : found->second;
}

void RplLoader::unload()
{
    if (allocation_) {
        if (system_allocation_) {
            OSFreeToSystem(allocation_);
        } else {
            MEMFreeToDefaultHeap(allocation_);
        }
    }
    allocation_ = nullptr;
    allocation_size_ = 0;
    system_allocation_ = false;
    codegen_base_ = 0;
    codegen_size_ = 0;
    text_buffer_.clear();
    sections_.clear();
    exports_.clear();
}

} // namespace retrou
