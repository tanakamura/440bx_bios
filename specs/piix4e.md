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

## RTC / CMOS NVRAM

- The PIIX4/PIIX4E RTC contains 256 bytes of battery-backed CMOS SRAM split into
  two 128-byte banks.
- The standard bank is accessed through ISA I/O ports `0x70/0x71`.
- The extended bank is accessed through ISA I/O ports `0x72/0x73`.
- PIIX4E PCI ISA bridge `00:07.0` config register `0xcb` is `RTCCFG`.
  - bit0 `RTC_ENABLE`: enables standard bank decode at `0x70/0x71`.
  - bit2 `UPPER_RAM_EN`: enables extended bank decode at `0x72/0x73`.
- The extended CMOS bank is a good candidate for BIOS-owned small NVRAM data,
  such as a Linux command line suffix or maintenance settings. Store at least
  a magic word because battery-backed CMOS may contain arbitrary stale data.
- Without a CMOS battery, the standard RTC bank may also contain arbitrary
  values and `Status A` may not be in normal 32 kHz divider mode. Before
  booting Linux, the BIOS should enable RTC decode, program `Status A` to a
  normal divider/rate, program `Status B` to 24-hour BCD mode, and write a
  valid default date/time if the existing registers are invalid.
- Avoid relying on extended bank offsets `0x38-0x3f` for important data because
  PIIX4 has lock bits for those bytes.
- Current BIOS-owned extended CMOS layout starts at extended bank offset `0x00`:
  - `0x00-0x03`: magic word, bytes `B X N 2`
  - `0x04`: vmlinux partition index, currently `0` or `1`
  - `0x05`: `flags0`
    - bit0: enable full DRAM memtest.
    - bit1: Linux serial console command line enable.
    - bit2: VESA 1024x768 Linux boot enable.
    - bit3: run test blob. If set, BIOS prepares the same environment used
      before Linux handoff and runs the ROM embedded ELF test blob instead of
      booting Linux/DOS. The BIOS clears this bit before running the test, so
      the trigger is one-shot and a bad test image does not trap every boot.
  - `0x06`: boot priority, `0=auto`, `1=IDE`, `2=USB`
  - `0x07-0x7f`: NUL-terminated Linux command line suffix appended after the
    ROM built-in console options

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
- UHCI on this generation uses PCI interrupt pins. Without ACPI/MP/PIR tables, Linux needs at least usable legacy PCI IRQ routing. PIIX-style ISA bridge PCI config `0x60-0x63` are PIRQ A-D route controls; values with bit7 clear and low nibble `0x0b` route the PIRQ to IRQ11. The BIOS currently routes PIRQ A-D to IRQ11 and sets PCI `INTERRUPT_LINE` to `0x0b` for devices with an interrupt pin. IRQ11 must be level-triggered in ELCR (`0x4d1` bit3).
- On P2B98-XV, the ACPI DSDT `PX40` device is PCI `00:07.0`, so the PIRQ route registers used by LNKA-LNKD are PIIX4E ISA bridge config bytes `00:07.0 0x60-0x63`. If these remain at reset value `0x00`, ACPI reports the link as configured for IRQ0 and then disables it.

## IDE

- The PIIX4E IDE function is expected at PCI `00:07.1`, class code `0x0101xx`.
- The BIOS currently uses legacy PIO ports:
  - primary command/control: `0x1f0` / `0x3f6`
  - secondary command/control: `0x170` / `0x376`
- The scan path issues ATA `IDENTIFY DEVICE` (`0xec`) and, when that succeeds as ATA, reads LBA0 with READ SECTORS (`0x20`) using LBA28.
- Bus-master IDE BAR4 is assigned/enabled during PCI resource assignment.
- On at least one P2B98-XV board, PCI BAR4 size probing for the PIIX4E IDE
  function at `00:07.1` did not report a resource window during generic PCI
  BAR enumeration, and config `0x20` remained `0x0000` at boot. In that case,
  the BIOS must allocate a 16-byte I/O window itself, write BAR4 with
  `base | 1`, then use that non-zero `BMIBA` for bus-master IDE.
