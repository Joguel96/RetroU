#pragma once

#include <string>

namespace retrou {

/*
 * Loads an RPX into the HBL memory area and asks HBL to start it.
 * Adapted from RetroArch's Wii U HBL loader (GPLv3, dimok/RetroArch).
 */
bool hbl_load_to_memory(const std::string &rpx_path, std::string &error);

} // namespace retrou
