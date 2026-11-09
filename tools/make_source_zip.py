#!/usr/bin/env python3
"""Packs the project (including submodule contents) into one zip for the
local Windows build.

Timestamps matter: the Windows side copies files with robocopy (which keeps
timestamps) and builds with ninja (which compares timestamps). So every file
whose content changed since the previous zip gets the current time, and every
unchanged file keeps exactly the timestamp it had in the previous zip. A
manifest next to the output remembers what was packed last time.

Usage: make_source_zip.py <project_dir> <output_zip> <build_id>
"""
import hashlib
import json
import os
import sys
import time
import zipfile

SKIP_DIRS = {
    ".git", "out", "build", "package", ".vs", ".vscode", ".idea", "__pycache__",
}

# paths relative to the project root that are never needed for the build
SKIP_PREFIXES = (
    "dependencies/union/gothic-api/ZenGin/DirectX8/",
    "dependencies/union/gothic-api/ZenGin/Gothic_I_Classic/API/",
    "dependencies/union/gothic-api/ZenGin/Gothic_I_Addon/API/",
    "dependencies/union/gothic-api/ZenGin/Gothic_II_Classic/API/",
    "dependencies/imgui/imgui/examples/",
    "dependencies/imgui/imgui/docs/",
    "dependencies/imgui/imgui/backends/",
    "dependencies/imgui/imgui/misc/",
    "dependencies/union/union-api/dependencies/zlib/contrib/",
    "dependencies/union/union-api/dependencies/zlib/test/",
    "dependencies/union/union-api/dependencies/zlib/doc/",
    "dependencies/union/union-api/dependencies/vorbis/doc/",
    "dependencies/union/union-api/dependencies/vorbis/examples/",
    "dependencies/union/union-api/dependencies/vorbis/test/",
    "dependencies/union/union-api/dependencies/ogg/doc/",
    "dependencies/union/union-api/dependencies/detours/samples/",
    "tools/",
)


def main() -> int:
    root, output, build_id = sys.argv[1], sys.argv[2], sys.argv[3]
    root = os.path.abspath(root)
    manifest_path = os.environ.get("ZIP_MANIFEST") or os.path.join(root, "tools", ".zip_manifest.json")
    try:
        with open(manifest_path, "r", encoding="utf-8") as fh:
            previous = json.load(fh)
    except (OSError, ValueError):
        previous = {}

    now = time.localtime(time.time() + 60)[:6]   # a minute ahead: never older than a fresh build
    manifest = {}
    count = changed = 0
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
            for name in sorted(filenames):
                if name == ".git":
                    continue
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                if rel.startswith(SKIP_PREFIXES) or rel == "BUILD_ID.txt":
                    continue   # BUILD_ID.txt is written separately below
                with open(full, "rb") as fh:
                    data = fh.read()
                digest = hashlib.sha1(data).hexdigest()
                old = previous.get(rel)
                if old and old[0] == digest:
                    stamp = tuple(old[1])
                else:
                    stamp = now
                    changed += 1
                manifest[rel] = [digest, list(stamp)]
                info = zipfile.ZipInfo(rel, date_time=stamp)
                info.compress_type = zipfile.ZIP_DEFLATED
                info.external_attr = 0o644 << 16
                zf.writestr(info, data)
                count += 1
        zf.writestr(zipfile.ZipInfo("BUILD_ID.txt", date_time=now), build_id + "\n")

    with open(manifest_path, "w", encoding="utf-8") as fh:
        json.dump(manifest, fh)
    print(f"{count} files ({changed} changed) -> {output} ({os.path.getsize(output) / 1e6:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
