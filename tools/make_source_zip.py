#!/usr/bin/env python3
"""Packs the project (including submodule contents) into one zip for the
local Windows build. Unchanged files keep their timestamps so the build on
the other side stays incremental.

Usage: make_source_zip.py <project_dir> <output_zip> <build_id>
"""
import os
import sys
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
    count = 0
    with zipfile.ZipFile(output, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as zf:
        for dirpath, dirnames, filenames in os.walk(root):
            dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
            for name in sorted(filenames):
                if name == ".git":
                    continue
                full = os.path.join(dirpath, name)
                rel = os.path.relpath(full, root).replace(os.sep, "/")
                if rel.startswith(SKIP_PREFIXES):
                    continue
                info = zipfile.ZipInfo.from_file(full, rel)
                info.compress_type = zipfile.ZIP_DEFLATED
                with open(full, "rb") as fh:
                    zf.writestr(info, fh.read())
                count += 1
        zf.writestr("BUILD_ID.txt", build_id + "\n")
    print(f"{count} files -> {output} ({os.path.getsize(output) / 1e6:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
