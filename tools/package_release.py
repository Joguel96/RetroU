#!/usr/bin/env python3
"""Package committed RetroU sources and the matching Wii U applications."""
from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile
from xml.sax.saxutils import escape
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
GIT = r"C:\Program Files\Git\cmd\git.exe" if os.name == "nt" else shutil.which("git")
APPS = {"nes": "RetroNESU", "snes": "RetroSNESU", "gba": "RetroGBAU", "n64": "RetroN64U"}
CORES = {
    "fceumm": ("https://github.com/libretro/libretro-fceumm", "e0f3086a8104f8fbbe49a2615b10401504a9533f"),
    "snes9x": ("https://github.com/libretro/snes9x", "fae2fea08f74180759ef540ee94259213f503480"),
    "mgba": ("https://github.com/libretro/mgba", "7a12d6d4b9acb14c0ae62c9166b6a2f3d08007f6"),
    "our64": ("https://github.com/Ch0nbo/Our64", "d3c56db253cd1b20987eb33ecd5797c60e527b64"),
}
EXCLUDED = {".o", ".a", ".elf", ".rpx", ".wuhb", ".nes", ".sfc", ".smc", ".gba", ".gb", ".gbc", ".z64", ".n64", ".v64", ".nds"}


def git(*args: str, cwd: Path = ROOT) -> bytes:
    return subprocess.check_output([GIT, "-C", str(cwd), *args])


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def put(archive: zipfile.ZipFile, name: str, data: bytes) -> None:
    # A fixed timestamp also keeps the archive manifest reproducible.
    entry = zipfile.ZipInfo(name, (2026, 11, 23, 0, 0, 0))
    entry.compress_type = zipfile.ZIP_DEFLATED
    entry.external_attr = (0o100755 if name.endswith(".sh") else 0o100644) << 16
    archive.writestr(entry, data)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    if not re.fullmatch(r"v\d+\.\d+\.\d+(?:-[A-Za-z0-9.]+)?", args.version):
        parser.error("Use a version such as v0.1.0")
    if git("status", "--porcelain", "--untracked-files=no").strip():
        raise SystemExit("Commit the source changes before packaging")
    output = ROOT / "dist" / "releases" / args.version
    output.mkdir(parents=True, exist_ok=True)
    dependency_dir = ROOT / "native/build/release-dependencies"
    dependency_manifest = json.loads((dependency_dir / "manifest.json").read_text())
    for dependency in dependency_manifest:
        if digest((dependency_dir / dependency["file"]).read_bytes()) != dependency["sha256"]:
            raise SystemExit(f"Dependency checksum mismatch: {dependency['file']}")
    revision = git("rev-parse", "HEAD").decode().strip()
    manifest = {"version": args.version, "project_revision": revision, "publisher": "xoloDev",
                "tested_environment": "Aroma (Wii U hardware)", "tiramisu": "hardware validation pending (REL-2)",
                "cores": {}, "artifacts": {}, "source_dependencies": dependency_manifest, "packages": {}}
    cafeglsl = ROOT / ".cores/our64/third_party/cafeglsl"
    manifest["cafeglsl_sdk"] = {"upstream": "https://github.com/Exzap/CafeGLSL",
        "archive_sha256": digest((cafeglsl / "lib/libcafeglsl.a").read_bytes()),
        "note": "Exact prebuilt MIT/Mesa SDK supplied with the source package; upstream notices preserved"}
    package_root = Path(os.environ.get("DEVKITPRO", "C:/devkitPro" if os.name == "nt" else "/opt/devkitpro"))
    if os.name == "nt" and not package_root.exists():
        package_root = Path("C:/devkitPro")
    for desc in sorted((package_root / "msys2/var/lib/pacman/local").glob("*/desc")):
        fields = desc.read_text(errors="replace").split("\n\n")
        values = {section.splitlines()[0]: section.splitlines()[1:] for section in fields if section.strip()}
        name = values.get("%NAME%", [""])[0]
        if name.startswith(("wiiu-", "ppc-", "wut", "devkitppc-", "devkitPPC")):
            manifest["packages"][name] = values.get("%VERSION%", [""])[0]
    for core, (url, expected) in CORES.items():
        core_dir = ROOT / ".cores" / core
        actual = git("rev-parse", "HEAD", cwd=core_dir).decode().strip()
        if actual != expected:
            raise SystemExit(f"Unexpected {core} revision: {actual}")
        changed = git("diff", "--name-only", cwd=core_dir).decode().splitlines()
        allowed = ["src/third-party/zlib/contrib/minizip/ioapi.c"] if core == "mgba" else []
        if changed != allowed:
            raise SystemExit(f"Unexpected {core} modifications: {changed}")
        manifest["cores"][core] = {"upstream": url, "revision": actual, "modified_files": changed}
    binary_zip = output / f"RetroU-{args.version}.zip"
    source_zip = output / f"RetroU-{args.version}-sources.zip"
    with zipfile.ZipFile(binary_zip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as package:
        for system, app in APPS.items():
            for extension, environment, magic in (("wuhb", "Aroma", b"WUHB"), ("rpx", "Tiramisu", b"\x7fELF")):
                path = ROOT / f"native/build/retrou_{system}_gx2.{extension}"
                data = path.read_bytes()
                if data[:4] != magic:
                    raise SystemExit(f"Invalid {extension} header: {path}")
                if extension == "wuhb" and data != (ROOT / f"dist/wiiu/apps/{app}/{app}.wuhb").read_bytes():
                    raise SystemExit(f"Staged WUHB differs: {app}")
                entry = f"{environment}/wiiu/apps/{app}/{app}.{extension}"
                put(package, entry, data)
                manifest["artifacts"][entry] = {"sha256": digest(data), "bytes": len(data)}
            icon = Image.open(ROOT / f"assets/console/{system}/icon.png").convert("RGBA")
            icon.thumbnail((48, 48), Image.Resampling.LANCZOS)
            launcher_icon = Image.new("RGBA", (128, 48), (18, 25, 36, 255))
            launcher_icon.alpha_composite(icon, (0, 0))
            draw = ImageDraw.Draw(launcher_icon)
            font = ImageFont.truetype(str(ROOT / "native/content/ui.ttf"), 15)
            draw.text((52, 3), "RetroU", font=font, fill="white")
            draw.text((52, 25), system.upper(), font=font, fill=(130, 190, 245))
            icon_data = io.BytesIO()
            launcher_icon.save(icon_data, format="PNG")
            put(package, f"Tiramisu/wiiu/apps/{app}/icon.png", icon_data.getvalue())
            meta = (f'<?xml version="1.0" encoding="UTF-8"?>\n<app version="1"><name>RetroU {system.upper()}</name>'
                    f'<coder>xoloDev</coder><version>{escape(args.version)}</version><release_date>20261123000000</release_date>'
                    f'<short_description>RetroU {system.upper()}</short_description>'
                    '<long_description>Frontend RetroU con motor integrado. Hecho con asistencia de IA. '
                    'ROMs: sd:/retrou/roms/' + system + '/. Consulte CREDITOS y LICENCIAS. '
                    'Tiramisu pendiente de validacion fisica.</long_description></app>\n')
            put(package, f"Tiramisu/wiiu/apps/{app}/meta.xml", meta.encode())
        for name in ("LICENSE", "LICENSES.md", "CREDITS.md"):
            put(package, name, (ROOT / name).read_bytes())
        put(package, "LEEME.md", (ROOT / "docs/release.md").read_bytes())
        for path in sorted((ROOT / "licenses").rglob("*")):
            if path.is_file():
                put(package, path.relative_to(ROOT).as_posix(), path.read_bytes())
        manifest_data = (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode()
        put(package, "build-manifest.json", manifest_data)
    with zipfile.ZipFile(source_zip, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as package:
        with zipfile.ZipFile(io.BytesIO(git("archive", "--format=zip", "HEAD"))) as project:
            for entry in project.infolist():
                if not entry.is_dir():
                    put(package, "RetroU/" + entry.filename, project.read(entry))
        for core in CORES:
            core_dir = ROOT / ".cores" / core
            with zipfile.ZipFile(io.BytesIO(git("archive", "--format=zip", "HEAD", cwd=core_dir))) as sources:
                for entry in sources.infolist():
                    if entry.is_dir() or Path(entry.filename).suffix.lower() in EXCLUDED:
                        continue
                    data = sources.read(entry)
                    if entry.filename in manifest["cores"][core]["modified_files"]:
                        data = (core_dir / entry.filename).read_bytes().replace(b"\r\n", b"\n")
                    put(package, f"RetroU/.cores/{core}/{entry.filename}", data)
            put(package, f"RetroU/.cores/{core}/.retrou-source-revision", (CORES[core][1] + "\n").encode())
        for dependency in dependency_manifest:
            name = dependency["file"]
            put(package, "RetroU/native/build/release-dependencies/" + name, (dependency_dir / name).read_bytes())
        for path in sorted(cafeglsl.rglob("*")):
            if path.is_file():
                put(package, "RetroU/.cores/our64/third_party/cafeglsl/" + path.relative_to(cafeglsl).as_posix(), path.read_bytes())
        put(package, "RetroU/native/build/release-dependencies/manifest.json", (dependency_dir / "manifest.json").read_bytes())
        put(package, "RetroU/build-manifest.json", manifest_data)
    checksums = []
    for path in (binary_zip, source_zip):
        with zipfile.ZipFile(path) as archive:
            if archive.testzip():
                raise SystemExit(f"ZIP checksum validation failed: {path}")
        checksums.append(f"{digest(path.read_bytes())}  {path.name}\n")
        print(f"{path.name}: {path.stat().st_size} bytes")
    (output / "SHA256SUMS.txt").write_text("".join(checksums), encoding="utf-8")
    (output / "build-manifest.json").write_bytes(manifest_data)
    print(f"Source commit: {revision}")


if __name__ == "__main__":
    main()
