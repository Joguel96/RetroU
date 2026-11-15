#!/usr/bin/env bash
# Build the integrated N64 port and stage it beside the provisional app.
set -euo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/.." && pwd)
build="$here/build"
staged="$root/dist/wiiu/apps/RetroN64U-Dev/RetroN64U-Dev.wuhb"
mkdir -p "$build/logs"
logfile="$build/logs/n64-dev-$(date +%Y%m%d-%H%M%S).log"
exec > >(tee "$logfile") 2>&1
echo "RetroU N64 development build log: $logfile"

: "${DEVKITPRO:=/opt/devkitpro}"
: "${CMAKE_WIIU:=${DEVKITPRO}/portlibs/wiiu/bin/powerpc-eabi-cmake}"
: "${BUILD_TYPE:=Release}"
: "${JOBS:=$(nproc 2>/dev/null || echo 4)}"
export DEVKITPRO

echo "RetroU source commit: $(git -C "$root" rev-parse HEAD)"
"$here/cores/build-n64-engine.sh"
"$CMAKE_WIIU" -S "$here" -B "$build" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$build" --target stage_n64_dev -j"$JOBS"

cmp "$build/retrou_n64_gx2.wuhb" "$staged"
echo "Development WUHB: $staged"
sha256sum "$staged"
echo "Provisional RetroN64U.wuhb was not replaced"
