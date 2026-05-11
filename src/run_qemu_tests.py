#!/usr/bin/env python3
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parent


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
    flat_ok = run_case(
        "custombios-flat",
        [
            "qemu-system-i386",
            "-m", "32m",
            "-bios", str(ROOT / "qemu_flat_test_bios.bin"),
            "-M", "pc",
            "-serial", "stdio",
            "-monitor", "none",
            "-nographic",
            "-no-reboot",
            "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
        ],
        [
            "FLATTEST START",
            "TEST int60_flat OK",
        ],
    )
    with tempfile.TemporaryDirectory(prefix="440bx-storage-") as tmpdir:
        tmp = Path(tmpdir)
        ide_img = tmp / "ide.img"
        usb_img = tmp / "usb.img"
        shutil.copyfile(ROOT / "stdtest.img", ide_img)
        shutil.copyfile(ROOT / "stdtest.img", usb_img)
        storage_ok = run_case(
            "custombios-storage",
            [
                "qemu-system-i386",
                "-m", "32m",
                "-bios", str(ROOT / "qemu_flat_test_bios.bin"),
                "-M", "pc",
                "-serial", "stdio",
                "-monitor", "none",
                "-nographic",
                "-no-reboot",
                "-device", "isa-debug-exit,iobase=0xf4,iosize=0x04",
                "-drive", f"if=ide,format=raw,file={ide_img}",
                "-device", "piix4-usb-uhci,id=uhci",
                "-drive", f"if=none,id=usbdisk,format=raw,file={usb_img}",
                "-device", "usb-storage,drive=usbdisk,bus=uhci.0",
            ],
            [
                "IDE LBA0 @",
                "USB LBA0 @",
                "TEST int60_flat OK",
            ],
        )
    return 0 if std_ok and flat_ok and storage_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
