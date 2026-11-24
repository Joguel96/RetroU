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
    revision=$3
    if [ -f "$cores/$name/.retrou-source-revision" ] && [ ! -d "$cores/$name/.git" ]; then
        test "$(cat "$cores/$name/.retrou-source-revision")" = "$revision"
        return
    fi
    if [ ! -d "$cores/$name/.git" ]; then
        git clone "$url" "$cores/$name"
    fi
    if [ "$(git -C "$cores/$name" rev-parse HEAD)" != "$revision" ]; then
        test -z "$(git -C "$cores/$name" status --porcelain)"
        git -C "$cores/$name" checkout --detach "$revision"
    fi
}

clone fceumm https://github.com/libretro/libretro-fceumm.git e0f3086a8104f8fbbe49a2615b10401504a9533f
clone snes9x https://github.com/libretro/snes9x.git fae2fea08f74180759ef540ee94259213f503480
clone mgba https://github.com/libretro/mgba.git 7a12d6d4b9acb14c0ae62c9166b6a2f3d08007f6

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
