#!/bin/sh
# Build fceumm as an Aroma .rpl module for RetroU.
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
fceumm="$root/.spike_fceumm"
out="$root/dist/retrou/cores"

: "${DEVKITPRO:=/opt/devkitpro}"
: "${DEVKITPPC:=${DEVKITPRO}/devkitPPC}"
export DEVKITPRO DEVKITPPC

if [ ! -d "$fceumm" ]; then
    git clone --depth 1 https://github.com/libretro/libretro-fceumm.git "$fceumm"
fi

make -C "$fceumm" platform=wiiu -j"$(nproc 2>/dev/null || echo 4)"

"${DEVKITPRO}/portlibs/wiiu/bin/powerpc-eabi-cmake" \
    -S "$here/fceumm_rpl" -B "$here/fceumm_rpl/build" \
    -DCMAKE_BUILD_TYPE=Release -DFCEUMM_ROOT="$fceumm"
cmake --build "$here/fceumm_rpl/build" -j"$(nproc 2>/dev/null || echo 4)"

mkdir -p "$out"
cp -f "$here/fceumm_rpl/build/fceumm_rpl.rpl" "$out/fceumm.rpl"
echo "OK: $out/fceumm.rpl"
