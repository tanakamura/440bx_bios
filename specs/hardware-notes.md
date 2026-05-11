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

## Planned stage split

- The current plan is to split the ROM into:
  - a `16 KiB` bootblock in the reset-visible top window
  - a larger BIOS payload stored in the rest of the ROM
- For bring-up, the BIOS payload will be copied/decompressed into DRAM at `0x00100000`.
- This `0x00100000` choice is a temporary staging address for bring-up, not the final long-term BIOS/DOS-compatible runtime placement.
- Current stage split:
  - bootblock lives in the top `16KiB` ROM window at `0xFC000-0xFFFFF`
  - `BIOS.elf` is linked separately and copied to DRAM at `0x00100000`
  - `BIOS.elf` keeps the protected-mode C service code in high DRAM and copies only the real-mode thunk/runtime tables to `0xf0000-` shadow DRAM
  - `BIOS.elf` places its 32-bit C entry at `0x00101000`
- The runtime no longer reserves `0x80000` or `0x9fc00` for BIOS private state; conventional-memory size reported to DOS can be `640 KiB`.

## Clocking

- このボードでは FSB / CPU clock を一番遅い設定にすると、CPU の最小実行は通っても PCI config access や chipset 周辺が不安定になる可能性がある。
- `00:00.0` の `8086/7190` が見え始めたのは、FSB / CPU clock を CPU 想定の条件へ戻した後だった。
- PCI / SMBus / chipset 調査中は、極端に遅い clock 設定を避けて、Pentium II と P2B98-XV の想定条件で試す。

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
