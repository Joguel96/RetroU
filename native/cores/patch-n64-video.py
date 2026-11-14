#!/usr/bin/env python3
"""Adapt the pinned N64 graphics engine for repeated RetroU game sessions.

The Our64 checkout stays unmodified. Fail if its expected start/stop code
changes so a different upstream revision cannot silently drop this fix.
"""

from pathlib import Path
import sys


def replace_once(source: str, old: str, new: str) -> str:
    if source.count(old) != 1:
        raise SystemExit(f"N64 video patch: expected one occurrence: {old[:72]!r}")
    return source.replace(old, new, 1)


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: patch-n64-video.py SOURCE OUTPUT")
    source = Path(sys.argv[1]).read_text(encoding="utf-8")
    source = replace_once(
        source,
        'extern "C" int video_thread_start(unsigned core)\n{\n\tplayer_get_memory(&g_mem);',
        'extern "C" int video_thread_start(unsigned core)\n{\n'
        '\t// RetroU gives GX2 back to the library between games. Each new\n'
        '\t// graphics thread needs an empty command ring and a fresh handshake.\n'
        '\tg_quit = g_ready = g_gfx_ok = false;\n'
        '\tg_posted = g_done = 0;\n'
        '\tg_tasks_posted = g_tasks_done = 0;\n'
        '\tg_screens_posted = g_screens_run = 0;\n'
        '\tplayer_get_memory(&g_mem);',
    )
    source = replace_once(
        source,
        '\tOSSetThreadName(&g_thread, "Our64 graphics");\n'
        '\tOSResumeThread(&g_thread);\n'
        '\twhile (!g_ready)\n'
        '\t\tOSWaitEvent(&g_done_event);\n'
        '\treturn g_gfx_ok ? 0 : -1;',
        '\tOSSetThreadName(&g_thread, "RetroU N64 graphics");\n'
        '\tOSResumeThread(&g_thread);\n'
        '\twhile (!g_ready)\n'
        '\t\tOSWaitEvent(&g_done_event);\n'
        '\tif (!g_gfx_ok) {\n'
        '\t\tint ret;\n'
        '\t\tOSJoinThread(&g_thread, &ret);\n'
        '\t\tg_ready = false;\n'
        '\t\treturn -1;\n'
        '\t}\n'
        '\treturn 0;',
    )
    source = replace_once(
        source,
        '\tOSJoinThread(&g_thread, &ret);\n\tg_gfx_ok = false;\n}',
        '\tOSJoinThread(&g_thread, &ret);\n'
        '\tg_gfx_ok = false;\n'
        '\tg_ready = false;\n}',
    )
    target = Path(sys.argv[2])
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(source, encoding="utf-8")


if __name__ == "__main__":
    main()
