"""Plan, apply and undo CRC-backed ROM names on a RetroU SD card."""

import argparse
import base64
import collections
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import unicodedata
import zlib


EXTENSIONS = {"gba": ".gba", "n64": (".n64", ".z64", ".v64"),
              "nes": ".nes", "snes": ".sfc"}


def catalog(path):
    matches = collections.defaultdict(set)
    data = path.read_text(encoding="utf-8", errors="replace")
    for title, crc in re.findall(
            r'comment "([^"\n]+)"[\s\S]*?rom \( crc ([0-9A-Fa-f]{8})', data):
        matches[crc.upper()].add(title)
    return matches


def windows_title(title):
    title = title.replace(": ", " - ").replace(":", " - ")
    title = title.replace(" / ", " - ").replace("/", "-").replace("\\", "-")
    for char in '?*<>':
        title = title.replace(char, "")
    title = title.replace('"', "'").replace("|", "-")
    return re.sub(r"\s+", " ", title).strip(" .")


def asset_stem(title):
    title = unicodedata.normalize("NFKD", title)
    return "".join(c if ord(c) < 128 and c not in '&*/:`<>?\\|"' else "_"
                   for c in title if not unicodedata.combining(c))


def fnv_path(path):
    value = 14695981039346656037
    for byte in path.encode("utf-8"):
        value = ((value ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return f"{value:016x}.conf"


def crc32_file(path):
    crc = 0
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            crc = zlib.crc32(chunk, crc)
    return f"{crc & 0xFFFFFFFF:08X}"


def within_root(path, root):
    if not path.resolve().is_relative_to(root.resolve()):
        raise ValueError(f"Ruta fuera de la SD indicada: {path}")


def prepare(root, audit, extra_catalogs):
    data = json.loads(audit.read_text(encoding="utf-8"))
    gb = catalog(extra_catalogs / "Nintendo_-_Game_Boy.dat")
    gbc = catalog(extra_catalogs / "Nintendo_-_Game_Boy_Color.dat")
    for extension, names in ((".gb", gb), (".gbc", gbc)):
        for source in (root / "roms" / "GBA").glob("*" + extension):
            crc = crc32_file(source)
            data["gba"].append({"source": str(source), "crc": crc,
                                "candidates": sorted(names.get(crc, []))})

    proposed = []
    targets = collections.defaultdict(list)
    for system, entries in data.items():
        for entry in entries:
            if len(entry["candidates"]) != 1:
                continue
            source = Path(entry["source"])
            target = source.with_name(windows_title(entry["candidates"][0]) + source.suffix)
            targets[str(target).casefold()].append(str(source))
            if source.name.casefold() != target.name.casefold():
                proposed.append((system, source, target, entry))

    roms = []
    skipped = []
    for system, source, target, entry in proposed:
        reason = None
        if len(target.name) > 240:
            reason = "nombre de destino demasiado largo"
        elif target.exists():
            reason = "destino existente"
        elif len(targets[str(target).casefold()]) > 1:
            reason = "varias ROMs para el mismo destino"
        if reason:
            skipped.append({"source": str(source), "target": str(target), "reason": reason})
            continue
        roms.append({"system": system, "source": str(source), "target": str(target),
                     "crc": entry["crc"], "catalog_title": entry["candidates"][0],
                     "size": source.stat().st_size, "mtime_ns": source.stat().st_mtime_ns})

    moves = []
    for item in roms:
        system = item["system"]
        source, target = Path(item["source"]), Path(item["target"])
        old, new = source.stem, target.stem
        moves.append({"kind": "rom", "source": str(source), "target": str(target)})

        associated = []
        if system == "gba":
            associated += [(source.with_suffix(ext), target.with_suffix(ext))
                           for ext in (".sav", ".sgm")]
        associated.append((root / "saves" / system / "save" / (old + ".srm"),
                           root / "saves" / system / "save" / (new + ".srm")))
        snap_dir = root / "snap" / system
        if snap_dir.exists():
            for path in snap_dir.glob(old + "-*.*"):
                associated.append((path, path.with_name(new + path.name[len(old):])))
        for kind in ("thumbnails", "backdrops"):
            associated.append((root / kind / system / (asset_stem(old) + ".png"),
                               root / kind / system / (asset_stem(new) + ".png")))
        app_old = f"/vol/external01/retrou/roms/{system}/{source.name}"
        app_new = f"/vol/external01/retrou/roms/{system}/{target.name}"
        associated.append((root / "config" / system / "rom-maps" / fnv_path(app_old),
                           root / "config" / system / "rom-maps" / fnv_path(app_new)))
        for old_path, new_path in associated:
            if old_path.exists() and old_path != new_path and not new_path.exists():
                moves.append({"kind": "associated", "source": str(old_path),
                              "target": str(new_path)})

    unique_moves = []
    seen_source, seen_target = set(), set()
    for move in moves:
        source, target = move["source"].casefold(), move["target"].casefold()
        if source in seen_source or target in seen_target:
            if move["kind"] == "rom":
                raise ValueError(f"Conflicto entre ROMs: {move['source']}")
            skipped.append({"source": move["source"], "target": move["target"],
                            "reason": "archivo asociado compartido"})
            continue
        seen_source.add(source)
        seen_target.add(target)
        unique_moves.append(move)

    favorites = []
    for system in EXTENSIONS:
        path = root / "config" / system / "favorites.conf"
        if not path.exists():
            continue
        before = path.read_bytes()
        after = before
        for item in roms:
            if item["system"] != system:
                continue
            old_name, new_name = Path(item["source"]).name, Path(item["target"]).name
            old_path = f"/vol/external01/retrou/roms/{system}/{old_name}".encode("utf-8")
            new_path = f"/vol/external01/retrou/roms/{system}/{new_name}".encode("utf-8")
            after = after.replace(old_path, new_path)
        if before != after:
            favorites.append({"path": str(path),
                              "before_base64": base64.b64encode(before).decode("ascii"),
                              "after_base64": base64.b64encode(after).decode("ascii")})

    return {"root": str(root), "created": datetime.datetime.now().isoformat(),
            "roms": roms, "moves": unique_moves, "favorites": favorites,
            "skipped": skipped, "status": "planned"}


def write_bytes(path, data):
    temporary = path.with_name(path.name + ".rom-rename-tmp")
    temporary.write_bytes(data)
    os.replace(temporary, path)


def execute(manifest_path, undo=False):
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    root = Path(manifest["root"])
    if not root.is_dir() or root.name.lower() != "retrou":
        raise ValueError("La raíz de la SD no es un directorio retrou")
    if not undo and manifest["status"] != "planned":
        raise ValueError("Este manifiesto ya fue aplicado")
    moves = list(reversed(manifest["moves"])) if undo else manifest["moves"]
    pending = []
    for move in moves:
        source = Path(move["target"] if undo else move["source"])
        target = Path(move["source"] if undo else move["target"])
        within_root(source, root)
        within_root(target, root)
        if undo and not source.exists() and target.is_file():
            continue
        if not source.is_file() or target.exists():
            raise ValueError(f"Conflicto: {source} -> {target}")
        pending.append((source, target))
    if not undo:
        for item in manifest["roms"]:
            source = Path(item["source"])
            stat = source.stat()
            if stat.st_size != item["size"] or stat.st_mtime_ns != item["mtime_ns"]:
                raise ValueError(f"La ROM cambió desde la auditoría: {source}")
    favorites_to_write = []
    for favorite in manifest["favorites"]:
        path = Path(favorite["path"])
        within_root(path, root)
        expected = base64.b64decode(favorite["after_base64" if undo else "before_base64"])
        current = path.read_bytes()
        if undo and current == base64.b64decode(favorite["before_base64"]):
            continue
        if current != expected:
            raise ValueError(f"Favoritos cambiaron desde la auditoría: {path}")
        data = base64.b64decode(favorite["before_base64" if undo else "after_base64"])
        favorites_to_write.append((path, data, current))

    log_path = root / ("rom-rename-rollback.log" if undo else "rom-rename.log")
    mirror = root / (manifest_path.stem.replace("rom-rename-", "rom-rename-manifest-", 1) + ".json")
    if not undo:
        mirror.write_bytes(manifest_path.read_bytes())
    completed_moves = []
    completed_favorites = []
    with log_path.open("a", encoding="utf-8") as log:
        log.write(f"BEGIN {datetime.datetime.now().isoformat()} manifest={manifest_path}\n")
        log.flush()
        os.fsync(log.fileno())
        try:
            for source, target in pending:
                source.rename(target)
                completed_moves.append((source, target))
                log.write(f"MOVE {source} -> {target}\n")
                log.flush()
                os.fsync(log.fileno())
            for path, data, original in favorites_to_write:
                write_bytes(path, data)
                completed_favorites.append((path, original))
                log.write(f"FAVORITES {path} sha256={hashlib.sha256(data).hexdigest()}\n")
                log.flush()
                os.fsync(log.fileno())
        except Exception:
            for path, original in reversed(completed_favorites):
                write_bytes(path, original)
            for source, target in reversed(completed_moves):
                target.rename(source)
            log.write("ABORTED_AND_ROLLED_BACK\n")
            raise
        log.write("COMPLETE\n")
    manifest["status"] = "rolled_back" if undo else "applied"
    manifest["completed"] = datetime.datetime.now().isoformat()
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                             encoding="utf-8")
    mirror.write_bytes(manifest_path.read_bytes())
    print(f"{len(manifest['roms'])} ROMs, {len(moves)} movimientos, "
          f"{len(manifest['favorites'])} favoritos; log: {log_path}")


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    plan = subparsers.add_parser("plan")
    plan.add_argument("--root", type=Path, required=True)
    plan.add_argument("--audit", type=Path, required=True)
    plan.add_argument("--extra-catalogs", type=Path, required=True)
    plan.add_argument("--output", type=Path, required=True)
    for command in ("apply", "rollback"):
        subparsers.add_parser(command).add_argument("--manifest", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "plan":
        manifest = prepare(args.root.resolve(), args.audit, args.extra_catalogs)
        args.output.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
                               encoding="utf-8")
        print(f"{len(manifest['roms'])} ROMs por renombrar, "
              f"{len(manifest['moves'])} movimientos, "
              f"{len(manifest['skipped'])} colisiones/archivos asociados omitidos")
    else:
        execute(args.manifest, args.command == "rollback")


if __name__ == "__main__":
    main()
