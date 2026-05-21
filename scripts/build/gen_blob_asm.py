#!/usr/bin/env python3

import re
from pathlib import Path


EXPORTED = {
    "BLOB_STATUS_SIZE",
    "STAGE2_LOAD_LINEAR",
    "STAGE2_LOAD_CAPACITY",
    "STAGE2_ENTRY",
}


def parse_numeric_defines(path: Path):
    pattern = re.compile(r"^#define\s+([A-Z0-9_]+)\s+(.+?)\s*$")
    defines = {}
    for raw_line in path.read_text().splitlines():
        match = pattern.match(raw_line.strip())
        if not match:
            continue
        name, value = match.groups()
        if name not in EXPORTED:
            continue
        value = value.split("/*", 1)[0].strip()
        value = re.sub(r"[uUlL]+$", "", value)
        if not re.fullmatch(r"0x[0-9a-fA-F]+|[0-9]+", value):
            raise ValueError(f"unsupported define value for {name}: {value}")
        defines[name] = int(value, 0)
    missing = sorted(EXPORTED - set(defines))
    if missing:
        raise ValueError(f"missing defines: {', '.join(missing)}")
    return defines


def main():
    repo_root = Path(__file__).resolve().parents[2]
    src_dir = repo_root / "src"
    defines = parse_numeric_defines(src_dir / "include" / "blob.h")
    out = src_dir / "include" / "blob.inc"
    lines = ["; generated from include/blob.h"]
    for name in sorted(defines):
        lines.append(f"%define {name} 0x{defines[name]:x}")
    out.write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
