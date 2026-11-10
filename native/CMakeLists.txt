cmake_minimum_required(VERSION 3.14)
project(RetroU C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(FCEUMM_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../.cores/fceumm/fceumm_libretro_wiiu.a"
    CACHE FILEPATH "fceumm static core")
set(SNES9X_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../.cores/snes9x/libretro/snes9x_libretro_wiiu.a"
    CACHE FILEPATH "snes9x static core")
set(MGBA_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../.cores/mgba/build/mgba_libretro.a"
    CACHE FILEPATH "mgba static core")

set(LIBRETRO_COMMON "${CMAKE_CURRENT_SOURCE_DIR}/../.cores/fceumm/src/drivers/libretro/libretro-common")
set(RETRO_COMMON_SOURCES
    "${LIBRETRO_COMMON}/compat/compat_strl.c"
    "${LIBRETRO_COMMON}/file/file_path.c"
    "${LIBRETRO_COMMON}/file/file_path_io.c"
    "${LIBRETRO_COMMON}/streams/file_stream.c"
    "${LIBRETRO_COMMON}/streams/file_stream_transforms.c"
    "${LIBRETRO_COMMON}/streams/trans_stream.c"
    "${LIBRETRO_COMMON}/streams/trans_stream_pipe.c"
    "${LIBRETRO_COMMON}/streams/trans_stream_deflate.c"
    "${LIBRETRO_COMMON}/streams/memory_stream.c"
    "${LIBRETRO_COMMON}/encodings/encoding_deflate.c"
    "${LIBRETRO_COMMON}/string/stdstring.c"
    "${LIBRETRO_COMMON}/vfs/vfs_implementation.c"
)

set(OBJCOPY "/opt/devkitpro/devkitPPC/bin/powerpc-eabi-objcopy.exe")
set(FONT_OBJECT_DIR "${CMAKE_CURRENT_BINARY_DIR}/embedded_fonts")
set(FONT_REGULAR_OBJ "${FONT_OBJECT_DIR}/ui_ttf.o")
set(FONT_BOLD_OBJ "${FONT_OBJECT_DIR}/ui_bold_ttf.o")

add_custom_command(
    OUTPUT "${FONT_REGULAR_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${FONT_OBJECT_DIR}"
    COMMAND "${OBJCOPY}" -I binary -O elf32-powerpc -B powerpc
            --rename-section .data=.rodata,alloc,load,readonly,data,contents
            ui.ttf "${FONT_REGULAR_OBJ}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/content"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/content/ui.ttf"
)

add_custom_command(
    OUTPUT "${FONT_BOLD_OBJ}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${FONT_OBJECT_DIR}"
    COMMAND "${OBJCOPY}" -I binary -O elf32-powerpc -B powerpc
            --rename-section .data=.rodata,alloc,load,readonly,data,contents
            ui-bold.ttf "${FONT_BOLD_OBJ}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/content"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/content/ui-bold.ttf"
)

function(configure_retrou target)
    target_sources(${target} PRIVATE "${FONT_REGULAR_OBJ}" "${FONT_BOLD_OBJ}")
    set_source_files_properties("${FONT_REGULAR_OBJ}" "${FONT_BOLD_OBJ}"
        PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)

    target_include_directories(${target} PRIVATE
        src
        cores
        /opt/devkitpro/portlibs/wiiu/include
        /opt/devkitpro/portlibs/wiiu/include/SDL2
        /opt/devkitpro/portlibs/ppc/include
    )

    target_link_libraries(${target} PRIVATE
        SDL2_ttf
        SDL2_image
        webp
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

function(add_static_retrou target system label app_name core)
    add_executable(${target}
        src/main.cpp
        src/rpl_core.cpp
        src/rpl_loader.cpp
        ${RETRO_COMMON_SOURCES}
    )
    target_include_directories(${target} PRIVATE "${LIBRETRO_COMMON}/include")
    target_compile_definitions(${target} PRIVATE
        RETROU_STATIC_CORE=1
        RETROU_CORE_SYSTEM="${system}"
        RETROU_CORE_LABEL="${label}"
        RETROU_APP_NAME="${app_name}"
    )
    configure_retrou(${target})
    target_link_libraries(${target} PRIVATE "${core}" m z)
    wut_create_rpx(${target})
    wut_create_wuhb(${target}
        CONTENT    "${CMAKE_CURRENT_SOURCE_DIR}/content"
        NAME       "${app_name}"
        SHORTNAME  "${app_name}"
        AUTHOR     "joguel96"
        ICON       "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/${system}/icon.png"
        TVSPLASH   "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/${system}/tv-splash.png"
        DRCSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/${system}/drc-splash.png"
    )
endfunction()

add_static_retrou(retrou_nes nes "NES" "RetroNES U" "${FCEUMM_CORE}")
add_static_retrou(retrou_snes snes "SNES" "RetroSNES U" "${SNES9X_CORE}")
add_static_retrou(retrou_gba gba "GBA" "RetroGBA U" "${MGBA_CORE}")

add_custom_target(stage_wuhb ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../dist/wiiu/apps/RetroNESU"
            "${CMAKE_CURRENT_SOURCE_DIR}/../dist/wiiu/apps/RetroSNESU"
            "${CMAKE_CURRENT_SOURCE_DIR}/../dist/wiiu/apps/RetroGBAU"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/retrou_nes.wuhb"
            "${CMAKE_CURRENT_SOURCE_DIR}/../dist/wiiu/apps/RetroNESU/RetroNESU.wuhb"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/retrou_snes.wuhb"
            "${CMAKE_CURRENT_SOURCE_DIR}/../dist/wiiu/apps/RetroSNESU/RetroSNESU.wuhb"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/retrou_gba.wuhb"
            "${CMAKE_CURRENT_SOURCE_DIR}/../dist/wiiu/apps/RetroGBAU/RetroGBAU.wuhb"
    DEPENDS retrou_nes_wuhb retrou_snes_wuhb retrou_gba_wuhb
    COMMENT "Staging per-console WUHB files"
    VERBATIM
)