- If IDENTIFY word 88 reports Ultra DMA support, the BIOS selects the best PIIX4-supported UDMA mode 0-2, programs the drive with `SET FEATURES / SET TRANSFER MODE` using transfer mode `0x40 | mode`, enables that drive in `UDMACTL`, programs `UDMATIM`, and uses PIIX4 bus-master IDE READ DMA with a PRD table for kernel payload reads. If UDMA setup is not available, it falls back to the best reported multiword DMA mode. If READ DMA fails, it disables DMA for that boot and falls back to multi-sector PIO.
- IDE DMA reads use up to 256 sectors per ATA `READ DMA` command. If the drive reports 48-bit LBA support, the BIOS uses `READ DMA EXT` and groups reads up to 2048 sectors per command, currently limited by the PRD table size. Do not put `wbinvd` around every IDE DMA command: 440BX/Pentium II PCI bus-master DMA is cache coherent, and per-64KiB/128KiB `wbinvd` dominates Linux kernel load time. The Linux jump path uses a cheap serializing `cpuid`, not `wbinvd`.
- PIIX4 IDE timing registers:
  - PCI config `0x40-0x41` is primary `IDETIM`
  - PCI config `0x42-0x43` is secondary `IDETIM`
  - `IDETIM` bit15 is `IDE Decode Enable`; it must be set for PCI I/O cycles targeting ATA command/control blocks to be driven onto the IDE interface. If clear, accesses can be subtractively decoded toward ISA instead and no ATA device responds at `0x1f0/0x170`.
- PIIX4 Ultra DMA/33 registers:
  - PCI config `0x48` is `UDMACTL`; bit0 primary master, bit1 primary slave, bit2 secondary master, bit3 secondary slave. Set the bit to enable UDMA for that drive.
  - PCI config `0x4a-0x4b` is `UDMATIM`; primary master bits `1:0`, primary slave `5:4`, secondary master `9:8`, secondary slave `13:12`.
  - `UDMATIM` cycle-time field values are `00` for UDMA0 / 120 ns strobe, `01` for UDMA1 / 90 ns, and `10` for UDMA2 / 60 ns. `11` is reserved.

## References consulted

- Intel, 82371AB (PIIX4) PCI ISA IDE Xcelerator Datasheet.
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
- With custom BIOS, QEMU exposes generated ACPI tables through fw_cfg file `etc/acpi/tables`. The BIOS copies this file from fw_cfg for QEMU boots instead of using the real board `specs/dsdt.dsl`.
- QEMU fw_cfg ACPI tables need firmware-side fixups: rebuild RSDP/RSDT pointers for the copied RAM address, patch FADT FACS/DSDT pointers, fix table checksums, and convert FADT PM block offsets to QEMU PIIX ACPI PM base `0xb000`.
- QEMU PIIX ACPI PM I/O decode needs PCI `00:01.3` config `0x40 = 0x0000b001`, config `0x80 = 0x81`, and command bit0 set. Without this, Linux sees the ACPI PM timer at `0xb008` but reads `0xffffff`.
- Before handing off to Linux, the BIOS disables PM1/GPE enables and clears latched status bits. The real-board FADT currently advertises PM1 event at `0xe400`, PM1 control at `0xe404`, PM timer at `0xe408`, and GPE0 at `0xe40c`; QEMU uses the same offsets under PM base `0xb000`.
- The original ASUS FADT advertises `SMI_CMD=0xb2` and `ACPI_ENABLE=0xa1`,
  which assumes a working SMI handler. This custom BIOS does not install SMM,
  so the real-board FADT generated by the BIOS must not ask Linux to enter
  ACPI through SMI. Instead, the BIOS enables PM I/O decode itself and sets
  `PM1_CNT.SCI_EN` before handoff.
