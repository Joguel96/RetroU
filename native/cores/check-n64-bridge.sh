#!/usr/bin/env bash
# Compila y enlaza el puente N64; el binario resultante no se distribuye.
set -euo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
source_dir="$root/.cores/our64"
archive="$root/.cores/n64/not64_gliden64_wiiu.a"
out="$root/native/build/n64-bridge-check"
log="$root/native/build/logs/n64-bridge-$(date +%Y%m%d-%H%M%S).log"
mkdir -p "$out" "$root/native/build/logs"
exec > >(tee "$log") 2>&1

: "${DEVKITPRO:=/opt/devkitpro}"
cc="$DEVKITPRO/devkitPPC/bin/powerpc-eabi-gcc"
cxx="$DEVKITPRO/devkitPPC/bin/powerpc-eabi-g++"
test -f "$archive"
test -f "$source_dir/third_party/cafeglsl/lib/libcafeglsl.a"
echo "Puente N64: $(git -C "$root" rev-parse --short HEAD)"
echo "Motor: $(git -C "$source_dir" rev-parse HEAD)"
echo "Biblioteca: $(sha256sum "$archive")"

includes=(
    "-I$source_dir/bench/shim" "-I$source_dir/bench/source"
    "-I$source_dir/player/source" "-I$source_dir/third_party/not64"
    "-I$source_dir/third_party/minilzo" "-I$DEVKITPRO/wut/include"
    "-I$DEVKITPRO/portlibs/wiiu/include"
)
"$cc" -std=gnu23 -O2 -Wall -Wextra -Werror -fgnu89-inline \
    -fno-strict-aliasing -D__WIIU__ -D__WUT__ -DHW_RVL -DNOASM \
    -DUSE_EXPANSION -DUSE_RECOMP_CACHE -DPPC_DYNAREC -DFASTMEM \
    -DRELEASE -DPROFILE "${includes[@]}" \
    -c "$root/native/src/n64/n64_engine.c" -o "$out/engine.o"
"$cc" -std=gnu11 -O2 -Wall -Wextra -Werror \
    "-I$DEVKITPRO/wut/include" \
    -c "$root/native/src/n64/n64_report.c" -o "$out/report.o"
"$cxx" -std=gnu++17 -O2 -Wall -Wextra -Werror \
    "-I$source_dir/bench/source" "-I$DEVKITPRO/wut/include" \
    -c "$root/native/src/n64/n64_video.cpp" -o "$out/video.o"
"$cxx" -std=gnu++17 -O2 -Wall -Wextra -Werror \
    "-I$root/native/src" "-I$source_dir/player/source" \
    "-I$DEVKITPRO/wut/include" \
    -c "$root/native/src/n64/n64_input.cpp" -o "$out/input.o"
"$cxx" -std=gnu++17 -O2 -Wall -Wextra -Werror \
    "-I$DEVKITPRO/wut/include" \
    -c "$root/native/src/wpad_input.cpp" -o "$out/wpad_input.o"
"$cc" -std=gnu23 -O2 -Wall -Wextra -Werror \
    -c "$here/probes/n64-link.c" -o "$out/probe.o"

"$cxx" -mcpu=750 -meabi -mhard-float -O2 \
    "-L$DEVKITPRO/wut/lib" "-L$DEVKITPRO/portlibs/wiiu/lib" \
    "-L$DEVKITPRO/portlibs/ppc/lib" \
    "-L$source_dir/third_party/cafeglsl/lib" \
    "-specs=$DEVKITPRO/wut/share/wut.specs" -Wl,--wrap=doRspCycles \
    "$out/probe.o" "$out/engine.o" "$out/report.o" "$out/video.o" \
    "$out/input.o" "$out/wpad_input.o" \
    "$archive" -lcafeglsl -lturbojpeg -lfreetype -lpng16 -lbz2 -lz \
    -lwut -lm -o "$out/probe.elf"
echo "Enlace diagnóstico correcto: $(sha256sum "$out/probe.elf")"
echo "Estados vacíos exclusivamente en probes/n64-link.c; no se distribuye."
