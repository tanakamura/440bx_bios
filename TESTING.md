# TESTING

QEMU で見える範囲の BIOS service / DOS 実行環境は、まず `make test` で確認する。

## 方針

- `src/stdtest.img`
  - QEMU 標準 BIOS で起動する FreeDOS テストイメージ
  - `AUTOEXEC.BAT` で `BIOSTEST.EXE` を実行する
- `src/custtest.img`
  - 廃止
- `src/custflat.img`
  - 自作 BIOS 用の埋め込み FreeDOS テストイメージ
  - `AUTOEXEC.BAT` で `FLATTEST.EXE` を実行する
- `src/qemu_flat_test_bios.bin`
  - `custflat.img` を sparse 化して埋め込んだ QEMU 用 BIOS

## 実行

```sh
make -C src test
```

これで以下を自動実行する。

1. `stdtest.img` を QEMU 標準 BIOS で起動
2. `qemu_flat_test_bios.bin` を QEMU 自作 BIOS として起動
3. `qemu_flat_test_bios.bin` に QEMU IDE disk と `piix4-usb-uhci` + `usb-storage` を付けて起動
4. それぞれの serial 出力を見て `OK/NG` を判定
5. DOS 側の `SHUTDOWN.EXE` で QEMU を `isa-debug-exit` 経由で終了する

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

### FLATTEST.EXE

自作 BIOS だけで見る。

- `INT 60h` 経由の flat `read8/write8`
- 自作 BIOS の `INT 12h` が `640 KiB` を返すこと

### Storage Scan

自作 BIOS の `postcar_resume` / QEMU entry で見る。

- PCI bus enumeration が完走すること
- PCI I/O BAR を割り当てて command register の I/O / bus master bit を有効にできること
- QEMU IDE disk に ATA IDENTIFY を投げ、PIO READ SECTORS で LBA0 を読めること
- QEMU `piix4-usb-uhci` + `usb-storage` を UHCI control / bulk transfer で enumerate し、USB Mass Storage Bulk-Only Transport の READ(10) で LBA0 を読めること

### SHUTDOWN.EXE

- `AUTOEXEC.BAT` の最後で `SHUTDOWN 42` を実行すると success
- fail path は `SHUTDOWN 43`
- `run_qemu_tests.py` は QEMU の終了コードで success/fail を見る

## 運用ルール

- BIOS service / thunk / DOS runtime に変更を入れたら、原則として毎回 `make -C src test` を回す
- QEMU で再現できる問題は、先に `make test` を通してから実機へ持っていく
- 実機専用の問題でも、QEMU で見える範囲の退行が無いことを先に確認する
- QEMU 用の埋め込み FreeDOS が 256KiB ROM 枠に収まらない場合は、今回のようにテスト内容を複数イメージへ分割して維持する

## 現在のギャップ

- `make test` は、QEMU 標準 BIOS 上の FreeDOS/BIOSTEST と、自作 BIOS 上の `INT 60h`/DOS 実行環境までは守る
- ただし **自作 BIOS で FreeDOS を最後まで起動して FreeCOM まで行く経路** は、現時点では 256KiB の QEMU テスト ROM 枠に収めきれておらず、`make test` では未カバー
- その経路は、実機確認または別の分割テスト手段で維持する
