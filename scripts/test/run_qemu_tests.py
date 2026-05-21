#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path.cwd()


def run_case(name: str, cmd: list[str], expect: list[str]) -> bool:
    try:
        proc = subprocess.run(
            cmd,
            input=b"\r\r\r\r\r\r\r\r\r\r",
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=90,
            check=False,
        )
        out_bytes = proc.stdout
        rc = proc.returncode
    except subprocess.TimeoutExpired as exc:
        out_bytes = exc.stdout or b""
        rc = None
    out = out_bytes.decode("latin-1", errors="replace")
    print(f"== {name} ==")
    print(out)
    ok = all(token in out for token in expect)
    if rc is None:
        print(f"RESULT {name}: TIMEOUT")
        return False
    if rc == 87:
        print(f"RESULT {name}: NG")
        return False
    if rc != 85:
        print(f"RESULT {name}: EXIT {rc}")
        return False
    print(f"RESULT {name}: {'OK' if ok else 'NG'}")
    return ok


def main() -> int:
    std_ok = run_case(
        "stdbios",
        [
            "qemu-system-i386",
            "-m", "32m",
            "-drive", f"if=floppy,format=raw,file={ROOT / 'stdtest.img'}",
            "-boot", "a",
            "-M", "pc",
            "-serial", "stdio",
            "-monitor", "none",
            "-nographic",
            "-no-reboot",
            "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
        ],
        [
            "BIOSTEST START",
            "TEST int13_rootdir OK",
            "TEST SUMMARY OK",
        ],
    )
    with tempfile.TemporaryDirectory(prefix="440bx-storage-") as tmpdir:
        tmp = Path(tmpdir)
        ide_img = tmp / "ide.img"
        linuxprobe_img = tmp / "linuxprobe.img"
        linuxprobe_raw_img = tmp / "linuxprobe_raw.img"
        shutil.copyfile(ROOT / "usbmbr.img", ide_img)
        shutil.copyfile(ROOT / "linuxprobe.img", linuxprobe_img)
        shutil.copyfile(ROOT / "linuxprobe_raw.img", linuxprobe_raw_img)
        ideboot_ok = run_case(
            "custombios-idembr",
            [
                "qemu-system-i386",
                "-m", "32m",
                "-bios", str(ROOT / "qemu_legacy_genrom.bin"),
                "-M", "pc",
                "-serial", "stdio",
                "-monitor", "none",
                "-nographic",
                "-no-reboot",
                "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
                "-drive", f"if=ide,format=raw,file={ide_img}",
            ],
            [
                "Legacy app @ 000f0000",
                "IDE LBA0 @",
                "BIOS HDD80 IDE",
                "USBMBR",
                "TIMEROK",
                "INT60OK",
                "E820OK",
            ],
        )
        linuxprobe_ok = run_case(
            "custombios-linuxprobe",
            [
                "qemu-system-i386",
                "-m", "32m",
                "-bios", str(ROOT / "qemu_linux_genrom.bin"),
                "-M", "pc",
                "-serial", "stdio",
                "-monitor", "none",
                "-nographic",
                "-no-reboot",
                "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
                "-drive", f"if=ide,format=raw,file={linuxprobe_img}",
            ],
            [
                "Linux app @ 000f0000",
                "Linux part1 start=",
                "Linux initrd @",
                "Boot Linux entry=00100000",
                "LINUXPROBE",
            ],
        )
        linuxprobe_raw_ok = run_case(
            "custombios-linuxprobe-raw",
            [
                "qemu-system-i386",
                "-m", "32m",
                "-bios", str(ROOT / "qemu_linux_genrom.bin"),
                "-M", "pc",
                "-serial", "stdio",
                "-monitor", "none",
                "-nographic",
                "-no-reboot",
                "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
                "-drive", f"if=ide,format=raw,file={linuxprobe_raw_img}",
            ],
            [
                "Linux app @ 000f0000",
                "Linux disk start=00000000",
                "Linux initrd: none",
                "Boot Linux entry=00100000",
                "LINUXPROBE",
            ],
        )
        usbmbr_ok = run_case(
            "custombios-usbmbr",
            [
                "qemu-system-i386",
                "-m", "32m",
                "-bios", str(ROOT / "qemu_legacy_genrom.bin"),
                "-M", "pc",
                "-serial", "stdio",
                "-monitor", "none",
                "-nographic",
                "-no-reboot",
                "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
                "-device", "piix4-usb-uhci,id=uhci",
                "-drive", f"if=none,id=usbmbr,format=raw,file={ROOT / 'usbmbr.img'}",
                "-device", "usb-storage,drive=usbmbr,bus=uhci.0",
            ],
            [
                "Legacy app @ 000f0000",
                "BIOS HDD80 USB",
                "Booting drive=80",
                "USBMBR",
                "TIMEROK",
                "INT60OK",
                "E820OK",
            ],
        )
        legacy_floppy_ok = run_case(
            "custombios-legacy-floppy",
            [
                "qemu-system-i386",
                "-m", "32m",
                "-bios", str(ROOT / "qemu_legacy_test_bios.bin"),
                "-M", "pc",
                "-serial", "stdio",
                "-monitor", "none",
                "-nographic",
                "-no-reboot",
                "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
            ],
            [
                "Test floppy @",
                "Booting test floppy",
                "Booting drive=00",
                "SQ",
            ],
        )
    return 0 if (std_ok and ideboot_ok and linuxprobe_ok and
                 linuxprobe_raw_ok and usbmbr_ok and
                 legacy_floppy_ok) else 1


if __name__ == "__main__":
    raise SystemExit(main())
