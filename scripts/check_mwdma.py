#!/usr/bin/env python3
import argparse
import struct
import sys
import time

import monitor

PIIX4_IDE_BUS = 0
PIIX4_IDE_DEV = 7
PIIX4_IDE_FN = 1

IDE_STATUS_ERR = 0x01
IDE_STATUS_DRQ = 0x08
IDE_STATUS_DF = 0x20
IDE_STATUS_DRDY = 0x40
IDE_STATUS_BSY = 0x80

IDE_CMD_IDENTIFY = 0xEC
IDE_CMD_SET_FEATURES = 0xEF
IDE_CMD_READ_SECTORS = 0x20
IDE_CMD_READ_DMA = 0xC8
IDE_FEATURE_SET_TRANSFER_MODE = 0x03
IDE_XFER_MWDMA0 = 0x20

IDE_BM_CMD_START = 0x01
IDE_BM_CMD_TO_MEM = 1 << 3
IDE_BM_CMD_FROM_MEM = 0x00
IDE_BM_STATUS_ACTIVE = 0x01
IDE_BM_STATUS_ERROR = 0x02
IDE_BM_STATUS_INTR = 0x04
IDE_BM_STATUS_CLEAR = IDE_BM_STATUS_ERROR | IDE_BM_STATUS_INTR

PIIX4_IDETIM_DECODE_ONLY = 0x8000
PIIX4_IDETIM_PIO2_SWDMA2 = 0x9007
PIIX4_IDETIM_PIO3_MWDMA1 = 0xA107
PIIX4_IDETIM_PIO4_MWDMA2 = 0xA307


def ide_400ns_delay(m: monitor.Machine, ctrl: int):
    for _ in range(4):
        monitor.inb(m, ctrl)


def wait_not_busy_ms(m: monitor.Machine, io: int, timeout_ms: int) -> bool:
    deadline = time.monotonic() + timeout_ms / 1000.0
    while time.monotonic() < deadline:
        st = monitor.inb(m, io + 7)
        if st in (0x00, 0xFF):
            return False
        if (st & IDE_STATUS_BSY) == 0:
            return True
    return False


def wait_cmd_ready_ms(m: monitor.Machine, io: int, timeout_ms: int) -> bool:
    deadline = time.monotonic() + timeout_ms / 1000.0
    while time.monotonic() < deadline:
        st = monitor.inb(m, io + 7)
        if st in (0x00, 0xFF):
            return False
        if (st & IDE_STATUS_ERR) != 0:
            return False
        if (st & (IDE_STATUS_BSY | IDE_STATUS_DRQ)) == 0:
            return True
    return False


def wait_drq_ms(m: monitor.Machine, io: int, timeout_ms: int) -> bool:
    deadline = time.monotonic() + timeout_ms / 1000.0
    while time.monotonic() < deadline:
        st = monitor.inb(m, io + 7)
        if st in (0x00, 0xFF):
            return False
        if (st & IDE_STATUS_ERR) != 0 or (st & IDE_STATUS_DF) != 0:
            return False
        if (st & IDE_STATUS_BSY) == 0 and (st & IDE_STATUS_DRQ) != 0:
            return True
    return False


def identify_words_valid(words: list[int]) -> bool:
    if all(w == 0x0000 for w in words):
        return False
    if all(w == 0xFFFF for w in words):
        return False
    if words[0] in (0x0000, 0xFFFF):
        return False
    return True


