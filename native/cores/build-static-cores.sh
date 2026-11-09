#!/bin/sh
# Build the static Libretro cores used by the RPX apps.
set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
cores="$root/.cores"

: "${DEVKITPRO:=/opt/devkitpro}"
: "${DEVKITPPC:=${DEVKITPRO}/devkitPPC}"
export DEVKITPRO DEVKITPPC

mkdir -p "$cores"

clone() {
    name=$1
    url=$2
    if [ ! -d "$cores/$name" ]; then
        git clone --depth 1 "$url" "$cores/$name"
    fi
}

clone fceumm https://github.com/libretro/libretro-fceumm.git
clone snes9x https://github.com/libretro/snes9x.git
clone mgba https://github.com/libretro/mgba.git

make -C "$cores/fceumm" platform=wiiu -j"$(nproc 2>/dev/null || echo 4)"
make -C "$cores/snes9x/libretro" platform=wiiu -j"$(nproc 2>/dev/null || echo 4)"

# mgba's bundled minizip assumes fopen64 on this newlib; use fopen.
if ! grep -q "define USE_FILE32API" "$cores/mgba/src/third-party/zlib/contrib/minizip/ioapi.c"; then
    sed -i '1i #define USE_FILE32API' "$cores/mgba/src/third-party/zlib/contrib/minizip/ioapi.c"
fi

"${DEVKITPRO}/portlibs/wiiu/bin/powerpc-eabi-cmake" \
    -S "$cores/mgba" -B "$cores/mgba/build" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_LIBRETRO=ON -DSKIP_LIBRARY=ON
cmake --build "$cores/mgba/build" --target mgba_libretro -j"$(nproc 2>/dev/null || echo 4)"

echo "OK:"
echo "  $cores/fceumm/fceumm_libretro_wiiu.a"
echo "  $cores/snes9x/libretro/snes9x_libretro_wiiu.a"
echo "  $cores/mgba/build/mgba_libretro.a"
