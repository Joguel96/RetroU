cmake_minimum_required(VERSION 3.14)
project(RetroU C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIBRETRO_COMMON "${CMAKE_CURRENT_SOURCE_DIR}/../.cores/fceumm/src/drivers/libretro/libretro-common")
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

set(FCEUMM_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../.cores/fceumm/fceumm_libretro_wiiu.a"
    CACHE FILEPATH "fceumm static core")
set(SNES9X_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../.cores/snes9x/libretro/snes9x_libretro_wiiu.a"
    CACHE FILEPATH "snes9x static core")
set(MGBA_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../.cores/mgba/build/mgba_libretro.a"
    CACHE FILEPATH "mgba static core")

function(stage_rpx target output_name)
    get_target_property(RPX_FILE ${target} WUT_RPL)
    if(RPX_FILE)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${CMAKE_CURRENT_SOURCE_DIR}/../dist/retrou/cores"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${RPX_FILE}"
                    "${CMAKE_CURRENT_SOURCE_DIR}/../dist/retrou/cores/${output_name}"
            COMMENT "Staging ${output_name} in dist/retrou/cores"
            VERBATIM
        )
    endif()
endfunction()

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

function(add_static_retrou target system label core)
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
    )
    configure_retrou(${target})
    target_link_libraries(${target} PRIVATE "${core}" m z)
    wut_create_rpx(${target})
    stage_rpx(${target} "RetroU-${label}.rpx")
    wut_create_wuhb(${target}
        CONTENT    "${CMAKE_CURRENT_SOURCE_DIR}/content"
        NAME       "RetroU ${label}"
        SHORTNAME  "RetroU ${label}"
        AUTHOR     "joguel96"
        ICON       "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.png"
        TVSPLASH   "${CMAKE_CURRENT_SOURCE_DIR}/../assets/tv-splash.png"
        DRCSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/../assets/drc-splash.png"
    )
endfunction()

add_static_retrou(retrou_nes nes "NES" "${FCEUMM_CORE}")
add_static_retrou(retrou_snes snes "SNES" "${SNES9X_CORE}")
add_static_retrou(retrou_gba gba "GBA" "${MGBA_CORE}")

# Official frontend/launcher. It has no core; it launches the matching RPX.
add_executable(retrou_launcher
    src/main.cpp
    src/rpl_core.cpp
    src/rpl_loader.cpp
)
target_compile_definitions(retrou_launcher PRIVATE RETROU_LAUNCHER=1)
configure_retrou(retrou_launcher)
wut_create_rpx(retrou_launcher)
wut_create_wuhb(retrou_launcher
    CONTENT    "${CMAKE_CURRENT_SOURCE_DIR}/content"
    NAME       "RetroU"
    SHORTNAME  "RetroU"
    AUTHOR     "joguel96"
    ICON       "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.png"
    TVSPLASH   "${CMAKE_CURRENT_SOURCE_DIR}/../assets/tv-splash.png"
    DRCSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/../assets/drc-splash.png"
)

# Test build with the synthetic core compiled into the RPX.
add_executable(retrou_test
    src/main.cpp
    src/rpl_core.cpp
    src/rpl_loader.cpp
    cores/test_core.cpp
)
target_compile_definitions(retrou_test PRIVATE
    RETROU_STATIC_CORE=1
    RETROU_TEST_CORE=1
)
configure_retrou(retrou_test)
wut_create_rpx(retrou_test)
stage_rpx(retrou_test "RetroU-Test.rpx")
wut_create_wuhb(retrou_test
    CONTENT    "${CMAKE_CURRENT_SOURCE_DIR}/content"
    NAME       "RetroU Test"
    SHORTNAME  "RetroU Test"
    AUTHOR     "joguel96"
    ICON       "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.png"
    TVSPLASH   "${CMAKE_CURRENT_SOURCE_DIR}/../assets/tv-splash.png"
    DRCSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/../assets/drc-splash.png"
)
