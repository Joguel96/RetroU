#pragma once

#include <string>

namespace retrou {

/*
 * Launches an RPX from the SD card through Mocha, using the same mechanism as
 * RetroArch's wiiu-wut branch. The target receives the RPX path and ROM path
 * as NUL-separated standard arguments.
 */
bool mocha_launch_rpx(const std::string &rpx_path, const std::string &rom_path,
                      std::string &error);

} // namespace retrou
