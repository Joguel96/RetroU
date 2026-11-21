"""Audit duplicate ROMs and move only confirmed matches out of the SD catalog.

Run without --apply to print the plan. The backup is on C: so SD space is freed.
The JSON manifest records every move for a manual rollback.
"""

import argparse
from array import array
import hashlib
import json
import os
import re
import shutil
from collections import defaultdict
from pathlib import Path


EXTENSIONS = {
    "NES": {".nes"}, "SNES": {".sfc", ".smc"},
    "GBA": {".gba"}, "N64": {".n64", ".v64", ".z64"},
}
TAGS = re.compile(r"(?:\s*(?:\([^)]*\)|\[[^]]*\]))+$")
UNUSUAL = re.compile(r"\b(?:beta|demo|proto|sample|virtual console|switch online|arcade|wii)\b", re.I)


def title_key(path):
    base = TAGS.sub("", path.stem)
    return re.sub(r"[^a-z0-9]", "", base.casefold())


def region(path):
    name = path.stem.casefold()
    if re.search(r"\((?:usa|u|japan, usa|usa, europe)\)", name) or "[u]" in name:
        return "USA"
    if re.search(r"\((?:europe|e)\)", name) or "[e]" in name:
        return "Europe"
    return "Other"


def digest(path, system):
    h = hashlib.sha256()
    with path.open("rb") as f:
        prefix = f.read(4)
        f.seek(0)
        if system == "N64" and prefix in (b"\x37\x80\x40\x12", b"\x40\x12\x37\x80"):
            step = 2 if prefix[0] == 0x37 else 4
            while block := f.read(4 * 1024 * 1024):
                if len(block) % step:
                    raise ValueError(f"ROM N64 con tamaño inesperado: {path}")
                words = array("H" if step == 2 else "I")
                words.frombytes(block)
                words.byteswap()
                h.update(words.tobytes())
        else:
            while block := f.read(4 * 1024 * 1024):
                h.update(block)
    return h.hexdigest()


def rank(path):
    name = path.stem.casefold()
    return (region(path) != "USA", bool(UNUSUAL.search(name)),
            "(rev" in name, not any(tag in name for tag in ("(usa)", "(europe)")),
            "[!]" in name, path.suffix.lower() != ".z64", len(name), name)


def audit(root):
    moves = []
    for system, exts in EXTENSIONS.items():
        folder = root / system
        files = [p for p in folder.iterdir() if p.is_file() and p.suffix.lower() in exts]
        by_title = defaultdict(list)
        for p in files:
            by_title[title_key(p)].append(p)
        to_remove = {}
        # Same title and a standard USA release: European editions are redundant.
        for group in by_title.values():
            usa = [p for p in group if region(p) == "USA" and not UNUSUAL.search(p.stem)]
            if not usa:
                continue
            keep = min(usa, key=rank)
            for p in group:
                if region(p) == "Europe":
                    to_remove[p] = (keep, "USA sustituye Europa")

        # Identical ROM bytes, including N64 images with a different byte order.
        by_hash = defaultdict(list)
        for p in files:
            by_hash[digest(p, system)].append(p)
        for group in by_hash.values():
            if len(group) < 2:
                continue
            available = [p for p in group if p not in to_remove]
            keep = min(available or group, key=rank)
            for p in group:
                if p != keep:
                    to_remove[p] = (keep, "contenido idéntico")
        for p, (keep, reason) in sorted(to_remove.items()):
            moves.append({"system": system, "source": str(p), "keep": str(keep),
                          "reason": reason, "bytes": p.stat().st_size,
                          "modified_ns": p.stat().st_mtime_ns})
    return moves


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("E:/retrou/roms"))
    parser.add_argument("--backup", type=Path, default=Path("C:/proyectos/RetroU/native/build/rom-dedupe-20261121"))
    parser.add_argument("--plan", type=Path, default=Path("C:/proyectos/RetroU/native/build/rom-dedupe-plan-20261121.json"))
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--restore", action="store_true")
    args = parser.parse_args()
    if args.apply and args.restore:
        parser.error("--apply y --restore son excluyentes")
    if args.restore:
        manifest = args.backup / "manifest.json"
        moves = json.loads(manifest.read_text(encoding="utf-8"))
        for m in moves:
            if Path(m["source"]).exists() or not Path(m["backup"]).is_file():
                raise RuntimeError(f"No se puede restaurar: {m['source']}")
        for m in moves:
            shutil.copy2(m["backup"], m["source"])
            if digest(Path(m["source"]), m["system"]) != digest(Path(m["backup"]), m["system"]):
                raise RuntimeError(f"La copia restaurada no coincide: {m['source']}")
        print(f"Restauradas {len(moves)} ROMs; se conservan los respaldos")
        return
    if args.apply:
        moves = json.loads(args.plan.read_text(encoding="utf-8"))
    else:
        moves = audit(args.root)
        args.plan.parent.mkdir(parents=True, exist_ok=True)
        args.plan.write_text(json.dumps(moves, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Candidatas: {len(moves)} ROMs; {sum(m['bytes'] for m in moves):,} bytes")
    for m in moves:
        print(f"{m['system']}: {Path(m['source']).name} -> {Path(m['keep']).name} ({m['reason']})")
    if not args.apply:
        return
    manifest = args.backup / "manifest.json"
    previous = {}
    if manifest.exists():
        previous = {m["source"]: m for m in json.loads(manifest.read_text(encoding="utf-8"))}
    for m in moves:
        source = Path(m["source"])
        target = args.backup / m["system"] / source.name
        if source.is_file():
            valid = source.stat().st_size == m["bytes"] and \
                    source.stat().st_mtime_ns == m["modified_ns"]
        else:
            valid = m["source"] in previous and target.is_file()
        if not valid or not Path(m["keep"]).is_file():
            raise RuntimeError(f"El plan ya no coincide con la SD: {source}")
    for m in moves:
        source = Path(m["source"])
        target = args.backup / m["system"] / source.name
        if not source.exists():
            m["backup"] = str(target)
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists():
            if digest(source, m["system"]) != digest(target, m["system"]):
                raise RuntimeError(f"La copia no coincide con la ROM: {source}")
        else:
            shutil.copy2(source, target)
            if digest(source, m["system"]) != digest(target, m["system"]):
                raise RuntimeError(f"La copia no coincide con la ROM: {source}")
        os.chmod(source, 0o666)
        source.unlink()
        m["backup"] = str(target)
        manifest.write_text(json.dumps(moves, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
