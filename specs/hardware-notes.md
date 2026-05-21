# Hardware Notes

このファイルには、作業中に確定した 440BX / マザーボード依存の前提を追記する。

## ROM mapping

- ROM イメージ全体は `256 KiB`。
- リセット直後にそのまま見えるのは ROM イメージの上位 `64 KiB`。
- つまり CPU から自然に見えるのは ROM イメージの `192 KiB` から `256 KiB` の範囲。
- `f000:fff0` のリセットベクタは、その上位 `64 KiB` 窓の末尾に置く。
- ROM イメージの下位 `192 KiB` は、何らかの追加設定やバンク切り替えなしには見えない。

## Early boot constraints

- DRAM 初期化前は RAM を使えない。
- DRAM 初期化前はスタック使用禁止。
- DRAM 初期化前は `call`, `ret`, `push`, `pop` を使わない。
- DRAM 初期化前は RAM への読み書きをしない。

## Serial output

- 現在の初期デバッグ出力は `ttyS0` に出す。
- ボーレートは `115200`。

## POST code

- POST CODE は `out 0x80, al` で自由に出してよい。
- シリアル出力が信用できないときは、POST CODE を見て切り分ける。
- 人間が POST CODE を読む必要があるときは、チャットで明示して依頼する。

## Pentium II / CAR notes

- CPU は Pentium II で、Intel の分類では P6 family として扱う。
- P6 family では MTRR が使える。固定 MTRR は first `1 MiB` を `64 KiB`, `16 KiB`, `4 KiB` 単位で型付けできる。
- `CR0.CD=1, CR0.NW=1` は P6 family の reset state で、Intel SDM 上は memory coherency を維持しない mode。
- `CR0.CD=1, CR0.NW=0` は no-fill cache mode として説明されている。
- 今の CAR 実験では、固定 MTRR `IA32_MTRR_FIX4K_F8000` の `0xFE000-0xFFFFF` 側を `WB` にして、ROM 窓の上端近くに一時 stack を置く方針を試す。
- これは Intel SDM の MTRR / cache mode の記述に基づく最小実験で、Pentium II 実機で stack と `call` が通るかを実測で確認する。

## Timer / interrupt notes

- DOS programs such as FreeDOS `VDELAY.COM` wait for the BIOS tick count at
  `0040:006c` to advance. Updating the tick only on BIOS service calls is not
  enough once DOS programs run without calling back into BIOS.
- The BIOS installs a real-mode IRQ0 / `INT 08h` handler that increments the
  BDA tick count, sets the midnight flag at `0040:0070`, calls `INT 1Ch`, and
  sends EOI to the master 8259 PIC.
- Before booting DOS, the BIOS programs the PIT channel 0 to the standard
  `18.2 Hz` divisor and unmasks only IRQ0 on the 8259 PIC.
- On QEMU's i440fx/P6-like setup, IRQ0 did not reach real mode while the local
  APIC remained enabled without virtual-wire setup. The current BIOS disables
  the local APIC via `IA32_APIC_BASE.EN` before handing control to DOS, so the
  legacy 8259 `INTR` path works.

## Planned stage split

- The ROM is generated from a board/profile blob list. Stage1 is placed at the
  reset-visible end of the 256 KiB ROM; other payloads are recorded in the ROM
  payload directory.
- Current stage split:
  - stage1 performs CAR/DRAM init, installs the shared service table at the top
    of DRAM, and loads stage2 at `0x00080000`.
  - stage2 performs board-dependent chipset setup, including PAM/MTRR and ACPI
    table preparation, then loads stage3 at `0x00200000`.
  - stage3 is the common high-DRAM runtime. It loads apps through the shared
    blob service.
  - `app/legacy` and `app/linux_loader` are separate app payloads. Their load
    address is `0x000f0000`; stage3 itself is not placed in the shadow area.
