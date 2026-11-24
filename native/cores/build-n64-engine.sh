#!/usr/bin/env bash
# Build only the Not64/GLideN64 motor from a pinned Our64 source revision.
set -euo pipefail

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../.." && pwd)
source_dir=${OUR64_SOURCE:-"$root/.cores/our64"}
seed=${OUR64_SEED:-https://github.com/Ch0nbo/Our64.git}
revision=2670a4cd4258e5dc848aa01322441c5c46a67061
adapted_revision=d3c56db253cd1b20987eb33ecd5797c60e527b64
source_patch="$here/our64-retrou.patch"
archive="$root/.cores/n64/not64_gliden64_wiiu.a"
mkdir -p "$root/native/build/logs" "$root/.cores/n64"
logfile="$root/native/build/logs/n64-engine-$(date +%Y%m%d-%H%M%S).log"
exec > >(tee "$logfile") 2>&1
echo "N64 engine build log: $logfile"

if [ -f "$source_dir/.retrou-source-revision" ] && [ ! -d "$source_dir/.git" ]; then
    test "$(cat "$source_dir/.retrou-source-revision")" = "$adapted_revision"
    echo "Our64 engine source: packaged adapted snapshot $adapted_revision"
else
    if [ ! -d "$source_dir/.git" ]; then
        git clone "$seed" "$source_dir"
    fi
    current_revision=$(git -C "$source_dir" rev-parse HEAD)
    if [ "$current_revision" = "$adapted_revision" ]; then
        test -z "$(git -C "$source_dir" status --porcelain)"
    elif [ "$current_revision" = "$revision" ] &&
         git -C "$source_dir" apply --reverse --check "$source_patch" 2>/dev/null; then
        test -z "$(git -C "$source_dir" ls-files --others --exclude-standard)"
        git -C "$source_dir" diff --binary --full-index HEAD > "$root/.cores/n64/source-check.patch"
        cmp "$source_patch" "$root/.cores/n64/source-check.patch"
    else
        test -z "$(git -C "$source_dir" status --porcelain)"
        git -C "$source_dir" checkout --detach "$revision"
        git -C "$source_dir" apply --check "$source_patch"
        git -C "$source_dir" apply "$source_patch"
    fi
    echo "Our64 engine source: $revision + RetroU adaptation ($adapted_revision)"
fi
echo "Excluded: main.c, library*, setup*, covers*, ui*, theme, pause*, input_wiiu.c, report.c, states_wiiu.c, tpak_wiiu.c"
if [ ! -f "$source_dir/third_party/cafeglsl/include/cafeglsl/CafeGLSLCompiler.h" ]; then
    if [ -n "${CAFEGLSL_SOURCE:-}" ]; then
        mkdir -p "$source_dir/third_party/cafeglsl"
        cp -a "$CAFEGLSL_SOURCE/." "$source_dir/third_party/cafeglsl/"
    else
        "$source_dir/tools/fetch-cafeglsl.sh"
    fi
fi
echo "CafeGLSL archive SHA-256: $(sha256sum "$source_dir/third_party/cafeglsl/lib/libcafeglsl.a")"

patched_audio="$root/.cores/n64/audio-retrou.c"
if command -v python3 >/dev/null 2>&1; then
    python3 "$here/patch-n64-audio.py" "$source_dir/player/source/audio.c" "$patched_audio"
elif command -v python >/dev/null 2>&1; then
    python "$here/patch-n64-audio.py" "$source_dir/player/source/audio.c" "$patched_audio"
else
    py -3 "$here/patch-n64-audio.py" "$source_dir/player/source/audio.c" "$patched_audio"
fi
patched_video="$root/.cores/n64/video-retrou.cpp"
if command -v python3 >/dev/null 2>&1; then
    python3 "$here/patch-n64-video.py" "$source_dir/player/source/video.cpp" "$patched_video"
elif command -v python >/dev/null 2>&1; then
    python "$here/patch-n64-video.py" "$source_dir/player/source/video.cpp" "$patched_video"
else
    py -3 "$here/patch-n64-video.py" "$source_dir/player/source/video.cpp" "$patched_video"
fi
echo "RetroU graphics adapter SHA-256: $(sha256sum "$patched_video")"

patched_common="$root/.cores/n64/gliden64-common-retrou.cpp"
patched_window="$root/.cores/n64/gliden64-window-retrou.cpp"
if command -v python3 >/dev/null 2>&1; then
    python3 "$here/patch-n64-gliden64.py" "$source_dir/third_party/gliden64/src" "$patched_common" "$patched_window"
elif command -v python >/dev/null 2>&1; then
    python "$here/patch-n64-gliden64.py" "$source_dir/third_party/gliden64/src" "$patched_common" "$patched_window"
else
    py -3 "$here/patch-n64-gliden64.py" "$source_dir/third_party/gliden64/src" "$patched_common" "$patched_window"
fi
echo "RetroU GLideN64 diagnostics SHA-256: $(sha256sum "$patched_common" "$patched_window")"

: "${DEVKITPRO:=/opt/devkitpro}"
export DEVKITPRO
rm -f "$archive"
make -C "$source_dir/player" -f Makefile -f "$here/n64-engine.mk" \
    RELEASE=1 N64_ENGINE_OUT="$archive" N64_ENGINE_PATCHED_AUDIO="$patched_audio" N64_ENGINE_PATCHED_VIDEO="$patched_video" \
    N64_ENGINE_PATCHED_GLIDEN64_COMMON="$patched_common" N64_ENGINE_PATCHED_GLIDEN64_WINDOW="$patched_window" \
    n64_engine -j"${JOBS:-4}"
ar_bin="$DEVKITPRO/devkitPPC/bin/powerpc-eabi-ar"
"$ar_bin" t "$archive" > "$root/.cores/n64/archive-members.txt"
if grep -E '^(main|library(_wiiu)?|setup(_wiiu)?|covers(_wiiu)?|ui(_wiiu|_draw)?|theme|pause(_wiiu)?|prompt|notices|input_wiiu|report|states_wiiu|tpak_wiiu|alloc_stats)\.o$' "$root/.cores/n64/archive-members.txt"; then
    echo "Frontend object found in the N64 engine archive" >&2
    exit 1
fi
echo "Archive member audit: no Our64 frontend objects"
echo "Engine archive SHA-256: $(sha256sum "$archive")"
