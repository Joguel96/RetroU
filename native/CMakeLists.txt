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

# Per-console default thumbnail (the RetroU icon), embedded as an object so it
# does not depend on the SD card. The symbol is always _binary_icon_png_start.
set(ICON_OBJECT_DIR "${CMAKE_CURRENT_BINARY_DIR}/embedded_icons")
foreach(ICON_SYSTEM nes snes gba n64)
    set(ICON_OBJ_${ICON_SYSTEM} "${ICON_OBJECT_DIR}/${ICON_SYSTEM}_icon.o")
    add_custom_command(
        OUTPUT "${ICON_OBJ_${ICON_SYSTEM}}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${ICON_OBJECT_DIR}"
        COMMAND "${OBJCOPY}" -I binary -O elf32-powerpc -B powerpc
                --rename-section .data=.rodata,alloc,load,readonly,data,contents
                icon.png "${ICON_OBJ_${ICON_SYSTEM}}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/${ICON_SYSTEM}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/${ICON_SYSTEM}/icon.png"
    )
endforeach()

# CTRL-3: controller image per console (assets/controles/<system>.png),
# embedded so the mapping editor does not depend on the SD card.
set(PAD_OBJECT_DIR "${CMAKE_CURRENT_BINARY_DIR}/embedded_pads")
foreach(PAD_SYSTEM nes snes gba n64)
    set(PAD_OBJ_${PAD_SYSTEM} "${PAD_OBJECT_DIR}/${PAD_SYSTEM}_pad.o")
    add_custom_command(
        OUTPUT "${PAD_OBJ_${PAD_SYSTEM}}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${PAD_OBJECT_DIR}"
        COMMAND "${OBJCOPY}" -I binary -O elf32-powerpc -B powerpc
                --rename-section .data=.rodata,alloc,load,readonly,data,contents
                ${PAD_SYSTEM}.png "${PAD_OBJ_${PAD_SYSTEM}}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../assets/controles"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/controles/${PAD_SYSTEM}.png"
    )
endforeach()

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
        curl
        mbedtls
        mbedx509
        mbedcrypto
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
        src/wpad_input.cpp
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

