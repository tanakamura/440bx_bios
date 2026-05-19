#!/usr/bin/env python3

from pathlib import Path


def parse_defs(path: Path):
    entries = []
    for raw_line in path.read_text().splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        name, hex_code, desc = line.split(maxsplit=2)
        entries.append((name, int(hex_code, 16), desc))
    return entries


def write_header(path: Path, entries):
    lines = ["#ifndef POST_CODE_H", "#define POST_CODE_H", ""]
    for name, code, desc in entries:
        lines.append(f"#define {name} 0x{code:02x} /* {desc} */")
    lines.extend(["", "#endif"])
    path.write_text("\n".join(lines) + "\n")


def write_inc(path: Path, entries):
    lines = [f"%define {name} 0x{code:02x} ; {desc}" for name, code, desc in entries]
    path.write_text("\n".join(lines) + "\n")


def write_md(path: Path, entries):
    groups = {}
    for entry in entries:
        groups.setdefault(entry[1] >> 4, []).append(entry)

    lines = []
    if 0x0 in groups:
        lines.append("00-0f : pre car")
        for _, code, desc in groups[0x0]:
            lines.append(f"  {code:02X}  : {desc}")
    if 0x1 in groups:
        lines.append("10-1f : C")
        for _, code, desc in groups[0x1]:
            lines.append(f"  {code:02X}  : {desc}")
    for nibble in sorted(groups):
        if nibble in (0x0, 0x1, 0xF):
            continue
        lines.append("")
        lines.append(f"{nibble:X}0-{nibble:X}F : tmp")
        for _, code, desc in groups[nibble]:
            lines.append(f"  {code:02X}  : {desc}")
    if 0xF in groups:
        lines.append("F0-FF : fatal")
        for _, code, desc in groups[0xF]:
            lines.append(f"  {code:02X}  : {desc}")
    path.write_text("\n".join(lines) + "\n")


def main():
    repo_root = Path(__file__).resolve().parents[2]
    src_dir = repo_root / "src"
    entries = parse_defs(src_dir / "post_code.def")
    write_header(src_dir / "post_code.h", entries)
    write_inc(src_dir / "post_code.inc", entries)
    write_md(repo_root / "specs" / "POST_CODE.md", entries)


if __name__ == "__main__":
    main()
