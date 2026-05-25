#!/usr/bin/env python3
import argparse
import atexit
import os
import socket
import struct
from dataclasses import dataclass

try:
    import serial  # type: ignore
except ImportError:
    serial = None

INIT = 127

READ8 = 2
READ16 = 3
READ32 = 4

WRITE8 = 5
WRITE16 = 6
WRITE32 = 7

IN8 = 8
IN16 = 9
IN32 = 10

OUT8 = 11
OUT16 = 12
OUT32 = 13

RDMSR = 14
WRMSR = 15

LOADBIN = 16
RUNBIN = 17

LOADBIN16 = 18
RUNBIN16 = 19
WBINVD = 20

ACK = 0xFE
EOF = 0xFF


@dataclass
class Machine:
    to_mon: object
    from_mon: object


def connect_machine(dev_type: str, dev_path: str, baudrate: int) -> Machine:
    if dev_type == "serial":
        if serial is None:
            raise SystemExit("pyserial is required for --dev-type serial")
        port = serial.Serial(port=dev_path, baudrate=baudrate, parity="N", stopbits=1)
        atexit.register(port.close)
        return Machine(port, port)

    if dev_type == "pipe":
        base = dev_path
        out_path = base + ".out"
        in_path = base + ".in"
        if not os.path.exists(out_path) or not os.path.exists(in_path):
            raise SystemExit(f"missing qemu pipe endpoints: {in_path}, {out_path}")
        out_fp = open(out_path, "rb", buffering=0)
        in_fp = open(in_path, "wb", buffering=0)
        atexit.register(out_fp.close)
        atexit.register(in_fp.close)
        return Machine(in_fp, out_fp)

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(dev_path)
    atexit.register(sock.close)
    return Machine(sock.makefile("wb"), sock.makefile("rb"))


def _read_exact(fp, size: int) -> bytes:
    out = bytearray()
    while len(out) < size:
        chunk = fp.read(size - len(out))
        if not chunk:
            raise RuntimeError("monitor EOF")
        out.extend(chunk)
    return bytes(out)