# GX2 renderer smoke test (not part of the default WUHB staging).
add_executable(retrou_gx2_test EXCLUDE_FROM_ALL
    src/gx2/gx2_test_main.cpp
    src/gx2/gx2_renderer.cpp
    src/gx2/gx2_text.cpp
    src/gx2/gx2_image.cpp
    src/gx2/retrou_data.cpp
    "${FONT_REGULAR_OBJ}"
)
set_source_files_properties("${FONT_REGULAR_OBJ}"
    PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
target_include_directories(retrou_gx2_test PRIVATE
    src
    /opt/devkitpro/wut/include
    /opt/devkitpro/portlibs/wiiu/include
    /opt/devkitpro/portlibs/ppc/include
    /opt/devkitpro/portlibs/ppc/include/freetype2
)
target_compile_options(retrou_gx2_test PRIVATE -Wall -Wextra)
target_link_libraries(retrou_gx2_test PRIVATE
    m
    freetype
    png
    bz2
    brotlidec
    brotlicommon
    z
)
wut_create_rpx(retrou_gx2_test)
wut_create_wuhb(retrou_gx2_test
    CONTENT    "${CMAKE_CURRENT_SOURCE_DIR}/content"
    NAME       "RetroU GX2 Test"
    SHORTNAME  "RetroU GX2 Test"
    AUTHOR     "joguel96"
    ICON       "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/nes/icon.png"
    TVSPLASH   "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/nes/tv-splash.png"
    DRCSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/nes/drc-splash.png"
)

# Minimal GX2 clear/present diagnostic.
add_executable(retrou_gx2_diag EXCLUDE_FROM_ALL
    src/gx2/gx2_diag_main.cpp
    src/gx2/gx2_renderer.cpp
    src/gx2/gx2_text.cpp
    "${FONT_REGULAR_OBJ}"
)
set_source_files_properties("${FONT_REGULAR_OBJ}"
    PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
target_include_directories(retrou_gx2_diag PRIVATE
    src
    /opt/devkitpro/wut/include
    /opt/devkitpro/portlibs/wiiu/include
    /opt/devkitpro/portlibs/ppc/include
    /opt/devkitpro/portlibs/ppc/include/freetype2
)
target_compile_options(retrou_gx2_diag PRIVATE -Wall -Wextra)
target_link_libraries(retrou_gx2_diag PRIVATE
    m
    freetype
    png
    bz2
    brotlidec
    brotlicommon
    z
)
wut_create_rpx(retrou_gx2_diag)
wut_create_wuhb(retrou_gx2_diag
    CONTENT    "${CMAKE_CURRENT_SOURCE_DIR}/content"
    NAME       "RetroU GX2 Diag"
    SHORTNAME  "RetroU GX2 Diag"
    AUTHOR     "joguel96"
    ICON       "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/nes/icon.png"
    TVSPLASH   "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/nes/tv-splash.png"
    DRCSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/nes/drc-splash.png"
)

# GX2 frontend per console: library UI + static core + GX2 frame present.
function(add_gx2_retrou target system label app_name core)
    add_executable(${target} EXCLUDE_FROM_ALL
        src/gx2/gx2_core_test_main.cpp
        src/gx2/gx2_renderer.cpp
        src/gx2/gx2_text.cpp
        src/gx2/gx2_image.cpp
        src/gx2/retrou_data.cpp
        src/wpad_input.cpp
        src/rpl_core.cpp
        src/rpl_loader.cpp
        src/retrou_assets.cpp
        ${RETRO_COMMON_SOURCES}
        "${FONT_REGULAR_OBJ}"
        "${ICON_OBJ_${system}}"
        "${PAD_OBJ_${system}}"
    )
    set_source_files_properties("${FONT_REGULAR_OBJ}"
        PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    set_source_files_properties("${ICON_OBJ_${system}}"
        PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    set_source_files_properties("${PAD_OBJ_${system}}"
        PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
    target_include_directories(${target} PRIVATE
        src
        "${LIBRETRO_COMMON}/include"
        /opt/devkitpro/wut/include
        /opt/devkitpro/portlibs/wiiu/include
        /opt/devkitpro/portlibs/wiiu/include/SDL2
        /opt/devkitpro/portlibs/ppc/include
        /opt/devkitpro/portlibs/ppc/include/freetype2
    )
    target_compile_definitions(${target} PRIVATE
        RETROU_STATIC_CORE=1
        RETROU_CORE_SYSTEM="${system}"
        RETROU_CORE_LABEL="${label}"
        RETROU_PAD_PNG=${system}
    )
    target_compile_options(${target} PRIVATE -Wall -Wextra)
    target_link_libraries(${target} PRIVATE
        "${core}"
        m
        SDL2
        curl
        mbedtls
        mbedx509
        mbedcrypto
        freetype
        png
        z
        bz2
        brotlidec
        brotlicommon
    )
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

add_gx2_retrou(retrou_nes_gx2 nes "NES" "RetroU NES" "${FCEUMM_CORE}")
add_gx2_retrou(retrou_snes_gx2 snes "SNES" "RetroU SNES" "${SNES9X_CORE}")
add_gx2_retrou(retrou_gba_gx2 gba "GBA" "RetroU GBA" "${MGBA_CORE}")

# Development build of the definitive N64 port. Keep its staged bundle separate
# from the provisional Our64-based RetroN64U app until the port is verified.
set(N64_ENGINE_CORE "${CMAKE_CURRENT_SOURCE_DIR}/../.cores/n64/not64_gliden64_wiiu.a"
    CACHE FILEPATH "extracted Not64 and GLideN64 engine")
set(N64_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../.cores/our64")
add_executable(retrou_n64_gx2 EXCLUDE_FROM_ALL
    src/gx2/gx2_core_test_main.cpp
    src/gx2/gx2_renderer.cpp
    src/gx2/gx2_text.cpp
    src/gx2/gx2_image.cpp
    src/gx2/retrou_data.cpp
    src/wpad_input.cpp
    src/retrou_assets.cpp
    src/n64/n64_engine.c
    src/n64/n64_report.c
    src/n64/n64_states.c
    src/n64/n64_video.cpp
    src/n64/n64_input.cpp
    src/n64/n64_session.cpp
    "${FONT_REGULAR_OBJ}"
    "${ICON_OBJ_n64}"
    "${PAD_OBJ_n64}"
)
set_source_files_properties("${ICON_OBJ_n64}" "${PAD_OBJ_n64}"
    PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE)
set_source_files_properties(src/n64/n64_engine.c src/n64/n64_states.c
    PROPERTIES COMPILE_OPTIONS
        "-std=gnu23;-fgnu89-inline;-fno-strict-aliasing;-Wno-old-style-declaration")
target_compile_definitions(retrou_n64_gx2 PRIVATE
    RETROU_N64_ENGINE=1
    RETROU_CORE_SYSTEM="n64"
    RETROU_CORE_LABEL="N64"
    RETROU_PAD_PNG=n64
    __WIIU__ __WUT__ HW_RVL NOASM USE_EXPANSION USE_RECOMP_CACHE PPC_DYNAREC FASTMEM RELEASE PROFILE
)
target_include_directories(retrou_n64_gx2 PRIVATE
    src
    "${LIBRETRO_COMMON}/include"
    "${N64_SOURCE}/bench/shim"
    "${N64_SOURCE}/bench/source"
    "${N64_SOURCE}"
    "${N64_SOURCE}/player/source"
    "${N64_SOURCE}/third_party/not64"
    "${N64_SOURCE}/third_party/minilzo"
    /opt/devkitpro/wut/include
    /opt/devkitpro/portlibs/wiiu/include
    /opt/devkitpro/portlibs/wiiu/include/SDL2
    /opt/devkitpro/portlibs/ppc/include
    /opt/devkitpro/portlibs/ppc/include/freetype2
)
target_compile_options(retrou_n64_gx2 PRIVATE -Wall -Wextra)
target_link_options(retrou_n64_gx2 PRIVATE -Wl,--wrap=doRspCycles)
target_link_libraries(retrou_n64_gx2 PRIVATE
    "${N64_ENGINE_CORE}"
    "${N64_SOURCE}/third_party/cafeglsl/lib/libcafeglsl.a"
    turbojpeg
    SDL2 curl mbedtls mbedx509 mbedcrypto
    freetype png16 z bz2 brotlidec brotlicommon m
)
wut_create_rpx(retrou_n64_gx2)
wut_create_wuhb(retrou_n64_gx2
    CONTENT    "${CMAKE_CURRENT_SOURCE_DIR}/content"
    NAME       "RetroU N64 (desarrollo)"
    SHORTNAME  "RetroU N64 Dev"
    AUTHOR     "joguel96"
    ICON       "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/n64/icon.png"
    TVSPLASH   "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/n64/tv-splash.png"
    DRCSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/../assets/console/n64/drc-splash.png"
)
add_custom_target(stage_n64_dev
    COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../dist/wiiu/apps/RetroN64U-Dev"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/retrou_n64_gx2.wuhb"
            "${CMAKE_CURRENT_SOURCE_DIR}/../dist/wiiu/apps/RetroN64U-Dev/RetroN64U-Dev.wuhb"
    DEPENDS retrou_n64_gx2_wuhb
    COMMENT "Staging the separate RetroU N64 development bundle"
)

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
