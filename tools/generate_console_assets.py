#!/usr/bin/env python3
"""Generate per-console RetroU icons and splash screens.

The palette and glass composition follow the original RetroU assets:
dark blue-grey background, translucent rounded glass panel, white controller.
"""
from __future__ import annotations

import math
import os
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets"
FONT_BOLD = ROOT / "native" / "content" / "ui-bold.ttf"
FONT_REGULAR = ROOT / "native" / "content" / "ui.ttf"

CONSOLES = {
    "nes": {
        "name": "NES",
        "app": "RNESU",
        "controller": "nes",
    },
    "snes": {
        "name": "SNES",
        "app": "RSNESU",
        "controller": "snes",
    },
    "gba": {
        "name": "GBA",
        "app": "RGBAU",
        "controller": "gba",
    },
    "n64": {
        "name": "N64",
        "app": "RN64U",
        "controller": "n64",
    },
}


def lerp(a: tuple[int, int, int], b: tuple[int, int, int], t: float) -> tuple[int, int, int]:
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(3))


def font(size: int, bold: bool = True) -> ImageFont.FreeTypeFont:
    return ImageFont.truetype(str(FONT_BOLD if bold else FONT_REGULAR), size)


def rounded_panel(size: tuple[int, int], radius: int, top, bottom, border, scale: int = 4):
    w, h = size[0] * scale, size[1] * scale
    r = radius * scale
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    mask = Image.new("L", (w, h), 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle((0, 0, w - 1, h - 1), radius=r, fill=255)

    grad = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(grad)
    for y in range(h):
        c = lerp(top, bottom, y / max(1, h - 1))
        gd.line((0, y, w, y), fill=(*c, 255))
    image = Image.composite(grad, image, mask)

    draw = ImageDraw.Draw(image)
    # Diagonal glass highlight, clipped by the rounded mask.
    glare = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glare)
    gd.polygon(
        [
            (int(w * 0.54), 0),
            (int(w * 0.76), 0),
            (int(w * 0.38), h),
            (int(w * 0.16), h),
        ],
        fill=(255, 255, 255, 22),
    )
    glare = Image.composite(glare, Image.new("RGBA", (w, h), (0, 0, 0, 0)), mask)
    image = Image.alpha_composite(image, glare)

    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle(
        (0, 0, w - 1, h - 1),
        radius=r,
        outline=(*border, 235),
        width=max(2, 2 * scale),
    )
    return image.resize(size, Image.Resampling.LANCZOS)


