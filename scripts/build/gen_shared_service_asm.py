#!/usr/bin/env python3

import re
from pathlib import Path


EXPORTED = {
    "SHARED_ROM_LOW_BASE",
    "SHARED_ROM_HIGH_DELTA",
    "SHARED_TABLE_BYTES",
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
    defines = parse_numeric_defines(src_dir / "shared_service" / "service_table.h")
    inc_out = src_dir / "shared_service" / "service_table.inc"
    inc_lines = ["; generated from shared_service/service_table.h"]
    for name in sorted(defines):
        inc_lines.append(f"%define {name} 0x{defines[name]:x}")
    inc_out.write_text("\n".join(inc_lines) + "\n")

    ld_out = src_dir / "shared_service" / "service_table.ld"
    ld_lines = ["/* generated from shared_service/service_table.h */"]
    for name in sorted(defines):
        ld_lines.append(f"{name} = 0x{defines[name]:x};")
    ld_out.write_text("\n".join(ld_lines) + "\n")


if __name__ == "__main__":
    main()
