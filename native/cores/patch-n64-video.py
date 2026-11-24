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
        '\t// Task IDs are shared with plugins.c and remain monotonic across ROMs.\n'
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
    source = replace_once(
        source,
        '\tif (!g_plugin_up && PluginStartup(nullptr, nullptr, debug) != M64ERR_SUCCESS) {',
        '\treport_line("N64 video: PluginStartup begin");\n'
        '\tif (!g_plugin_up && PluginStartup(nullptr, nullptr, debug) != M64ERR_SUCCESS) {',
    )
    source = replace_once(
        source,
        '\tg_plugin_up = true;\n\tour64_gx2_shader_progress = ui_gfx_loading_progress;\n'
        '\tif (!InitiateGFX(gi)) {',
        '\tg_plugin_up = true;\n'
        '\treport_line("N64 video: PluginStartup done");\n'
        '\tour64_gx2_shader_progress = ui_gfx_loading_progress;\n'
        '\treport_line("N64 video: InitiateGFX begin");\n'
        '\tif (!InitiateGFX(gi)) {',
    )
    source = replace_once(
        source,
        '\tif (!RomOpen()) {',
        '\treport_line("N64 video: InitiateGFX done");\n'
        '\treport_line("N64 video: RomOpen begin");\n'
        '\tif (!RomOpen()) {',
    )
    source = replace_once(
        source,
        '\tViStatusChanged();\n\tViWidthChanged();\n\tg_started = true;',
        '\treport_line("N64 video: RomOpen done");\n'
        '\tViStatusChanged();\n\tViWidthChanged();\n\tg_started = true;',
    )
    source = replace_once(
        source,
        '\tWHBGfxShutdown();\n\treturn 0;',
        '\t// RetroU destroys GX2 between ROMs, unlike the original player.\n'
        '\tif (g_plugin_up) {\n'
        '\t\tPluginShutdown();\n'
        '\t\tg_plugin_up = false;\n'
        '\t}\n'
        '\tg_hold = false;\n'
        '\tWHBGfxShutdown();\n\treturn 0;',
    )
    target = Path(sys.argv[2])
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(source, encoding="utf-8")


if __name__ == "__main__":
    main()
