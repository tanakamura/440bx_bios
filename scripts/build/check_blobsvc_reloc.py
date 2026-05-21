#!/usr/bin/env python3
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


COPIED_SECTIONS = {".blobsvc", ".blobsvc.entry"}
IGNORED_ALLOC_SECTIONS = {".eh_frame", ".note.gnu.property"}


def run_readelf(args: list[str]) -> str:
    return subprocess.check_output(["readelf", *args], text=True)


def parse_sections(obj: Path) -> tuple[dict[int, str], list[str]]:
    sections: dict[int, str] = {}
    errors: list[str] = []
    section_re = re.compile(
        r"^\s*\[\s*(\d+)\]\s+(\S+)\s+\S+\s+\S+\s+\S+\s+"
        r"([0-9a-fA-F]+)\s+\S+\s+(\S*)"
    )

    for line in run_readelf(["-SW", str(obj)]).splitlines():
        match = section_re.match(line)
        if match is None:
            continue
        index = int(match.group(1))
        name = match.group(2)
        size = int(match.group(3), 16)
        flags = match.group(4)
        sections[index] = name
        if "A" in flags and size != 0:
            if name not in COPIED_SECTIONS and name not in IGNORED_ALLOC_SECTIONS:
                errors.append(f"unexpected alloc section {name} size=0x{size:x}")

    return sections, errors


def parse_symbols(obj: Path, sections: dict[int, str]) -> dict[str, str]:
    symbols: dict[str, str] = {}

    for line in run_readelf(["-sW", str(obj)]).splitlines():
        stripped = line.strip()
        if not stripped or not stripped[0].isdigit():
            continue
        parts = stripped.split(None, 7)
        if len(parts) < 8:
            continue
        ndx = parts[6]
        name = parts[7]
        if ndx.isdigit():
            symbols[name] = sections.get(int(ndx), ndx)
        else:
            symbols[name] = ndx

    return symbols


def relocation_target_section(rel_section: str) -> str:
    if rel_section.startswith(".rel."):
        return rel_section[len(".rel"):]
    if rel_section.startswith(".rela."):
        return rel_section[len(".rela"):]
    return ""


def check_relocations(obj: Path, symbols: dict[str, str]) -> list[str]:
    errors: list[str] = []
    current_target = ""
    rel_header_re = re.compile(r"^Relocation section '([^']+)'")

    for line in run_readelf(["-rW", str(obj)]).splitlines():
        header = rel_header_re.match(line)
        if header is not None:
            current_target = relocation_target_section(header.group(1))
            continue
        if current_target not in COPIED_SECTIONS:
            continue

        stripped = line.strip()
        if not stripped or stripped[0] not in "0123456789abcdefABCDEF":
            continue
        parts = stripped.split()
        if len(parts) < 5:
            continue
        reloc_type = parts[2]
        symbol = parts[-1]
        if symbol in COPIED_SECTIONS:
            symbol_section = symbol
        else:
            symbol_section = symbols.get(symbol, "")

        if reloc_type != "R_386_PC32":
            errors.append(
                f"{current_target}: unsupported relocation {reloc_type} to {symbol}"
            )
            continue
        if symbol_section not in COPIED_SECTIONS:
            errors.append(
                f"{current_target}: relocation to non-blobsvc symbol "
                f"{symbol} ({symbol_section or 'unknown'})"
            )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify shared blob service can be copied as one block."
    )
    parser.add_argument("object", type=Path)
    args = parser.parse_args()

    sections, section_errors = parse_sections(args.object)
    symbols = parse_symbols(args.object, sections)
    errors = section_errors + check_relocations(args.object, symbols)

    if errors:
        for error in errors:
            print(f"check_blobsvc_reloc.py: {error}", file=sys.stderr)
        return 1

    print(f"blobsvc reloc ok: {args.object}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
