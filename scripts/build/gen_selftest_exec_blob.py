#!/usr/bin/env python3
from pathlib import Path
import argparse
import re
import struct


def parse_c_int_expr(expr: str) -> int:
    expr = expr.split("/*", 1)[0].strip()
    expr = re.sub(
        r"(0x[0-9a-fA-F]+|[0-9]+)[uUlL]*",
        lambda match: match.group(1),
        expr,
    )
    if not re.fullmatch(r"[0-9a-fA-FxX()+*/%<>&|~^ \t+-]+", expr):
        raise ValueError(f"unsupported integer expression: {expr}")
    return int(eval(expr, {"__builtins__": {}}, {}))


def load_constants(path: Path) -> dict[str, int]:
    out: dict[str, int] = {}
    for raw in path.read_text().splitlines():
        raw = raw.strip()
        if not raw.startswith("#define "):
            continue
        parts = raw.split(None, 2)
        if len(parts) != 3:
            continue
        _, name, value = parts
        if name in {
            "SELFTEST_EXEC_BLOB_MAGIC",
            "SELFTEST_EXEC_BLOB_HEADER_SIZE",
        }:
            out[name] = parse_c_int_expr(value)
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("binary", type=Path)
    parser.add_argument("load_addr")
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    consts = load_constants(
        Path(__file__).resolve().parents[2] / "src" / "include" /
        "selftest_exec_blob.h")
    load_addr = parse_c_int_expr(args.load_addr)
    image = args.binary.read_bytes()
    hdr = struct.pack(
        "<IIII",
        consts["SELFTEST_EXEC_BLOB_MAGIC"],
        load_addr,
        len(image),
        0,
    )
    if len(hdr) != consts["SELFTEST_EXEC_BLOB_HEADER_SIZE"]:
        raise SystemExit("bad selftest exec blob header size")
    args.output.write_bytes(hdr + image)
    print(
        f"wrote {args.output} load=0x{load_addr:08x} "
        f"size={len(image)} total={len(hdr) + len(image)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
