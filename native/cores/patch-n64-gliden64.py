#!/usr/bin/env python3
"""Add persistent startup milestones to the pinned GLideN64 motor."""

from pathlib import Path
import sys


def patch(source: Path, target: Path, replacements: list[tuple[str, str]]) -> None:
    data = source.read_text(encoding="utf-8")
    for old, new in replacements:
        if data.count(old) != 1:
            raise SystemExit(f"N64 GLideN64 patch: expected one occurrence: {old[:72]!r}")
        data = data.replace(old, new, 1)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(data, encoding="utf-8")


def main() -> None:
    if len(sys.argv) != 4:
        raise SystemExit("usage: patch-n64-gliden64.py GLIDEN64_SOURCE COMMON_OUTPUT WINDOW_OUTPUT")
    source = Path(sys.argv[1])
    patch(source / "common/CommonAPIImpl_common.cpp", Path(sys.argv[2]), [
        ('#include <PluginAPI.h>', '#include <PluginAPI.h>\nextern "C" {\n#include "report.h"\n}'),
        ('\tRSP_Init();\n\tGBI.init();\n\tConfig_LoadConfig();\n\tif (!dwnd().start())',
         '\treport_line("N64 GLideN64: RSP_Init begin");\n\tRSP_Init();\n'
         '\treport_line("N64 GLideN64: GBI.init begin");\n\tGBI.init();\n'
         '\treport_line("N64 GLideN64: Config_LoadConfig begin");\n\tConfig_LoadConfig();\n'
         '\treport_line("N64 GLideN64: dwnd.start begin");\n\tif (!dwnd().start())'),
        ('\t\treturn 0;\n#endif\n\n\tm_bRomOpen = true;',
         '\t\treturn 0;\n\treport_line("N64 GLideN64: dwnd.start done");\n#endif\n\n\tm_bRomOpen = true;'),
    ])
    patch(source / "DisplayWindow.cpp", Path(sys.argv[3]), [
        ('#include "DisplayWindow.h"', '#include "DisplayWindow.h"\nextern "C" {\n#include "report.h"\n}'),
        ('\tif (!_start())\n\t\treturn false;\n\n\tgraphics::ObjectHandle::defaultFramebuffer',
         '\treport_line("N64 GLideN64: window _start begin");\n'
         '\tif (!_start())\n\t\treturn false;\n'
         '\treport_line("N64 GLideN64: window _start done");\n\n'
         '\tgraphics::ObjectHandle::defaultFramebuffer'),
        ('\tgfxContext.init();\n\tm_drawer._initData();',
         '\treport_line("N64 GLideN64: gfxContext.init begin");\n'
         '\tgfxContext.init();\n'
         '\treport_line("N64 GLideN64: drawer.init begin");\n'
         '\tm_drawer._initData();\n'
         '\treport_line("N64 GLideN64: drawer.init done");'),
    ])


if __name__ == "__main__":
    main()
