#include "mocha_launch.hpp"

#include <mocha/mocha.h>
#include <sysapp/args.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace retrou {

namespace {

const char *kLogPath = "/vol/external01/retrou/launcher.log";

void mocha_log(const std::string &message)
{
    std::ofstream out(kLogPath, std::ios::app);
    out << std::ios::unitbuf;
    if (out) {
        out << message << std::endl;
    }
}

std::string sd_relative_path(const std::string &path)
{
    if (path.rfind("/vol/external01/", 0) == 0) {
        return path.substr(16);
    }
    if (path.rfind("sd:/", 0) == 0) {
        return path.substr(4);
    }
    return {};
}

} // namespace

bool mocha_launch_rpx(const std::string &rpx_path, const std::string &rom_path,
                      std::string &error)
{
    const std::string relative = sd_relative_path(rpx_path);
    if (relative.empty()) {
        error = "El RPX no esta en la SD";
        mocha_log("Mocha error: " + error);
        return false;
    }

    MochaUtilsStatus status = Mocha_InitLibrary();
    if (status != MOCHA_RESULT_SUCCESS) {
        error = std::string("Mocha init: ") + Mocha_GetStatusStr(status);
        mocha_log(error);
        return false;
    }
    mocha_log("Mocha init ok");

    MochaRPXLoadInfo load_info{};
    load_info.target = LOAD_RPX_TARGET_SD_CARD;
    load_info.filesize = 0;
    load_info.fileoffset = 0;
    if (std::snprintf(load_info.path, sizeof(load_info.path), "%s",
                      relative.c_str()) >=
        static_cast<int>(sizeof(load_info.path))) {
        error = "Ruta RPX demasiado larga";
        mocha_log("Mocha error: " + error);
        return false;
    }
    mocha_log(std::string("Mocha PrepareRPXLaunch ") + load_info.path);

    status = Mocha_PrepareRPXLaunch(&load_info);
    if (status != MOCHA_RESULT_SUCCESS) {
        error = std::string("Mocha PrepareRPXLaunch: ") + Mocha_GetStatusStr(status);
        mocha_log(error);
        return false;
    }
    mocha_log("Mocha PrepareRPXLaunch ok");

    std::string args;
    args.append(rpx_path);
    args.push_back('\0');
    args.append(rom_path);
    args.push_back('\0');

    SYSStandardArgsIn std_args{};
    std_args.argString = args.data();
    std_args.size = static_cast<uint32_t>(args.size());

    status = Mocha_LaunchHomebrewWrapperEx(&std_args);
    if (status != MOCHA_RESULT_SUCCESS) {
        error = std::string("Mocha LaunchHomebrewWrapperEx: ") +
                Mocha_GetStatusStr(status);
        mocha_log(error);

        MochaRPXLoadInfo revert{};
        revert.target = LOAD_RPX_TARGET_EXTRA_REVERT_PREPARE;
        Mocha_PrepareRPXLaunch(&revert);
        return false;
    }

    mocha_log("Mocha LaunchHomebrewWrapperEx ok");
    return true;
}

} // namespace retrou
