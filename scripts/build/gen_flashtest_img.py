#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import tempfile


def main() -> int:
    src = Path("../freedos_boot_fd.img")
    dst = Path("flashtest.img")
    shutil.copyfile(src, dst)

    autoexec = b"@echo off\r\nFLASHUTL SELFTEST\r\n"
    config = b"SWITCHES=/N\r\n"
    with tempfile.TemporaryDirectory() as td:
        auto_path = Path(td) / "AUTOEXEC.BAT"
        cfg_path = Path(td) / "CONFIG.SYS"
        auto_path.write_bytes(autoexec)
        cfg_path.write_bytes(config)
        subprocess.run(
            ["mcopy", "-o", "-i", str(dst), "flashutil.exe", "::FLASHUTL.EXE"],
            check=True,
        )
        subprocess.run(
            ["mcopy", "-o", "-i", str(dst), str(auto_path), "::AUTOEXEC.BAT"],
            check=True,
        )
        subprocess.run(
            ["mcopy", "-o", "-i", str(dst), str(cfg_path), "::CONFIG.SYS"],
            check=True,
        )

    print(f"wrote {dst}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