def background(size: tuple[int, int]) -> Image.Image:
    w, h = size
    im = Image.new("RGBA", (w, h), (38, 43, 48, 255))
    d = ImageDraw.Draw(im)
    for y in range(h):
        t = y / max(1, h - 1)
        c = lerp((46, 51, 57), (27, 30, 34), t)
        d.line((0, y, w, y), fill=(*c, 255))
    # Soft center glow.
    glow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse(
        (w * 0.18, h * 0.08, w * 0.82, h * 0.95),
        fill=(110, 120, 130, 22),
    )
    glow = glow.filter(ImageFilter.GaussianBlur(max(12, w // 60)))
    return Image.alpha_composite(im, glow)


def draw_controller(draw: ImageDraw.ImageDraw, kind: str, box: tuple[int, int, int, int],
                    white=(250, 251, 252, 255), dark=(47, 57, 67, 255),
                    mid=(84, 94, 104, 255)):
    x0, y0, x1, y1 = box
    w, h = x1 - x0, y1 - y0
    cx, cy = (x0 + x1) // 2, (y0 + y1) // 2
    lw = max(2, h // 30)

    if kind == "nes":
        body = (x0, y0 + h * 0.12, x1, y1 - h * 0.12)
        draw.rounded_rectangle(body, radius=h * 0.18, fill=white)
        # D-pad
        pad = h * 0.30
        px, py = x0 + w * 0.24, cy
        draw.rounded_rectangle((px - pad, py - pad * 0.34, px + pad, py + pad * 0.34),
                               radius=h * 0.04, fill=dark)
        draw.rounded_rectangle((px - pad * 0.34, py - pad, px + pad * 0.34, py + pad),
                               radius=h * 0.04, fill=dark)
        # A/B
        br = h * 0.14
        for i, bx in enumerate((x0 + w * 0.70, x0 + w * 0.82)):
            by = cy + (h * 0.10 if i else -h * 0.10)
            draw.ellipse((bx - br, by - br, bx + br, by + br), fill=dark)
        # Select/start
        sw, sh = w * 0.10, h * 0.075
        for bx in (x0 + w * 0.40, x0 + w * 0.52):
            draw.rounded_rectangle((bx - sw / 2, cy + h * 0.26,
                                    bx + sw / 2, cy + h * 0.26 + sh),
                                   radius=sh / 2, fill=mid)
    elif kind == "snes":
        body = (x0, y0 + h * 0.16, x1, y1 - h * 0.10)
        draw.rounded_rectangle(body, radius=h * 0.30, fill=white)
        # Shoulder lobes
        draw.ellipse((x0 - h * 0.04, y0 + h * 0.06, x0 + w * 0.28, y0 + h * 0.44), fill=white)
        draw.ellipse((x1 - w * 0.28, y0 + h * 0.06, x1 + h * 0.04, y0 + h * 0.44), fill=white)
        pad = h * 0.26
        px, py = x0 + w * 0.25, cy + h * 0.05
        draw.rounded_rectangle((px - pad, py - pad * 0.34, px + pad, py + pad * 0.34),
                               radius=h * 0.04, fill=dark)
        draw.rounded_rectangle((px - pad * 0.34, py - pad, px + pad * 0.34, py + pad),
                               radius=h * 0.04, fill=dark)
        br = h * 0.115
        for bx, by in ((0.68, -0.15), (0.80, -0.15), (0.74, 0.08), (0.86, 0.08)):
            bx = x0 + w * bx
            by = cy + h * by
            draw.ellipse((bx - br, by - br, bx + br, by + br), fill=dark)
        sw, sh = w * 0.065, h * 0.065
        for bx in (x0 + w * 0.43, x0 + w * 0.52):
            draw.rounded_rectangle((bx - sw / 2, cy + h * 0.24,
                                    bx + sw / 2, cy + h * 0.24 + sh),
                                   radius=sh / 2, fill=mid)
    elif kind == "gba":
        # Handheld body.
        body = (x0 + w * 0.04, y0 + h * 0.12, x1 - w * 0.04, y1 - h * 0.12)
        draw.rounded_rectangle(body, radius=h * 0.28, fill=white)
        # Screen.
        sx0, sy0 = x0 + w * 0.26, y0 + h * 0.20
        sx1, sy1 = x1 - w * 0.26, y1 - h * 0.22
        draw.rounded_rectangle((sx0, sy0, sx1, sy1), radius=h * 0.06, fill=dark)
        draw.rounded_rectangle((sx0 + w * 0.02, sy0 + h * 0.02,
                                sx1 - w * 0.02, sy1 - h * 0.02),
                               radius=h * 0.04, fill=(32, 39, 47, 255))
        # D-pad.
        pad = h * 0.20
        px, py = x0 + w * 0.14, cy + h * 0.06
        draw.rounded_rectangle((px - pad, py - pad * 0.32, px + pad, py + pad * 0.32),
                               radius=h * 0.03, fill=mid)
        draw.rounded_rectangle((px - pad * 0.32, py - pad, px + pad * 0.32, py + pad),
                               radius=h * 0.03, fill=mid)
        # A/B.
        br = h * 0.10
        for bx, by in ((0.86, 0.14), (0.78, 0.26)):
            bx = x0 + w * bx
            by = y0 + h * by
            draw.ellipse((bx - br, by - br, bx + br, by + br), fill=mid)
        # Shoulders.
        draw.rounded_rectangle((x0 + w * 0.12, y0 + h * 0.02, x0 + w * 0.36, y0 + h * 0.13),
                               radius=h * 0.05, fill=white)
        draw.rounded_rectangle((x1 - w * 0.36, y0 + h * 0.02, x1 - w * 0.12, y0 + h * 0.13),
                               radius=h * 0.05, fill=white)
    elif kind == "n64":
        # Three-pronged controller, seen straight on: central body with
        # D-pad, analog stick and the two face buttons (A, B).
        body = (x0 + w * 0.18, y0 + h * 0.14, x1 - w * 0.18, y1 - h * 0.14)
        draw.rounded_rectangle(body, radius=h * 0.22, fill=white)
        # D-pad (left).
        pad = h * 0.16
        px, py = x0 + w * 0.32, cy
        draw.rounded_rectangle((px - pad, py - pad * 0.34, px + pad, py + pad * 0.34),
                               radius=h * 0.03, fill=dark)
        draw.rounded_rectangle((px - pad * 0.34, py - pad, px + pad * 0.34, py + pad),
                               radius=h * 0.03, fill=dark)
        # Analog stick (right).
        sr = h * 0.17
        sx, sy = x0 + w * 0.68, cy - h * 0.12
        draw.ellipse((sx - sr, sy - sr, sx + sr, sy + sr), fill=dark)
        sr2 = h * 0.10
        draw.ellipse((sx - sr2, sy - sr2, sx + sr2, sy + sr2), fill=mid)
        # A (big, lower right) and B (small, upper right).
        a = h * 0.14
        draw.ellipse((x0 + w * 0.86 - a, cy + h * 0.16 - a,
                      x0 + w * 0.86 + a, cy + h * 0.16 + a), fill=dark)
        b = h * 0.10
        draw.ellipse((x0 + w * 0.78 - b, cy - h * 0.10 - b,
                      x0 + w * 0.78 + b, cy - h * 0.10 + b), fill=mid)
        # Start (small, center bottom).
        sw, sh = w * 0.07, h * 0.05
        draw.rounded_rectangle((cx - sw / 2, y1 - h * 0.32,
                                cx + sw / 2, y1 - h * 0.32 + sh),
                               radius=sh / 2, fill=mid)


def make_icon(kind: str, out: Path):
    size = 128
    scale = 6
    bg = background((size * scale, size * scale)).resize((size, size), Image.Resampling.LANCZOS)
    panel = rounded_panel(
        (104 * scale, 104 * scale),
        22 * scale,
        (126, 134, 143),
        (62, 72, 82),
        (206, 214, 222),
        scale=1,
    )
    canvas = Image.new("RGBA", (size * scale, size * scale), (0, 0, 0, 0))
    canvas.alpha_composite(panel, (12 * scale, 12 * scale))
    draw = ImageDraw.Draw(canvas)
    draw_controller(draw, kind, (24 * scale, 46 * scale, 104 * scale, 84 * scale))
    canvas = canvas.resize((size, size), Image.Resampling.LANCZOS)
    out.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(out, optimize=True)
    return canvas


def make_tv(kind: str, name: str, out: Path):
    size = (1280, 720)
    scale = 2
    canvas = background((size[0] * scale, size[1] * scale))
    panel = rounded_panel(
        (680 * scale, 360 * scale),
        42 * scale,
        (122, 127, 132),
        (72, 77, 83),
        (216, 222, 228),
        scale=1,
    )
    canvas.alpha_composite(panel, (300 * scale, 180 * scale))
    draw = ImageDraw.Draw(canvas)
    draw_controller(draw, kind, (390 * scale, 300 * scale, 520 * scale, 405 * scale))
    draw.text((540 * scale, 292 * scale), "RetroU", font=font(96 * scale),
              fill=(247, 248, 250, 255))
    draw.text((640 * scale, 425 * scale), name, font=font(54 * scale),
              fill=(236, 239, 243, 255), anchor="mm")
    canvas = canvas.resize(size, Image.Resampling.LANCZOS)
    out.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(out, optimize=True)
    return canvas


def make_drc(kind: str, name: str, out: Path):
    size = (854, 480)
    scale = 3
    canvas = background((size[0] * scale, size[1] * scale))
    panel = rounded_panel(
        (258 * scale, 258 * scale),
        34 * scale,
        (122, 128, 135),
        (68, 78, 88),
        (214, 221, 228),
        scale=1,
    )
    canvas.alpha_composite(panel, (298 * scale, 112 * scale))
    draw = ImageDraw.Draw(canvas)
    draw_controller(draw, kind, (335 * scale, 150 * scale, 520 * scale, 220 * scale))
    draw.text((427 * scale, 250 * scale), "RetroU", font=font(58 * scale),
              fill=(247, 248, 250, 255), anchor="mm")
    draw.text((427 * scale, 315 * scale), name, font=font(34 * scale),
              fill=(226, 231, 236, 255), anchor="mm")
    canvas = canvas.resize(size, Image.Resampling.LANCZOS)
    out.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(out, optimize=True)
    return canvas


def main():
    for kind, info in CONSOLES.items():
        out = ASSETS / "console" / kind
        make_icon(info["controller"], out / "icon.png")
        make_tv(info["controller"], info["name"], out / "tv-splash.png")
        make_drc(info["controller"], info["name"], out / "drc-splash.png")
        print(f"{kind}: {out}")


if __name__ == "__main__":
    main()
