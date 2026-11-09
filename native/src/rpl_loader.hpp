#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace retrou {

class RplLoader {
public:
    ~RplLoader();

    bool load(const std::string &path, std::string &error);
    void unload();
    void *findExport(const std::string &name) const;

private:
    struct Section {
        uint32_t name_offset = 0;
        uint32_t type = 0;
        uint32_t flags = 0;
        uint32_t addr = 0;
        uint32_t offset = 0;
        uint32_t size = 0;
        uint32_t link = 0;
        uint32_t info = 0;
        uint32_t addralign = 0;
        std::string name;
        std::vector<uint8_t> data;
        uint32_t new_addr = 0;
        uint8_t *runtime = nullptr;
        bool loaded = false;
    };

    struct Symbol {
        uint32_t name = 0;
        uint32_t value = 0;
        uint32_t size = 0;
        uint8_t info = 0;
        uint8_t other = 0;
        uint16_t shndx = 0;
    };

    std::vector<Section> sections_;
    std::map<std::string, void *> exports_;
    std::vector<uint8_t> text_buffer_;
    void *allocation_ = nullptr;
    uint32_t allocation_size_ = 0;
    bool system_allocation_ = false;
    uint32_t codegen_base_ = 0;
    uint32_t codegen_size_ = 0;

    bool read_file(const std::string &path, std::vector<uint8_t> &out, std::string &error);
    bool decode_section(Section &section, const std::vector<uint8_t> &file, std::string &error);
    bool load_sections(const std::vector<uint8_t> &file, std::string &error);
    bool apply_relocations(std::string &error);
    bool parse_exports(std::string &error);
    void *resolve_external(const std::string &name, bool weak);
    Section *section_for_address(uint32_t address, uint32_t &offset);
};

} // namespace retrou
