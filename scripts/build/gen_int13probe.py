from pathlib import Path


def main() -> None:
    base = Path("../freedos_boot_fd.img").read_bytes()
    boot = Path("int13probe.bin").read_bytes()
    if len(boot) != 512:
        raise SystemExit(f"unexpected boot sector size: {len(boot)}")
    image = bytearray(base)
    image[:512] = boot
    Path("int13probe.img").write_bytes(image)
    print("wrote int13probe.img")


if __name__ == "__main__":
    main()
