#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} INPUT.dsl OUTPUT.aml", file=sys.stderr)
        return 2

    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    text = src.read_text()
    start = text.find("DefinitionBlock")
    if start < 0:
        print(f"{src}: DefinitionBlock not found", file=sys.stderr)
        return 1

    asl = out.with_suffix(".asl")
    asl.write_text(text[start:])
    subprocess.check_call(["iasl", "-p", str(out.with_suffix("")), str(asl)])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
