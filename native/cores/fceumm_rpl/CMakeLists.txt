cmake_minimum_required(VERSION 3.14)
project(FceummRpl C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FCEUMM_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.spike_fceumm"
    CACHE PATH "fceumm source tree")
set(FCEUMM_LIB "${FCEUMM_ROOT}/fceumm_libretro_wiiu.a"
    CACHE FILEPATH "fceumm_libretro_wiiu.a")
set(LIBRETRO_COMMON "${FCEUMM_ROOT}/src/drivers/libretro/libretro-common")

add_executable(fceumm_rpl
    rpl_start.cpp
    "${LIBRETRO_COMMON}/compat/compat_strl.c"
    "${LIBRETRO_COMMON}/file/file_path.c"
    "${LIBRETRO_COMMON}/file/file_path_io.c"
    "${LIBRETRO_COMMON}/streams/file_stream.c"
    "${LIBRETRO_COMMON}/streams/file_stream_transforms.c"
    "${LIBRETRO_COMMON}/string/stdstring.c"
    "${LIBRETRO_COMMON}/vfs/vfs_implementation.c"
)

target_include_directories(fceumm_rpl PRIVATE
    "${LIBRETRO_COMMON}/include"
)

target_link_libraries(fceumm_rpl PRIVATE "${FCEUMM_LIB}" m)

wut_create_rpl(fceumm_rpl)
wut_add_exports(fceumm_rpl fceumm.def)