def open_machine():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dev-type", choices=["serial", "uds", "pipe"], default="uds")
    parser.add_argument("--dev-path", default="/tmp/ttyS0_bcast.sock")
    parser.add_argument("--baudrate", type=int, default=115200)
    sub = parser.add_subparsers(dest="cmd", required=True)

    sub.add_parser("init")

    p = sub.add_parser("read8")
    p.add_argument("addr", type=lambda x: int(x, 0))
    p = sub.add_parser("read16")
    p.add_argument("addr", type=lambda x: int(x, 0))
    p = sub.add_parser("read32")
    p.add_argument("addr", type=lambda x: int(x, 0))

    p = sub.add_parser("write8")
    p.add_argument("addr", type=lambda x: int(x, 0))
    p.add_argument("value", type=lambda x: int(x, 0))
    p = sub.add_parser("write16")
    p.add_argument("addr", type=lambda x: int(x, 0))
    p.add_argument("value", type=lambda x: int(x, 0))
    p = sub.add_parser("write32")
    p.add_argument("addr", type=lambda x: int(x, 0))
    p.add_argument("value", type=lambda x: int(x, 0))

    p = sub.add_parser("in8")
    p.add_argument("port", type=lambda x: int(x, 0))
    p = sub.add_parser("in16")
    p.add_argument("port", type=lambda x: int(x, 0))
    p = sub.add_parser("in32")
    p.add_argument("port", type=lambda x: int(x, 0))

    p = sub.add_parser("out8")
    p.add_argument("port", type=lambda x: int(x, 0))
    p.add_argument("value", type=lambda x: int(x, 0))
    p = sub.add_parser("out16")
    p.add_argument("port", type=lambda x: int(x, 0))
    p.add_argument("value", type=lambda x: int(x, 0))
    p = sub.add_parser("out32")
    p.add_argument("port", type=lambda x: int(x, 0))
    p.add_argument("value", type=lambda x: int(x, 0))

    p = sub.add_parser("rdmsr")
    p.add_argument("msr", type=lambda x: int(x, 0))
    p = sub.add_parser("wrmsr")
    p.add_argument("msr", type=lambda x: int(x, 0))
    p.add_argument("value", type=lambda x: int(x, 0))

    p = sub.add_parser("loadbin")
    p.add_argument("path")
    p = sub.add_parser("runbin")

    p = sub.add_parser("loadbin16")
    p.add_argument("path")
    p = sub.add_parser("runbin16")
    sub.add_parser("wbinvd")

    p = sub.add_parser("pci-read8")
    p.add_argument("bus", type=lambda x: int(x, 0))
    p.add_argument("dev", type=lambda x: int(x, 0))
    p.add_argument("func", type=lambda x: int(x, 0))
    p.add_argument("off", type=lambda x: int(x, 0))
    p = sub.add_parser("pci-read16")
    p.add_argument("bus", type=lambda x: int(x, 0))
    p.add_argument("dev", type=lambda x: int(x, 0))
    p.add_argument("func", type=lambda x: int(x, 0))
    p.add_argument("off", type=lambda x: int(x, 0))
    p = sub.add_parser("pci-read32")
    p.add_argument("bus", type=lambda x: int(x, 0))
    p.add_argument("dev", type=lambda x: int(x, 0))
    p.add_argument("func", type=lambda x: int(x, 0))
    p.add_argument("off", type=lambda x: int(x, 0))
    p = sub.add_parser("pci-write8")
    p.add_argument("bus", type=lambda x: int(x, 0))
    p.add_argument("dev", type=lambda x: int(x, 0))
    p.add_argument("func", type=lambda x: int(x, 0))
    p.add_argument("off", type=lambda x: int(x, 0))
    p.add_argument("value", type=lambda x: int(x, 0))
    p = sub.add_parser("pci-write16")
    p.add_argument("bus", type=lambda x: int(x, 0))
    p.add_argument("dev", type=lambda x: int(x, 0))
    p.add_argument("func", type=lambda x: int(x, 0))
    p.add_argument("off", type=lambda x: int(x, 0))
    p.add_argument("value", type=lambda x: int(x, 0))
    p = sub.add_parser("pci-write32")
    p.add_argument("bus", type=lambda x: int(x, 0))
    p.add_argument("dev", type=lambda x: int(x, 0))
    p.add_argument("func", type=lambda x: int(x, 0))
    p.add_argument("off", type=lambda x: int(x, 0))
    p.add_argument("value", type=lambda x: int(x, 0))
    args = parser.parse_args()
    return connect_machine(args.dev_type, args.dev_path, args.baudrate), args


def init(m: Machine):
    m.to_mon.write(struct.pack("<B", INIT))
    m.to_mon.flush()
    while True:
        b = _read_exact(m.from_mon, 1)[0]
        if b == 1:
            return


def read8(m: Machine, addr: int) -> int:
    m.to_mon.write(struct.pack("<BI", READ8, addr))
    m.to_mon.flush()
    return _read_exact(m.from_mon, 1)[0]


def read16(m: Machine, addr: int) -> int:
    m.to_mon.write(struct.pack("<BI", READ16, addr))
    m.to_mon.flush()
    return struct.unpack("<H", _read_exact(m.from_mon, 2))[0]


def read32(m: Machine, addr: int) -> int:
    m.to_mon.write(struct.pack("<BI", READ32, addr))
    m.to_mon.flush()
    return struct.unpack("<I", _read_exact(m.from_mon, 4))[0]


def _expect_ack(m: Machine):
    ack = _read_exact(m.from_mon, 1)[0]
    if ack != ACK:
        raise RuntimeError(f"monitor ack failed: {ack:02x}")


