cmake_minimum_required(VERSION 3.14)
project(RetroU C CXX)

# =============================================================================
# RetroU - Frontend Wii U para ROMs retro via cores Libretro
# =============================================================================
#
# TOOLCHAIN (devkitPro CMake moderno):
#   El toolchain wut.toolchain.cmake esta DEPRECADO. La forma oficial es
#   invocar CMake a traves del wrapper de devkitPro (instalado con el paquete
#   wiiu-cmake), que configura -DCMAKE_SYSTEM_NAME=CafeOS y el toolchain
#   devkitPPC por nosotros:
#
#     Linux/macOS:   DEVKITPRO=/opt/devkitpro \
#                    $DEVKITPRO/portlibs/wiiu/bin/powerpc-eabi-cmake -B build -DCMAKE_BUILD_TYPE=Release
#
#     Windows (MSYS2/devkitPro): C:/devkitPro/portlibs/wiiu/bin/powerpc-eabi-cmake -B build -DCMAKE_BUILD_TYPE=Release
#
#   Build:  cmake --build build -j[ncpu]
#   Salida: build/retroU.rpx  (via `wut_create_rpx` -> elf2rpl POST_BUILD)
#
# PAQUETES devkitPro requeridos (devkitProUpdater / pacman):
#   devkitPPC, wut (WUT_ROOT=$DEVKITPRO/wut), wut-tools (elf2rpl, wuhbtool),
#   wiiu-cmake, wiiu-pkg-config
# =============================================================================

# --- Estándar del lenguaje: C++17 (obligatorio, definido en AGENTS.md) -------
set(CMAKE_C_STANDARD            11)
set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)  # Evitar extensiones GNU accidentales

# --- Ejecutable nativo -------------------------------------------------------
# add_executable en CafeOS produce un .elf; wut_create_rpx() lo convierte a
# .rpx con elf2rpl mediante un POST_BUILD (funciones del paquete wiiu-cmake).
add_executable(retroU
    src/main.cpp
    src/gfx/renderer.cpp
    src/gfx/texture_loader.cpp
    src/ui/menu.cpp
    src/ui/grid.cpp
    src/core/launcher.cpp
    src/fs/fs.cpp
    src/fs/scanner.cpp
    third_party/stb/stb_image_impl.cpp
)

# src/ para los modulos propios; third_party/stb para "stb_image.h"
# (decodificador PNG/JPG de dominio publico, usado por texture_loader).
target_include_directories(retroU PRIVATE src third_party/stb)

# stb_image es codigo de terceros: se compila sin nuestros warnings
# estrictos (no lo controlamos; -w solo en este archivo).
set_source_files_properties(third_party/stb/stb_image_impl.cpp
    PROPERTIES COMPILE_OPTIONS "-w")

# --- Librerias Cafe OS --------------------------------------------------------
# NO hace falta enlazar gx2/vpad/coreinit/sndcore2/sysapp de forma explicita:
# libwut.a (generada desde los cafe/*.def del repo WUT, verificado con
# powerpc-eabi-nm: contiene OSScreenInit, GX2Init, VPADRead, SYSLaunchMenu...)
# ya incluye los import stubs de toda la API de Cafe OS.
# El toolchain CafeOS enlaza automaticamente -lwut -lm (WUT_STANDARD_LIBRARIES).

# --- Convertir a formato RPX de Wii U ---------------------------------------
wut_create_rpx(retroU)

# --- Dash de Aroma (WUHB) tras generar el .rpx --------------------------------
# Aroma NO muestra .rpx en su menu de homebrew: usa el formato .wuhb
# (Wii U Homebrew Bundle: icono + splash TV/DRC + el binario .rpx dentro).
# El flujo correcto es: wut_create_rpx -> wut_create_wuhb (usa wuhbtool de
# wut-tools). El .wuhb va a /wiiu/apps/... en la SD junto a su icono.
wut_create_wuhb(retroU
    NAME       "RetroU"
    SHORTNAME  "RetroU"
    AUTHOR     "joguel96"
    ICON       "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png"
    TVSPLASH   "${CMAKE_CURRENT_SOURCE_DIR}/assets/tv-splash.png"
    DRCSPLASH  "${CMAKE_CURRENT_SOURCE_DIR}/assets/drc-splash.png"
)

# --- Warnings -----------------------------------------------------------------
# Lenientes por defecto pero visibles para ir endureciendo con el tiempo.
target_compile_options(retroU PRIVATE
    -Wall
    -Wextra
    -pedantic
)
