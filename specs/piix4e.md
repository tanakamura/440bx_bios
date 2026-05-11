# PIIX4E Notes

このファイルには Intel 82371AB/EB (PIIX4/PIIX4E) の仕様メモを蓄積する。

## SMBus

- SMBus host controller is at PCI function `00:07.3` on this board.
- `00:07.3` vendor ID has been observed as `8086`.
- PCI config register `0x90` is `SMBBA`.
- PCI config register `0xD2` is `SMBHSTCFG` / host configuration area.
- SMBus host I/O register block is PIIX4-compatible.
- PCI config command register `0x04` bit0 enables I/O decode for the function.

SMBBA に16bit アドレスを書くとそこのIO空間に SMBus 関連レジスタが見える。
SMBBA は 16byte align, 0xfff0 でマスクがかかる。

base + 0 : SMBHSTSTS 8bit
base + 1 : SMBSLVSTS 8bit
base + 2 : SMBHSTCNT 8bit
base + 3 : SMBHSTCMD 8bit
base + 4 : SMBHSTADD 8bit
base + 5 : SMBHSTDAT0 8bit
base + 6 : SMBHSTDAT1 8bit

## SMBus host status / control bits

- `SMBHSTSTS`
  - bit0 : `HOST_BUSY`
  - bit1 : host completion / interrupt status
  - bit2 : `DEV_ERR`
  - bit3 : `BUS_ERR`
  - bit4 : `FAILED`
- `SMBHSTCNT`
  - bit6 : `START`
  - protocol code `0x08` : `BYTE_DATA`

## Observed behavior on this board

- `00:07.3` low 16 bits have been observed as `8086`.
- `SMBBA` has been observed as `0x1001`, so the effective base looks like `0x1000`.
- `SMBHSTCFG` was observed as `0x0000` before explicit enable.
- SMBus / PCI probing became much more stable after restoring CPU/FSB clocking from the slowest setting back to CPU-appropriate settings.
- Fast A20 / system control port `0x92` is present enough to use from `main.c`.
- When using port `0x92`, set bit1 for A20 enable and keep bit0 clear to avoid fast reset.
- On one confirmed SPD probe run:
  - initial `SMBBA=0000`
  - after writing `SMBBA=0x1000`, readback stayed `0x1000`
  - after setting `SMBHSTCFG bit0`, readback became `0x0001`
  - SPD `0x52` responded with data
  - SPD `0x50`, `0x51`, `0x53` returned host status `0x04`

## USB

- ACPI DSDT on this board exposes `USB0` at PCI address `00:07.2`.
- Therefore the PIIX4E USB host function on this board is expected at PCI function `00:07.2`.
- This is the legacy Intel USB host of the era, so software should expect a UHCI-style controller rather than OHCI/EHCI/XHCI.
- UHCI class code is `0x0c0300` and its I/O base is PCI BAR4 / `USBBASE`; the register block is I/O mapped.
- UHCI I/O offsets used by the BIOS:
  - `+0x00` `USBCMD` 16-bit
  - `+0x02` `USBSTS` 16-bit
  - `+0x04` `USBINTR` 16-bit
  - `+0x06` `FRNUM` 16-bit
  - `+0x08` `FLBASEADD` 32-bit
  - `+0x0c` `SOFMOD` 8-bit
  - `+0x10` / `+0x12` root port status/control
- UHCI frame list is 1024 dwords and must be 4KiB aligned. TDs must be 16-byte aligned. The current BIOS places UHCI descriptors/buffers in low DRAM at `0x00600000` and uses `wbinvd` around controller execution because descriptors live in write-back cached DRAM.
- UHCI TD token uses PID values `SETUP=0x2d`, `IN=0x69`, `OUT=0xe1`; max length is encoded as `len - 1`, with `0x7ff` for zero length.
- USB mass storage scan currently implements the Bulk-Only Transport minimum path: device/config descriptor reads, set address, set configuration, first mass-storage interface with protocol `0x50`, then CBW + SCSI READ(10) LBA 0 + CSW.

## IDE

- The PIIX4E IDE function is expected at PCI `00:07.1`, class code `0x0101xx`.
- The BIOS currently uses legacy PIO ports:
  - primary command/control: `0x1f0` / `0x3f6`
  - secondary command/control: `0x170` / `0x376`
- The scan path issues ATA `IDENTIFY DEVICE` (`0xec`) and, when that succeeds as ATA, reads LBA0 with READ SECTORS (`0x20`) using LBA28.
- Bus-master IDE BAR4 is assigned/enabled during PCI resource assignment, but the current boot scan reads via PIO only.

## References consulted

- Intel, Universal Host Controller Interface (UHCI) Design Guide, Revision 1.1.
- USB-IF, USB Mass Storage Class Bulk-Only Transport, Revision 1.0.

## QEMU approximation

- Local QEMU provides a `piix4-usb-uhci` PCI device.
- `qemu-system-i386 -device help` confirms:
  - `piix4-usb-uhci`
  - `addr=` property is available, so it can be placed at a chosen PCI slot/function.
- A close approximation for this board is:
  - `-machine pc-i440fx-...`
  - `-device piix4-usb-uhci,addr=07.2,multifunction=on`
- This only approximates the USB host function, not the whole 440BX + PIIX4E chipset combination.
