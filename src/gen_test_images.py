#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parent
BASE_IMG = ROOT / "../freedos_boot_fd.img"
STD_IMG = ROOT / "stdtest.img"
CUST_BIO_IMG = ROOT / "custbio.img"
CUST_FLAT_IMG = ROOT / "custflat.img"


def write_image(dst: Path, autoexec: bytes, programs: list[str]) -> None:
    shutil.copyfile(BASE_IMG, dst)
    config = b"SWITCHES=/N\r\n"
    with tempfile.TemporaryDirectory() as td:
        td_path = Path(td)
        auto_path = td_path / "AUTOEXEC.BAT"
        cfg_path = td_path / "CONFIG.SYS"
        auto_path.write_bytes(autoexec)
        cfg_path.write_bytes(config)
        subprocess.run(
            ["mdel", "-i", str(dst), "::FLASHUTL.EXE"],
            check=False,
        )
        subprocess.run(
            ["mdel", "-i", str(dst), "::MEM_DMP.EXE"],
            check=False,
        )
        subprocess.run(
            ["mdel", "-i", str(dst), "::XRECV.EXE"],
            check=False,
        )
        subprocess.run(
            ["mdel", "-i", str(dst), "::XRECV.COM"],
            check=False,
        )
        subprocess.run(
            ["mdel", "-i", str(dst), "::SRECSAVE.COM"],
            check=False,
        )
        subprocess.run(
            ["mcopy", "-o", "-i", str(dst), str(ROOT / "shutdown.exe"), "::SHUTDOWN.EXE"],
            check=True,
        )
        for program in programs:
            subprocess.run(
                ["mcopy", "-o", "-i", str(dst), str(ROOT / program), f"::{program.upper()}"],
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


def main() -> int:
    std_autoexec = (
        b"@echo off\r\n"
        b"echo TEST LIST:\r\n"
        b"echo 1. BIOSTEST\r\n"
        b"BIOSTEST\r\n"
        b"if errorlevel 1 goto fail\r\n"
        b"echo TEST AUTOEXEC OK\r\n"
        b"SHUTDOWN 42\r\n"
        b"goto end\r\n"
        b":fail\r\n"
        b"echo TEST AUTOEXEC NG\r\n"
        b"SHUTDOWN 43\r\n"
        b":end\r\n"
    )
    cust_bio_autoexec = (
        b"@echo off\r\n"
        b"echo TEST LIST:\r\n"
        b"echo 1. BIOSTEST\r\n"
        b"BIOSTEST\r\n"
        b"if errorlevel 1 goto fail\r\n"
        b"echo TEST AUTOEXEC OK\r\n"
        b"SHUTDOWN 42\r\n"
        b"goto end\r\n"
        b":fail\r\n"
        b"echo TEST AUTOEXEC NG\r\n"
        b"SHUTDOWN 43\r\n"
        b":end\r\n"
    )
    cust_flat_autoexec = (
        b"@echo off\r\n"
        b"echo TEST LIST:\r\n"
        b"echo 1. FLATTEST\r\n"
        b"FLATTEST\r\n"
        b"if errorlevel 1 goto fail\r\n"
        b"echo TEST AUTOEXEC OK\r\n"
        b"SHUTDOWN 42\r\n"
        b"goto end\r\n"
        b":fail\r\n"
        b"echo TEST AUTOEXEC NG\r\n"
        b"SHUTDOWN 43\r\n"
        b":end\r\n"
    )
    write_image(STD_IMG, std_autoexec, ["biostest.exe"])
    write_image(CUST_BIO_IMG, cust_bio_autoexec, ["biostest.exe"])
    write_image(CUST_FLAT_IMG, cust_flat_autoexec, ["flattest.exe"])
    print(f"wrote {STD_IMG.name}")
    print(f"wrote {CUST_BIO_IMG.name}")
    print(f"wrote {CUST_FLAT_IMG.name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
