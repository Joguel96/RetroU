cmake_minimum_required(VERSION 3.14)
project(RetroU CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(RetroU
    src/main.cpp
    src/rpl_core.cpp
    src/rpl_loader.cpp
)

target_include_directories(RetroU PRIVATE
    src
    cores
    /opt/devkitpro/portlibs/wiiu/include
    /opt/devkitpro/portlibs/wiiu/include/SDL2
    /opt/devkitpro/portlibs/ppc/include
)

target_link_libraries(RetroU PRIVATE
    SDL2_ttf
    freetype
    harfbuzz
    png
    z
    bz2
    brotlidec
    brotlicommon
    SDL2
    m
)

target_compile_options(RetroU PRIVATE -Wall -Wextra)

wut_create_rpx(RetroU)

wut_create_wuhb(RetroU
    CONTENT    "${CMAKE_CURRENT_SOURCE_DIR}/content"
    NAME       "RetroU"
    SHORTNAME  "RetroU"
    AUTHOR     "joguel96"
    ICON       "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.png"
    TVSPLASH   "${CMAKE_CURRENT_SOURCE_DIR}/../assets/tv-splash.png"
    DRCSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/../assets/drc-splash.png"
)