def ide_identify(m: monitor.Machine, io: int, ctrl: int, drive: int):
    for attempt in range(4):
        monitor.outb(m, ctrl, 0x02)
        monitor.outb(m, io + 6, 0xA0 | (drive << 4))
        ide_400ns_delay(m, ctrl)
        st = monitor.inb(m, io + 7)
        if st in (0x00, 0xFF):
            return None
        if not wait_not_busy_ms(m, io, 3000):
            if attempt != 3:
                time.sleep(0.1)
                continue
            return None
        monitor.outb(m, io + 2, 0)
        monitor.outb(m, io + 3, 0)
        monitor.outb(m, io + 4, 0)
        monitor.outb(m, io + 5, 0)
        monitor.outb(m, io + 7, IDE_CMD_IDENTIFY)
        st = monitor.inb(m, io + 7)
        if st in (0x00, 0xFF):
            return None
        if not wait_not_busy_ms(m, io, 3000):
            if attempt != 3:
                time.sleep(0.1)
                continue
            return None
        if monitor.inb(m, io + 4) != 0 or monitor.inb(m, io + 5) != 0:
            return None
        if not wait_drq_ms(m, io, 3000):
            if attempt != 3:
                time.sleep(0.1)
                continue
            return None
        words = [monitor.inw(m, io) for _ in range(256)]
        if identify_words_valid(words):
            return words
        if attempt != 3:
            time.sleep(0.1)
    return None


def best_mwdma_mode(words: list[int]) -> int:
    if (words[49] & 0x0100) == 0:
        return -1
    modes = words[63] & 0x0007
    if modes & 0x0004:
        return 2
    if modes & 0x0002:
        return 1
    if modes & 0x0001:
        return 0
    return -1


def current_mwdma_mode(words: list[int]) -> int:
    active = (words[63] >> 8) & 0x0007
    if active & 0x0004:
        return 2
    if active & 0x0002:
        return 1
    if active & 0x0001:
        return 0
    return -1


def current_udma_mode(words: list[int]) -> int:
    active = (words[88] >> 8) & 0x003F
    for mode in range(5, -1, -1):
        if active & (1 << mode):
            return mode
    return -1


def supports_lba48(words: list[int]) -> bool:
    return (words[83] & 0xC000) == 0x4000 and (words[83] & 0x0400) != 0


def piix4_primary_idetim_for_mode(mode: int) -> int:
    if mode == (IDE_XFER_MWDMA0 | 0):
        return PIIX4_IDETIM_PIO2_SWDMA2
    if mode == (IDE_XFER_MWDMA0 | 1):
        return PIIX4_IDETIM_PIO3_MWDMA1
    if mode == (IDE_XFER_MWDMA0 | 2):
        return PIIX4_IDETIM_PIO4_MWDMA2
    return PIIX4_IDETIM_DECODE_ONLY


def set_transfer_mode(
    m: monitor.Machine, io: int, ctrl: int, drive: int, mode: int
) -> bool:
    monitor.pci_config_write16(
        m,
        PIIX4_IDE_BUS,
        PIIX4_IDE_DEV,
        PIIX4_IDE_FN,
        0x40,
        piix4_primary_idetim_for_mode(mode),
    )
    monitor.outb(m, ctrl, 0x00)
    monitor.outb(m, io + 6, 0xE0 | (drive << 4))
    ide_400ns_delay(m, ctrl)
    if not wait_cmd_ready_ms(m, io, 1000):
        return False
    monitor.outb(m, io + 1, IDE_FEATURE_SET_TRANSFER_MODE)
    monitor.outb(m, io + 2, mode)
    monitor.outb(m, io + 7, IDE_CMD_SET_FEATURES)
    return wait_not_busy_ms(m, io, 1000)


def print_channel_status(m: monitor.Machine, label: str, io: int, ctrl: int):
    st = monitor.inb(m, io + 7)
    alt = monitor.inb(m, ctrl)
    print(f"{label}: st=0x{st:02x} alt=0x{alt:02x}")


def write_mem(m: monitor.Machine, addr: int, data: bytes):
    for i in range(0, len(data), 4):
        chunk = data[i : i + 4]
        if len(chunk) == 4:
            monitor.write32(m, addr + i, struct.unpack("<I", chunk)[0])
        elif len(chunk) == 2:
            monitor.write16(m, addr + i, struct.unpack("<H", chunk)[0])
        else:
            for j, b in enumerate(chunk):
                monitor.write8(m, addr + i + j, b)