- The runtime no longer reserves `0x80000` or `0x9fc00` for BIOS private state; conventional-memory size reported to DOS can be `640 KiB`.

## Current BIOS memory map policy

- Conventional memory `0x00000-0x9fbff` is reported usable.
- `0x9fc00-0xfffff` is reserved for EBDA-compatible holes, VGA/option ROM area, and the low app/thunk window at `0xf0000-`.
- `0x00100000` through `detected_dram_end - 1MiB` is reported usable by `INT 15h E820h`, `AH=88h`, and `E801h`.
- The top `1MiB` of detected DRAM is reserved for BIOS protected-mode stack and IDE/USB scratch buffers used by BIOS services after boot.
- RAM floppy staging has been removed; boot media should be supplied by IDE/USB storage.

## Clocking

- このボードでは FSB / CPU clock を一番遅い設定にすると、CPU の最小実行は通っても PCI config access や chipset 周辺が不安定になる可能性がある。
- `00:00.0` の `8086/7190` が見え始めたのは、FSB / CPU clock を CPU 想定の条件へ戻した後だった。
- PCI / SMBus / chipset 調査中は、極端に遅い clock 設定を避けて、Pentium II と P2B98-XV の想定条件で試す。

## P2B98-XV maintenance inputs

- ASUS P2B98-XV manual lists only two board jumpers in the jumper section:
  `CLRTC` and `INT`.
- `CLRTC` shorts the RTC/CMOS clear solder points. This is useful as an
  emergency recovery signal, but it destroys CMOS/NVRAM contents and is not a
  good normal maintenance-mode selector.
- `INT` is the onboard VGA interrupt selection jumper. It changes VGA interrupt
  routing and should not be repurposed as a maintenance-mode selector unless a
  software-visible bit is experimentally found.
- The front-panel connector area includes a 2-pin `SMI` lead. This is the best
  hardware candidate for a maintenance switch, but it should first be tested by
  polling PIIX4E PM/GPE/GPIO status registers. Do not enable SMI delivery until
  an SMM handler exists.
- Manual reference: ASUS P2B98-XV User's Manual `p2b98xv-200.pdf`.

## Observed SDRAM SPD

- SPD probing currently scans `0x50-0x53` on SMBus and does not assume a fixed slot.
- One confirmed run found a responding SPD EEPROM at `0x52`.
- Observed first 64 bytes at `0x52`:
  - `80 08 04 0c 08 01 40 00 01 a0 60 00 80 10 00 01`
  - `8f 04 06 01 01 00 0e a0 60 00 00 14 14 14 32 08`
  - `20 10 20 10 ...`
- Manual interpretation of the meaningful fields:
  - byte2 `0x04`: SDRAM
  - byte3 `0x0c`: 12 row address bits
  - byte4 `0x08`: 8 column address bits
  - byte5 `0x01`: 1 module bank / single-sided module
  - byte6..7 `0x0040`: 64-bit module data width
  - byte8 `0x01`: 3.3V signaling
  - byte9 `0xa0`: 10.0 ns cycle time
  - byte17 `0x04`: 4 internal banks per SDRAM device
  - byte18 `0x06`: CAS latency 2 and 3 supported
  - byte27 `0x14`: tRP 20 ns
  - byte28 `0x14`: tRRD 20 ns
  - byte29 `0x14`: tRCD 20 ns
  - byte30 `0x32`: tRAS 50 ns
  - byte31 `0x08`: 32 MB per physical row
- This looks like a plausible single-sided 64-bit SDR SDRAM DIMM, roughly a 32 MB PC100-class module built from x16 SDRAM devices.
- A confirmed bring-up run completed SDRAM init and passed a basic read/write test at
  `0x00100000` and `0x00100004`.
- The reliable version used a stackless SDRAM special-command sequence (`NOP`,
  `PRECHARGE`, `CBR`, `MRS`) because once `DRB` starts decoding low memory as DRAM,
  a CAR stack in the first MiB overlaps that decode window.