def write8(m: Machine, addr: int, val: int):
    m.to_mon.write(struct.pack("<BIB", WRITE8, addr, val & 0xFF))
    m.to_mon.flush()
    _expect_ack(m)


def write16(m: Machine, addr: int, val: int):
    m.to_mon.write(struct.pack("<BIH", WRITE16, addr, val & 0xFFFF))
    m.to_mon.flush()
    _expect_ack(m)


def write32(m: Machine, addr: int, val: int):
    m.to_mon.write(struct.pack("<BII", WRITE32, addr, val & 0xFFFFFFFF))
    m.to_mon.flush()
    _expect_ack(m)


def inb(m: Machine, port: int) -> int:
    m.to_mon.write(struct.pack("<BH", IN8, port))
    m.to_mon.flush()
    return _read_exact(m.from_mon, 1)[0]


def inw(m: Machine, port: int) -> int:
    m.to_mon.write(struct.pack("<BH", IN16, port))
    m.to_mon.flush()
    return struct.unpack("<H", _read_exact(m.from_mon, 2))[0]


def inl(m: Machine, port: int) -> int:
    m.to_mon.write(struct.pack("<BH", IN32, port))
    m.to_mon.flush()
    return struct.unpack("<I", _read_exact(m.from_mon, 4))[0]


def outb(m: Machine, port: int, val: int):
    m.to_mon.write(struct.pack("<BHB", OUT8, port, val & 0xFF))
    m.to_mon.flush()
    _expect_ack(m)


def outw(m: Machine, port: int, val: int):
    m.to_mon.write(struct.pack("<BHH", OUT16, port, val & 0xFFFF))
    m.to_mon.flush()
    _expect_ack(m)


def outl(m: Machine, port: int, val: int):
    m.to_mon.write(struct.pack("<BHI", OUT32, port, val & 0xFFFFFFFF))
    m.to_mon.flush()
    _expect_ack(m)


def rdmsr(m: Machine, msr: int) -> int:
    m.to_mon.write(struct.pack("<BI", RDMSR, msr))
    m.to_mon.flush()
    lo, hi = struct.unpack("<II", _read_exact(m.from_mon, 8))
    return lo | (hi << 32)


def wrmsr(m: Machine, msr: int, val: int):
    m.to_mon.write(
        struct.pack("<BIII", WRMSR, msr, val & 0xFFFFFFFF, (val >> 32) & 0xFFFFFFFF)
    )
    m.to_mon.flush()
    _expect_ack(m)


def loadbin(m: Machine, payload: bytes, cmd: int = LOADBIN):
    checksum = 0
    for b in payload:
        checksum ^= b
    m.to_mon.write(struct.pack("<BIB", cmd, len(payload), checksum))
    m.to_mon.write(payload)
    m.to_mon.flush()
    ack = _read_exact(m.from_mon, 1)[0]
    if ack != ACK:
        raise RuntimeError(f"loadbin failed: {ack:02x}")


def runbin(m: Machine, cmd: int = RUNBIN):
    m.to_mon.write(struct.pack("<B", cmd))
    m.to_mon.flush()
    while True:
        b = _read_exact(m.from_mon, 1)
        if b[0] == EOF:
            break
    return struct.unpack("<I", _read_exact(m.from_mon, 4))[0]


def wbinvd(m: Machine):
    m.to_mon.write(struct.pack("<B", WBINVD))
    m.to_mon.flush()
    _expect_ack(m)


