# TESTING

QEMU で見える範囲の BIOS service / boot path は、まず `make test` で確認する。

## 方針

- `src/stdtest.img`
  - QEMU 標準 BIOS で起動する FreeDOS テストイメージ
  - `AUTOEXEC.BAT` で `BIOSTEST.EXE` を実行する
- `src/custtest.img`
  - 廃止
- `src/qemu_flat_test_bios.bin`
  - 自作 BIOS の QEMU テスト ROM
  - RAM floppy は埋め込まない

## 実行

```sh
make -C src test
```

これで以下を自動実行する。

1. `stdtest.img` を QEMU 標準 BIOS で起動
2. `qemu_flat_test_bios.bin` に IDE MBR image を付けて、IDE MBR boot path を起動
3. `qemu_flat_test_bios.bin` に IDE Linux probe image を付けて、raw partition ELF loader を起動
4. `qemu_flat_test_bios.bin` に IDE Linux raw-disk probe image を付けて、disk 先頭 ELF loader を起動
5. `qemu_flat_test_bios.bin` に USB MBR image を付けて、USB MBR boot path を起動
6. それぞれの serial 出力を見て `OK/NG` を判定
7. DOS 側の `SHUTDOWN.EXE` またはテスト MBR / Linux probe で QEMU を `isa-debug-exit` 経由で終了する

## テスト内容

### BIOSTEST.EXE

QEMU 標準 BIOS で見る。

- `INT 11h`
- `INT 12h`
- `INT 13h AH=08`
- `INT 13h AH=15`
- `INT 13h` で boot sector read
- `INT 13h` で FAT12 root dir read
- `INT 1Ah AH=00`
- `INT 1Ah AH=02`

### Storage Scan

自作 BIOS の `postcar_resume` / QEMU entry で見る。

- PCI bus enumeration が完走すること
- PCI I/O BAR を割り当てて command register の I/O / bus master bit を有効にできること
- QEMU IDE disk に ATA IDENTIFY を投げ、PIO READ SECTORS で LBA0 を読めること
- QEMU `piix4-usb-uhci` + `usb-storage` を UHCI control / bulk transfer で enumerate し、USB Mass Storage Bulk-Only Transport の READ(10) で LBA0 を読めること

### MBR Boot

自作 BIOS で見る。

- IDE または USB Mass Storage の LBA0 を BIOS boot sector として `0x7c00` へ読むこと
- MBR 判定された場合、boot drive として `DL=0x80` を渡して real mode entry へ入ること
- IRQ0 timer が動き、BDA tick count `0040:006c` が増えること
- `INT 60h` 経由の flat `read8/write8` が動くこと
- `INT 15h E820h` が現在の自作 BIOS 用メモリマップを返すこと
- `src/usbmbr.asm` の最小 MBR が `USBMBR` を出して `isa-debug-exit` で success を返すこと

### Linux Probe

自作 BIOS で見る。

- IDE disk の第1 MBR partition 先頭を raw ELF32 i386 `vmlinux` 相当として認識すること
- partition table が無く disk 先頭が raw ELF32 i386 の場合も `vmlinux` 相当として認識すること
- ELF program header の `PT_LOAD` を physical address へ読み込むこと
- 第2 partition を optional initrd として読み込み、boot params の `ramdisk_image` / `ramdisk_size` に渡すこと
- boot params の setup header / E820 / cmdline を作り、`ESI=boot_params` で 32bit entry へ入ること
- `src/linuxprobe.asm` が `LINUXPROBE` を出して `isa-debug-exit` で success を返すこと

### ROM Test ELF

S3 / ACPI / Linux handoff 相当の状態を Linux 起動なしで確認するため、
ROM には `src/s3test.c` から作る ELF test blob を入れる。

- `flags0` bit0 は memtest 用。test blob 起動には使わない
- CMOS extended bank の BIOS-owned `flags0` bit3 を 1 にすると、次回起動で serial 入力なしに test blob を起動する
- BIOS は test blob 実行前に bit3 を clear するため、この trigger は one-shot
- test blob 実行前に ACPI table / PM I/O / boot params を Linux handoff 直前相当に作る
- test blob は uACPI をリンクし、BIOS が用意した RSDP から namespace load / initialize して `\_PTS(3)` を実行する
- 現時点の `make -C src test` は通常 boot path の退行テストであり、CMOS bit3 を立てた実機 test blob path は別途確認する

### SHUTDOWN.EXE

- `AUTOEXEC.BAT` の最後で `SHUTDOWN 42` を実行すると success
- fail path は `SHUTDOWN 43`
- `run_qemu_tests.py` は QEMU の終了コードで success/fail を見る

## 運用ルール

- BIOS service / thunk / DOS runtime に変更を入れたら、原則として毎回 `make -C src test` を回す
- QEMU で再現できる問題は、先に `make test` を通してから実機へ持っていく
- 実機専用の問題でも、QEMU で見える範囲の退行が無いことを先に確認する
- 自作 BIOS の QEMU テストは RAM floppy に依存しない。必要な boot media は IDE/USB disk として付ける

## 現在のギャップ

- `make test` は、QEMU 標準 BIOS 上の FreeDOS/BIOSTEST と、自作 BIOS 上の IDE/USB MBR boot、Linux raw partition boot、`INT 60h`、`INT 15h E820h` までは守る
- ただし **自作 BIOS で FreeDOS を最後まで起動して FreeCOM まで行く経路** は、現時点では外部 IDE/USB image が必要なので、`make test` では未カバー
- その経路は、実機確認または別の分割テスト手段で維持する
