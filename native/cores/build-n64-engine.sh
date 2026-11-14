#!/usr/bin/env bash
# Build only the Not64/GLideN64 motor from a pinned Our64 source revision.
set -euo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
source_dir=${OUR64_SOURCE:-"$root/.cores/our64"}
seed=${OUR64_SEED:-https://github.com/Ch0nbo/Our64.git}
revision=d3c56db253cd1b20987eb33ecd5797c60e527b64
archive="$root/.cores/n64/not64_gliden64_wiiu.a"
mkdir -p "$root/native/build/logs" "$root/.cores/n64"
logfile="$root/native/build/logs/n64-engine-$(date +%Y%m%d-%H%M%S).log"
exec > >(tee "$logfile") 2>&1
echo "N64 engine build log: $logfile"

if [ ! -d "$source_dir/.git" ]; then
    git clone "$seed" "$source_dir"
fi
git -C "$source_dir" checkout --detach "$revision"
if [ -n "$(git -C "$source_dir" status --porcelain)" ]; then
    echo "Our64 source has local changes; use a clean checkout for a reproducible build" >&2
    exit 1
fi
echo "Our64 engine source: $(git -C "$source_dir" rev-parse HEAD)"
echo "Excluded: main.c, library*, setup*, covers*, ui*, theme, pause*, input*, report.c, states_wiiu.c, tpak_wiiu.c"
if [ ! -f "$source_dir/third_party/cafeglsl/include/cafeglsl/CafeGLSLCompiler.h" ]; then
    if [ -n "${CAFEGLSL_SOURCE:-}" ]; then
        mkdir -p "$source_dir/third_party/cafeglsl"
        cp -a "$CAFEGLSL_SOURCE/." "$source_dir/third_party/cafeglsl/"
    else
        "$source_dir/tools/fetch-cafeglsl.sh"
    fi
fi
echo "CafeGLSL archive SHA-256: $(sha256sum "$source_dir/third_party/cafeglsl/lib/libcafeglsl.a")"

: "${DEVKITPRO:=/opt/devkitpro}"
export DEVKITPRO
rm -f "$archive"
make -C "$source_dir/player" -f Makefile -f "$here/n64-engine.mk" \
    RELEASE=1 N64_ENGINE_OUT="$archive" n64_engine -j"${JOBS:-4}"
ar_bin="$DEVKITPRO/devkitPPC/bin/powerpc-eabi-ar"
"$ar_bin" t "$archive" > "$root/.cores/n64/archive-members.txt"
if grep -E '^(main|library(_wiiu)?|setup(_wiiu)?|covers(_wiiu)?|ui(_wiiu|_draw)?|theme|pause(_wiiu)?|prompt|notices|input(_wiiu)?|report|states_wiiu|tpak_wiiu|alloc_stats)\.o$' "$root/.cores/n64/archive-members.txt"; then
    echo "Frontend object found in the N64 engine archive" >&2
    exit 1
fi
echo "Archive member audit: no Our64 frontend objects"
echo "Engine archive SHA-256: $(sha256sum "$archive")"