def pci_config_read8(m: Machine, bus: int, dev: int, func: int, off: int) -> int:
    outl(m, 0xCF8, (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (off & 0xFC))
    return inb(m, 0xCFC + (off & 3))


def pci_config_read16(m: Machine, bus: int, dev: int, func: int, off: int) -> int:
    outl(m, 0xCF8, (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (off & 0xFC))
    return inw(m, 0xCFC + (off & 2))


def pci_config_read32(m: Machine, bus: int, dev: int, func: int, off: int) -> int:
    outl(m, 0xCF8, (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (off & 0xFC))
    return inl(m, 0xCFC)


def pci_config_write8(m: Machine, bus: int, dev: int, func: int, off: int, val: int):
    outl(
        m,
        0xCF8,
        (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (off & 0xFC),
    )
    outb(m, 0xCFC + (off & 3), val)


def pci_config_write16(
    m: Machine, bus: int, dev: int, func: int, off: int, val: int
):
    outl(
        m,
        0xCF8,
        (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (off & 0xFC),
    )
    outw(m, 0xCFC + (off & 2), val)


def pci_config_write32(
    m: Machine, bus: int, dev: int, func: int, off: int, val: int
):
    outl(
        m,
        0xCF8,
        (1 << 31) | (bus << 16) | (dev << 11) | (func << 8) | (off & 0xFC),
    )
    outl(m, 0xCFC, val)


def main():
    m, args = open_machine()
    init(m)

    if args.cmd == "init":
        print("OK")
    elif args.cmd == "read8":
        print(f"0x{read8(m, args.addr):02x}")
    elif args.cmd == "read16":
        print(f"0x{read16(m, args.addr):04x}")
    elif args.cmd == "read32":
        print(f"0x{read32(m, args.addr):08x}")
    elif args.cmd == "write8":
        write8(m, args.addr, args.value)
        print("OK")
    elif args.cmd == "write16":
        write16(m, args.addr, args.value)
        print("OK")
    elif args.cmd == "write32":
        write32(m, args.addr, args.value)
        print("OK")
    elif args.cmd == "in8":
        print(f"0x{inb(m, args.port):02x}")
    elif args.cmd == "in16":
        print(f"0x{inw(m, args.port):04x}")
    elif args.cmd == "in32":
        print(f"0x{inl(m, args.port):08x}")
    elif args.cmd == "out8":
        outb(m, args.port, args.value)
        print("OK")
    elif args.cmd == "out16":
        outw(m, args.port, args.value)
        print("OK")
    elif args.cmd == "out32":
        outl(m, args.port, args.value)
        print("OK")
    elif args.cmd == "rdmsr":
        print(f"0x{rdmsr(m, args.msr):016x}")
    elif args.cmd == "wrmsr":
        wrmsr(m, args.msr, args.value)
        print("OK")
    elif args.cmd == "loadbin":
        with open(args.path, "rb") as f:
            loadbin(m, f.read(), LOADBIN)
        print("OK")
    elif args.cmd == "runbin":
        print(f"0x{runbin(m, RUNBIN):08x}")
    elif args.cmd == "loadbin16":
        with open(args.path, "rb") as f:
            loadbin(m, f.read(), LOADBIN16)
        print("OK")
    elif args.cmd == "runbin16":
        print(f"0x{runbin(m, RUNBIN16):08x}")
    elif args.cmd == "wbinvd":
        wbinvd(m)
        print("OK")
    elif args.cmd == "pci-read8":
        print(f"0x{pci_config_read8(m, args.bus, args.dev, args.func, args.off):02x}")
    elif args.cmd == "pci-read16":
        print(f"0x{pci_config_read16(m, args.bus, args.dev, args.func, args.off):04x}")
    elif args.cmd == "pci-read32":
        print(f"0x{pci_config_read32(m, args.bus, args.dev, args.func, args.off):08x}")
    elif args.cmd == "pci-write8":
        pci_config_write8(m, args.bus, args.dev, args.func, args.off, args.value)
        print("OK")
    elif args.cmd == "pci-write16":
        pci_config_write16(m, args.bus, args.dev, args.func, args.off, args.value)
        print("OK")
    elif args.cmd == "pci-write32":
        pci_config_write32(m, args.bus, args.dev, args.func, args.off, args.value)
        print("OK")


if __name__ == "__main__":
    main()
