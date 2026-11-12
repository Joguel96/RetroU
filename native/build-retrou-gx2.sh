#!/usr/bin/env bash
# Build the GX2 RetroU apps and stage their WUHB files for distribution.
set -euo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/.." && pwd)
build="$here/build"
mkdir -p "$build/logs"
logfile="$build/logs/build-$(date +%Y%m%d-%H%M%S).log"
exec > >(tee "$logfile") 2>&1
echo "RetroU GX2 build log: $logfile"

: "${DEVKITPRO:=/opt/devkitpro}"
: "${CMAKE_WIIU:=${DEVKITPRO}/portlibs/wiiu/bin/powerpc-eabi-cmake}"
: "${BUILD_TYPE:=Release}"
: "${JOBS:=$(nproc 2>/dev/null || echo 4)}"

export DEVKITPRO

"$CMAKE_WIIU" -S "$here" -B "$build" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$build" \
    --target retrou_nes_gx2_wuhb retrou_snes_gx2_wuhb retrou_gba_gx2_wuhb \
    -j"$JOBS"

stage() {
    source=$1
    app=$2
    mkdir -p "$root/dist/wiiu/apps/$app"
    cp -f "$build/$source" "$root/dist/wiiu/apps/$app/$app.wuhb"
}

stage retrou_nes_gx2.wuhb RetroNESU
stage retrou_snes_gx2.wuhb RetroSNESU
stage retrou_gba_gx2.wuhb RetroGBAU

echo "Staged GX2 WUHB files:"
printf '  %s\n' \
    "$root/dist/wiiu/apps/RetroNESU/RetroNESU.wuhb" \
    "$root/dist/wiiu/apps/RetroSNESU/RetroSNESU.wuhb" \
    "$root/dist/wiiu/apps/RetroGBAU/RetroGBAU.wuhb"
