cmake_minimum_required(VERSION 3.14)
project(RetroU CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(configure_retrou target)
    target_include_directories(${target} PRIVATE
        src
        cores
        /opt/devkitpro/portlibs/wiiu/include
        /opt/devkitpro/portlibs/wiiu/include/SDL2
        /opt/devkitpro/portlibs/ppc/include
    )

    target_link_libraries(${target} PRIVATE
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

    target_compile_options(${target} PRIVATE -Wall -Wextra)
endfunction()

add_executable(RetroU
    src/main.cpp
    src/rpl_core.cpp
    src/rpl_loader.cpp
)
configure_retrou(RetroU)
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

# Static-core test: same frontend, but test_core.cpp is linked into the RPX
# instead of being loaded as an RPL. This mirrors the RetroArch Wii U model.
add_executable(retrou_test
    src/main.cpp
    src/rpl_core.cpp
    src/rpl_loader.cpp
    cores/test_core.cpp
)
target_compile_definitions(retrou_test PRIVATE RETROU_STATIC_TEST=1)
configure_retrou(retrou_test)
wut_create_rpx(retrou_test)
wut_create_wuhb(retrou_test
    CONTENT    "${CMAKE_CURRENT_SOURCE_DIR}/content"
    NAME       "RetroU Test"
    SHORTNAME  "RetroU Test"
    AUTHOR     "joguel96"
    ICON       "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.png"
    TVSPLASH   "${CMAKE_CURRENT_SOURCE_DIR}/../assets/tv-splash.png"
    DRCSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/../assets/drc-splash.png"
)