def read_mem(m: monitor.Machine, addr: int, size: int) -> bytes:
    out = bytearray()
    for i in range(0, size, 4):
        remain = min(4, size - i)
        if remain == 4:
            out.extend(struct.pack("<I", monitor.read32(m, addr + i)))
        elif remain == 2:
            out.extend(struct.pack("<H", monitor.read16(m, addr + i)))
        else:
            for j in range(remain):
                out.append(monitor.read8(m, addr + i + j))
    return bytes(out)


def build_prd(dest: int, bytes_count: int):
    entries: list[tuple[int, int]] = []
    while bytes_count != 0:
        boundary = 0x10000 - (dest & 0xFFFF)
        chunk = min(bytes_count, boundary, 0x10000)
        if chunk == 0:
            raise RuntimeError("invalid PRD chunk")
        count_field = 0 if chunk == 0x10000 else chunk
        entries.append((dest, count_field))
        dest += chunk
        bytes_count -= chunk
    last_base, last_count = entries[-1]
    entries[-1] = (last_base, last_count | 0x80000000)
    return entries


def dump_first_bytes(label: str, data: bytes, count: int = 16):
    shown = data[:count]
    hexs = " ".join(f"{b:02x}" for b in shown)
    print(f"{label}: {hexs}")


def do_dma_read(
    m: monitor.Machine,
    io: int,
    ctrl: int,
    bmio: int,
    drive: int,
    lba: int,
    count: int,
    dest: int,
    prd_addr: int,
):
    prd_entries = build_prd(dest, count * 512)
    prd_bytes = bytearray()
    for base, count_eot in prd_entries:
        prd_bytes.extend(struct.pack("<II", base, count_eot))
    write_mem(m, prd_addr, prd_bytes)
    print(f"PRD:({prd_addr:08x})")
    for idx, (base, count_eot) in enumerate(prd_entries[:4]):
        print(f"  prd{idx:02d}=0x{base:08x}/0x{count_eot:08x}")

    fill = bytes([0xCC]) * min(count * 512, 64)
    write_mem(m, dest, fill)

    monitor.outb(m, bmio + 0, IDE_BM_CMD_TO_MEM)
    monitor.outb(m, bmio + 2, IDE_BM_STATUS_CLEAR)
    monitor.outl(m, bmio + 4, prd_addr)
    monitor.wbinvd(m)
    print(
        f"prep bmcmd=0x{monitor.inb(m, bmio):02x} bmst=0x{monitor.inb(m, bmio + 2):02x} "
        f"st=0x{monitor.inb(m, io + 7):02x} err=0x{monitor.inb(m, io + 1):02x} prd=0x{monitor.inl(m, bmio + 4):08x}"
    )

    monitor.outb(m, ctrl, 0x00)
    monitor.outb(m, io + 6, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F))
    ide_400ns_delay(m, ctrl)
    if not wait_not_busy_ms(m, io, 1000):
        raise RuntimeError("drive not ready before READ DMA")

    monitor.outb(m, io + 2, count & 0xFF)
    monitor.outb(m, io + 3, lba & 0xFF)
    monitor.outb(m, io + 4, (lba >> 8) & 0xFF)
    monitor.outb(m, io + 5, (lba >> 16) & 0xFF)
    monitor.outb(m, io + 7, IDE_CMD_READ_DMA)

    monitor.outb(m, bmio + 0, IDE_BM_CMD_TO_MEM | IDE_BM_CMD_START)

    print(
        f"start bmcmd=0x{monitor.inb(m, bmio):02x} bmst=0x{monitor.inb(m, bmio + 2):02x} "
        f"st=0x{monitor.inb(m, io + 7):02x} err=0x{monitor.inb(m, io + 1):02x}"
    )

    deadline = time.monotonic() + 2.0
    last_bmst = 0
    while time.monotonic() < deadline:
        last_bmst = monitor.inb(m, bmio + 2)
        if (last_bmst & IDE_BM_STATUS_ERROR) != 0:
            break
        if (last_bmst & IDE_BM_STATUS_INTR) != 0:
            print("INTR set")
            break
        if (last_bmst & IDE_BM_STATUS_ACTIVE) == 0:
            print("ACTIVE cleared")
            break
    monitor.outb(m, bmio + 0, IDE_BM_CMD_TO_MEM)
    monitor.wbinvd(m)
    st = monitor.inb(m, io + 7)
    err = monitor.inb(m, io + 1)
    bmst = monitor.inb(m, bmio + 2)
    monitor.outb(m, bmio + 2, IDE_BM_STATUS_CLEAR)
    print(
        f"stop bmcmd=0x{monitor.inb(m, bmio):02x} bmst=0x{bmst:02x} "
        f"st=0x{st:02x} err=0x{err:02x} last_bmst=0x{last_bmst:02x} "
        f"prd=0x{monitor.inl(m, bmio + 4):08x} "
        f"buf=0x{dest:08x}"
    )
    buf = read_mem(m, dest, 64)
    dump_first_bytes("buf", buf)
    return {
        "st": st,
        "err": err,
        "bmst": bmst,
        "last_bmst": last_bmst,
        "buf": buf,
    }


