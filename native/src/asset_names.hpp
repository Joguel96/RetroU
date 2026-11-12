#pragma once

#include <string>

namespace retrou {

inline bool isUtf8(const std::string &value)
{
    for (size_t i = 0; i < value.size();) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x80) { ++i; continue; }
        const size_t count = (c & 0xE0) == 0xC0 ? 2 :
                             (c & 0xF0) == 0xE0 ? 3 :
                             (c & 0xF8) == 0xF0 ? 4 : 0;
        if (!count || i + count > value.size()) return false;
        for (size_t j = 1; j < count; ++j)
            if ((static_cast<unsigned char>(value[i + j]) & 0xC0) != 0x80)
                return false;
        i += count;
    }
    return true;
}

inline std::string displayUtf8(const std::string &value)
{
    if (isUtf8(value)) return value;
    std::string out;
    for (unsigned char c : value) {
        if (c < 0x80) out.push_back(static_cast<char>(c));
        else {
            out.push_back(static_cast<char>(0xC0 | (c >> 6)));
            out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return out;
}

inline std::string assetFileStem(const std::string &title)
{
    std::string out;
    for (size_t i = 0; i < title.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(title[i]);
        if (c == 0xC3 && i + 1 < title.size()) {
            const unsigned char next = static_cast<unsigned char>(title[i + 1]);
            if (next >= 0x80 && next <= 0xBF) { c = next + 0x40; ++i; }
        }
        if (c >= 0xC0 && c <= 0xC5) c = 'A';
        else if (c >= 0xE0 && c <= 0xE5) c = 'a';
        else if (c >= 0xC8 && c <= 0xCB) c = 'E';
        else if (c >= 0xE8 && c <= 0xEB) c = 'e';
        else if (c >= 0xCC && c <= 0xCF) c = 'I';
        else if (c >= 0xEC && c <= 0xEF) c = 'i';
        else if (c >= 0xD2 && c <= 0xD6) c = 'O';
        else if (c >= 0xF2 && c <= 0xF6) c = 'o';
        else if (c >= 0xD9 && c <= 0xDC) c = 'U';
        else if (c >= 0xF9 && c <= 0xFC) c = 'u';
        else if (c == 0xD1) c = 'N';
        else if (c == 0xF1) c = 'n';
        if (c >= 0x80 || std::string("&*/:`<>?\\|\"").find(static_cast<char>(c)) !=
                             std::string::npos)
            c = '_';
        out.push_back(static_cast<char>(c));
    }
    return out;
}

} // namespace retrou
