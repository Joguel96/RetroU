#!/usr/bin/env bash
# Build the integrated N64 port and stage its official RetroU bundle.
set -euo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/.." && pwd)
build="$here/build"
staged="$root/dist/wiiu/apps/RetroN64U/RetroN64U.wuhb"
mkdir -p "$build/logs"
logfile="$build/logs/n64-$(date +%Y%m%d-%H%M%S).log"
exec > >(tee "$logfile") 2>&1
echo "RetroU N64 build log: $logfile"

: "${DEVKITPRO:=/opt/devkitpro}"
: "${CMAKE_WIIU:=${DEVKITPRO}/portlibs/wiiu/bin/powerpc-eabi-cmake}"
: "${BUILD_TYPE:=Release}"
: "${JOBS:=$(nproc 2>/dev/null || echo 4)}"
export DEVKITPRO

echo "RetroU source commit: $(git -C "$root" rev-parse HEAD)"
"$here/cores/build-n64-engine.sh"
"$CMAKE_WIIU" -S "$here" -B "$build" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$build" --target stage_n64 -j"$JOBS"

cmp "$build/retrou_n64_gx2.wuhb" "$staged"
echo "RetroU N64 WUHB: $staged"
sha256sum "$staged"