def do_pio_read(
    m: monitor.Machine,
    io: int,
    ctrl: int,
    drive: int,
    lba: int,
    count: int,
    dest: int,
):
    data = bytearray()

    monitor.outb(m, ctrl, 0x00)
    monitor.outb(m, io + 6, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F))
    ide_400ns_delay(m, ctrl)
    if not wait_cmd_ready_ms(m, io, 1000):
        raise RuntimeError("drive not ready before READ SECTORS")
    monitor.outb(m, io + 2, count & 0xFF)
    monitor.outb(m, io + 3, lba & 0xFF)
    monitor.outb(m, io + 4, (lba >> 8) & 0xFF)
    monitor.outb(m, io + 5, (lba >> 16) & 0xFF)
    monitor.outb(m, io + 7, IDE_CMD_READ_SECTORS)
    print(
        f"pio start st=0x{monitor.inb(m, io + 7):02x} err=0x{monitor.inb(m, io + 1):02x}"
    )

    for sector in range(count):
        if not wait_drq_ms(m, io, 2000):
            st = monitor.inb(m, io + 7)
            err = monitor.inb(m, io + 1)
            print(f"pio sector {sector}: no DRQ st=0x{st:02x} err=0x{err:02x}")
            return None
        for _ in range(256):
            data.extend(struct.pack("<H", monitor.inw(m, io)))
        ide_400ns_delay(m, ctrl)

    write_mem(m, dest, data)
    st = monitor.inb(m, io + 7)
    err = monitor.inb(m, io + 1)
    print(f"pio stop st=0x{st:02x} err=0x{err:02x}")
    buf = data[:64]
    dump_first_bytes("buf", buf)
    return {"st": st, "err": err, "buf": buf}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dev-type", choices=["serial", "uds", "pipe"], default="uds")
    parser.add_argument("--dev-path", default="/tmp/ttyS0_bcast.sock")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--force-mwdma2", action="store_true")
    parser.add_argument("--dma-read", action="store_true")
    parser.add_argument("--pio-read", action="store_true")
    parser.add_argument("--lba", type=lambda x: int(x, 0), default=1)
    parser.add_argument("--count", type=lambda x: int(x, 0), default=None)
    parser.add_argument("--dest", type=lambda x: int(x, 0), default=0x00600000)
    parser.add_argument("--prd", type=lambda x: int(x, 0), default=0x0050A000)
    args = parser.parse_args()

    m = monitor.connect_machine(args.dev_type, args.dev_path, args.baudrate)
    monitor.init(m)

    ven_dev = monitor.pci_config_read32(
        m, PIIX4_IDE_BUS, PIIX4_IDE_DEV, PIIX4_IDE_FN, 0x00
    )
    cmd = monitor.pci_config_read16(m, PIIX4_IDE_BUS, PIIX4_IDE_DEV, PIIX4_IDE_FN, 0x04)
    bar4 = monitor.pci_config_read32(
        m, PIIX4_IDE_BUS, PIIX4_IDE_DEV, PIIX4_IDE_FN, 0x20
    )
    idetim_pri = monitor.pci_config_read16(
        m, PIIX4_IDE_BUS, PIIX4_IDE_DEV, PIIX4_IDE_FN, 0x40
    )
    idetim_sec = monitor.pci_config_read16(
        m, PIIX4_IDE_BUS, PIIX4_IDE_DEV, PIIX4_IDE_FN, 0x42
    )
    udmactl = monitor.pci_config_read8(
        m, PIIX4_IDE_BUS, PIIX4_IDE_DEV, PIIX4_IDE_FN, 0x48
    )
    udmatim = monitor.pci_config_read16(
        m, PIIX4_IDE_BUS, PIIX4_IDE_DEV, PIIX4_IDE_FN, 0x4A
    )

    print(f"IDE PCI id=0x{ven_dev:08x} cmd=0x{cmd:04x}")
    print(f"BAR4=0x{bar4:08x} BMIBA=0x{bar4 & 0xFFF0:04x}")
    print(f"IDETIM pri=0x{idetim_pri:04x} sec=0x{idetim_sec:04x}")
    print(f"UDMA ctl=0x{udmactl:02x} tim=0x{udmatim:04x}")
    print_channel_status(m, "pri", 0x1F0, 0x3F6)
    print_channel_status(m, "sec", 0x170, 0x376)
    # monitor.pci_config_write16(
    #    m, PIIX4_IDE_BUS, PIIX4_IDE_DEV, PIIX4_IDE_FN, 0x40, 0xA    print()

    found = False
    found_drive = -1
    for drive, name in ((0, "priM"), (1, "priS")):
        words = ide_identify(m, 0x1F0, 0x3F6, drive)
        if words is None:
            print(f"{name}: identify none")
            continue
        found = True
        found_drive = drive
        mwdma = best_mwdma_mode(words)
        mwdma_cur = current_mwdma_mode(words)
        udma_cur = current_udma_mode(words)
        lba48 = supports_lba48(words)
        sectors28 = words[60] | (words[61] << 16)
        print(
            f"{name}: identify ok total28=0x{sectors28:08x} "
            f"mwdma_sup={mwdma} mwdma_cur={mwdma_cur} "
            f"udma_cur={udma_cur} lba48={int(lba48)}"
        )
        print(
            f"{name}: id49=0x{words[49]:04x} id63=0x{words[63]:04x} "
            f"id83=0x{words[83]:04x} id88=0x{words[88]:04x}"
        )
        if args.force_mwdma2 or args.dma_read:
            ok = set_transfer_mode(m, 0x1F0, 0x3F6, drive, IDE_XFER_MWDMA0 | 2)
            if args.dma_read and not args.force_mwdma2:
                print(f"{name}: auto MWDMA2 before dma-read {'ok' if ok else 'fail'}")
            else:
                print(f"{name}: force MWDMA2 {'ok' if ok else 'fail'}")
            words2 = ide_identify(m, 0x1F0, 0x3F6, drive)
            if words2 is not None:
                print(
                    f"{name}: after setfeatures id63=0x{words2[63]:04x} "
                    f"id88=0x{words2[88]:04x} mwdma_cur={current_mwdma_mode(words2)} "
                    f"udma_cur={current_udma_mode(words2)}"
                )
        break

    if not found:
        print("RESULT: no primary drive detected")
        return 1
    if (bar4 & 0xFFF0) == 0:
        print("RESULT: BMIDE BAR4 missing")
        return 1
    if (cmd & 0x0001) == 0:
        print("RESULT: IDE I/O decode disabled")
        return 1

    count = args.count
    if count is None:
        count = 0xFF if args.dma_read else 1

    if args.dma_read:
        do_dma_read(
            m,
            io=0x1F0,
            ctrl=0x3F6,
            bmio=bar4 & 0xFFF0,
            drive=found_drive,
            lba=args.lba,
            count=count,
            dest=args.dest,
            prd_addr=args.prd,
        )
    if args.pio_read:
        do_pio_read(
            m,
            io=0x1F0,
            ctrl=0x3F6,
            drive=found_drive,
            lba=args.lba,
            count=count,
            dest=args.dest,
        )

    print("RESULT: python MWDMA probe done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
