#!/usr/bin/env python3
"""Keep the extracted engine's AX lifetime inside RetroU's SDL lifetime."""
from pathlib import Path
import sys

def replace_once(source, old, new):
    if source.count(old) != 1:
        raise SystemExit("N64 audio patch: source mismatch: " + old[:70])
    return source.replace(old, new, 1)

source = Path(sys.argv[1]).read_text(encoding="utf-8")
source = replace_once(source, 'static int g_ready;', 'static int g_ready;\nstatic int g_owns_ax;')
source = replace_once(source,
    '\tAXInitWithParams(&init);',
    '\tg_owns_ax = !AXIsInit();\n'
    '\tif (g_owns_ax) AXInitWithParams(&init);')
source = replace_once(source, '\tmemset(g_ring, 0, sizeof(g_ring));',
    '\tg_wr = g_rd = g_last_off = 0;\n'
    '\tg_playing = g_quiet = g_starting = g_ramping = 0;\n'
    '\tg_fill_avg = 0;\n'
    '\tg_last_push = g_wait_ticks = 0;\n'
    '\tg_last_count = g_underruns = g_pushed = g_dropped = g_dips = 0;\n'
    '\tg_lowest_pm = g_highest_pm = 0;\n'
    '\tg_unpaced = 0;\n'
    '\tmemset(g_ring, 0, sizeof(g_ring));')
source = replace_once(source, '\tAXQuit();',
    '\tg_playing = 0;\n'
    '\tg_voice[0] = g_voice[1] = NULL;\n'
    '\tif (g_owns_ax) AXQuit();\n'
    '\tg_owns_ax = 0;')
Path(sys.argv[2]).write_text(source, encoding="utf-8")
