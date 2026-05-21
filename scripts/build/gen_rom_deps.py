#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path
import argparse
import os
import sys

import gen_rom


def make_escape(path: str) -> str:
    return path.replace("\\", "\\\\").replace(" ", "\\ ")


def make_dep_path(path: Path) -> str:
    return os.path.relpath(path.resolve(), Path.cwd().resolve())


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", required=True)
    parser.add_argument("blob_list", type=Path)
    parser.add_argument("depfile", type=Path)
    args = parser.parse_args()

    try:
        entries = gen_rom.parse_blob_list(args.blob_list)
    except ValueError as exc:
        print(f"gen_rom_deps.py: {exc}", file=sys.stderr)
        return 1

    deps = [
        make_dep_path(args.blob_list),
        make_dep_path(gen_rom.SRC_DIR / "shared_service" / "service_table.h"),
        "../scripts/build/gen_rom.py",
        "../scripts/build/gen_blob.py",
    ]
    deps.extend(make_dep_path(path) for _, path in entries)

    text = "{}: {}\n".format(
        make_escape(args.target),
        " ".join(make_escape(path) for path in deps),
    )
    args.depfile.write_text(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
