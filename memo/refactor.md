# refactor

stage とディレクトリを整理する。移動だけで済むものは先に機械的に移し、ABI/配置変更は手順を分けて行う。

## 現状との矛盾点

- `stage2` のロード先は現状 `0x00080000`。旧メモの `0x00100000` は誤り。
- `stage3` のロード先は現状 `0x000F0000` だが、これは移動する。`0x000F0000` は `app/legacy` や `app/linux_loader` を置く低位 app slot として使う。
- `stage3` は「PCI/IDE/UHCI など標準処理」と「legacy BIOS service / Linux loader / selftest」を全部含んでいる。責務を分け、`stage3` は高位 DRAM 上の実行基盤、`app/` は個別ロードアドレスを持つ機能モジュールとして整理する。
- `shared_service` は全 stage に静的リンクするものではなく、stage1 が DRAM 末尾に設置し、DRAM 最後 4 byte の table pointer から辿って呼ぶ runtime service。現状は blob loader がこれに該当する。
- ACPI table は board 依存入力を持つ。P2B98-XV は `specs/dsdt.dsl` 由来、QEMU は fw_cfg 由来にする必要がある。
- `srcipts/` は typo。スクリプトは repo root の `scripts/` に集約する。

## 方針

- stage1/stage2 は board ごとに分ける。
- 各 stage は次 stage へ移った後に破壊される前提で作る。
- stage3 は board 非依存の BIOS runtime とし、`0x000F0000` には置かない。
- `0x000F0000-0x000FFFFF` は legacy/Linux loader 用 app slot として使う。他の app は app blob 側でロードアドレスを決める。
- stage 間でシンボルを相互参照しない。共通データと関数は shared service table 経由で受け渡す。
- board 固有情報は stage2 が shared service table 上の boot context に詰めて stage3 に渡す。
- `bios_main.c` は stage3 entry と機能モジュールへ分割する。
- blob format / POST code / memory map / shared service table は C と asm で定数を重複させない。
- QEMU と実機で同じ stage3 を使う。違いは stage1/stage2 と shared service table の内容だけに閉じ込める。
- uACPI は自作 AML interpreter の代わりに使う。ただし stage3 には常駐させず、selftest blob 側だけで使う。

## 目標ディレクトリ

```
src/
  include/
    boot_context.h
    memory_map.h
    blob.h
    post_code.h

  platform/
    p2b98_xv/
      stage1/
        start.asm
        start.ld
        bootblock.c
      stage2/
        stage2.c
        stage2.ld
        l2_cache.c
        l2_cache.h
        acpi_platform.c
    qemu/
      stage1/
        qemu_start.asm
        qemu.ld
      stage2/
        stage2.c
        stage2.ld
        acpi_fwcfg.c

  shared_service/
    blob_service.c
    blob_service.h
    service_table.h
    service_table_offsets.inc
    heap.c

  stage3/
    entry.c
    stage3.ld
    boot_context.c
    maintenance.c
    nvram.c

  app/
    legacy/
      legacy.ld
      bios16.asm
      int10.c
      int11.c
      int12.c
      int13.c
      int15.c
      int16.c
      int1a.c
    linux_loader/
      linux_loader.ld
      linux_loader.c
      elf_loader.c
      linux_params.c
    selftest/
      s3test/
        s3test.c
        uacpi_kernel.c
        uacpi_config.h
        s3test.ld

  lib/
    x86/
      io.h
      msr.h
      mtrr.c
      pic.c
      pit.c
      rtc.c
    serial/
      serial.c
      serial.h
    pci/
      pci.c
      pci.h
      pci_assign.c
    acpi/
      acpi_tables.c
      acpi_tables.h
    storage/
      blockdev.h
      ide.c
      uhci.c
      usb_storage.c
```

Repo root:

```
scripts/
  build/
  test/
  board/

third_party/
  uACPI/

tools/
  dos/
    flashutil.c
    mem_dmp.c
    xrecv.c
```

## メモリマップ

整理後の目標メモリマップ。

```
0x00080000  stage2 load address
0x00090000  stage2 end limit
0x000C0000  option ROM / VGA BIOS window
0x000F0000  legacy/linux_loader app slot load address
0x00100000  legacy/linux_loader app slot end limit
0x00200000  stage3 load address
DRAM end-4  shared service table pointer
DRAM tail   shared service code, table, heap, common data
before tail per-stage stack initial top
```

ROM は 256KiB として扱う。

```
low alias   0x000C0000-0x000FFFFF
high alias  0xFFFC0000-0xFFFFFFFF
reset       0xFFFFFFF0
```

ROM file offset / low alias:

```
0x00000-0x01fff  0x000c0000-0x000c1fff  ROM header + payload directory
0x02000-0x3bfff  0x000c2000-0x000fbfff  payload area
0x3c000-0x3ffff  0x000fc000-0x000fffff  stage1 reset/bootblock
```

stage1 は ROM header の payload directory を読み、shared service table 上の payload manifest を作る。stage2 以降は ROM linker symbol を直接参照しない。

stage1 は reset vector から始まり、DRAM 初期化前は RAM/stack/call/push/pop 禁止を維持する。DRAM 初期化後に CAR から DRAM stack へ移り、shared service を DRAM 末尾へ設置し、stage2 を DRAM に展開する。

## stage lifetime

各 stage は、自分の実行終了後に破壊されてもよい前提で作る。

- stage1 は stage2 へ jump した後に破壊されてよい。
- stage2 は stage3 へ jump した後に破壊されてよい。
- stage3 は app を展開するとき、以前の app を破壊してよい。
- stage 間で「前 stage のグローバル変数」「前 stage の関数」「ROM 内 linker symbol」を直接参照しない。
- 生き残る必要があるデータと関数は、すべて shared service table に置く。

## stage1

責務:

- reset vector から最小初期化を行う。
- P2B98-XV では SuperIO UART, CAR, SPD read, DRAM init, A20 enable を行う。
- QEMU では DRAM が最初から使える前提で、同じ ABI に合わせて stage2 をロードする。
- PIC な shared service code を DRAM 末尾へコピーする。
- shared service table を DRAM 末尾へ作る。
- DRAM 最後 4 byte に shared service table pointer を書く。
- shared service table 上の boot context を初期化する。
- stage2 blob を `0x00080000` に展開して jump する。

stage1 に入れないもの:

- PCI resource assign
- IDE/UHCI scan
- Linux loader
- legacy BIOS service
- ACPI AML 実行

## stage2

責務:

- board 固有初期化を行う。
- P2B98-XV では L2 cache init, PAM/MTRR shadow 設定, board DSDT/ACPI 入力作成を行う。
- QEMU では fw_cfg から ACPI table 情報を取得する。
- stage3 が必要とする board 情報を shared service table 上の boot context に書く。
- stage3 blob を高位 DRAM に展開して jump する。ロード先は `0x00200000` とする。

stage2 に入れないもの:

- 汎用 PCI tree scan/resource assign
- 汎用 storage scan
- OS loader 本体
- INT handler 実装

## stage3

stage3 は board 非依存 BIOS runtime。入口は P2B98-XV と QEMU で共通にする。

stage3 は `0x000F0000` には置かない。stage3 は `0x00200000` で動き、必要な app を app blob に指定されたロードアドレスへ展開して呼ぶ。

責務:

- `boot_context` の検証。
- BDA/EBDA/IVT/GDT の設置。
- app blob のロードと実行。
- legacy BIOS app のロードと登録。
- PCI enumeration と resource assignment。
- IDE/UHCI/USB storage scan。
- ACPI table expose。
- maintenance mode。
- boot priority に従って Linux loader / legacy boot / selftest を起動。

stage3 自体には board 固有 register 値を直接持たせない。必要な host bridge window, ACPI 入力, blob pointer は shared service table 経由で受け取る。

## shared service

shared service は stage 間 ABI の唯一の窓口にする。全 stage と app は、共通データと共通関数を shared service table 経由で使う。

配置:

- shared service 関数は PIC でビルドする。
- stage1 が DRAM 初期化後に shared service code と shared service table を DRAM 末尾へ配置する。
- DRAM 最後 4 byte に shared service table pointer を置く。
- table pointer が差す先に shared service table を置く。
- shared service table から code/data/heap へ到達できるようにする。

禁止:

- stage1/stage2/stage3/app 間の linker symbol 直接参照。
- stage1/stage2 のグローバルデータを stage3 が参照すること。
- ROM 内 blob symbol を stage2/stage3 が直接参照すること。
- 固定アドレス `BLOB_SERVICE_LINEAR` のような service entry 直接呼び出し。

許可:

- stage が自分自身の blob 内 symbol を使うこと。
- stage が shared service table pointer を DRAM 最後 4 byte から読むこと。
- stage が shared service table 内の関数 pointer と data pointer を使うこと。

shared service table には、少なくとも以下を入れる。

```
magic
version
size
crc32 or checksum
total_dram_bytes
service_base
service_size
table_linear
table_size
stack_top
heap_base
heap_free_list
heap_limit
boot_context_ptr
payload_manifest_ptr
blob_expand
blob_load
heap_alloc
heap_free
heap_realloc
crc32
memcpy
memset
serial_write
post_code
panic
```

`blob_expand` は、ROM blob pointer と destination を受け取り、block CRC と retry を shared service 内で処理する。stage2/stage3/app は LZ4 や CRC の実装を直接持たない。stage2 以降は ROM linker symbol を直接参照せず、shared service table の `payload_manifest_ptr` から payload を探す。

## payload manifest

build 時は linker が ROM header + payload directory を作る。stage1 は ROM header の payload directory を読み、DRAM 上の shared service table へ payload manifest としてコピーする。stage2 以降は ROM linker symbol を直接参照せず、payload manifest 経由で payload を探す。

最低限:

```
magic
version
entry_count
entries[]
```

entry:

```
id
type
flags
blob_ptr
blob_size
slot_size
```

`id` は `stage2`, `stage3`, `legacy_app`, `linux_loader_app`, `vgabios`, `dsdt`, `test_elf`, `test_floppy` など。`blob_ptr` は BLZ4 blob か app blob を指す。app blob のロードアドレスは app blob 先頭の `load_addr` を使う。

`slot_size` は ROM 上でその payload に予約した最大サイズ。テスト時は `blob_size <= slot_size` の範囲で payload を書き換えてよい。BLZ4 内に CRC があるので、directory 側に payload CRC は持たせない。directory 自体には header checksum を持たせる。

### legacy test media

legacy BIOS service のテスト用に `test_floppy` payload を予約する。

- `test_floppy` は BLZ4 blob として持つ。
- `test_floppy` は ROM 上の固定 slot に置く。
- test runner は ROM file の `test_floppy` slot だけを書き換え、directory の `blob_size` と header checksum を更新する。
- stage3/legacy は `test_floppy` payload があれば、展開して INT 13h の floppy image として使う。
- 小さい regression floppy は通常 ROM の空き slot に入れる。
- FreeDOS など大きい floppy image を使う場合は、`legacy-test` ROM profile で `vgabios` や `test_elf` を外して `test_floppy` slot を大きく取る。

## app blob format

app blob は既存 BLZ4 blob の先頭に 4 byte の load address を追加する。

```
offset  size  name
0x00    4     load_addr
0x04    ...   BLZ4 blob
```

CRC と block retry は BLZ4 blob 側の既存機構を使う。stage3 は `load_addr` を読み、`0x04` 以降の BLZ4 blob を `load_addr` へ展開して app を起動する。

原則:

- `legacy` と `linux_loader` の `load_addr` は `0x000F0000`。
- 他 app の `load_addr` は app ごとに決める。
- stage3 と overlap しないように、原則 `0x00200000` 未満に置く。

shared service tail 配置:

- 固定予約サイズは持たない。
- stage1 が DRAM size に応じて必要量を DRAM 末尾から下向きに割り当てる。
- 最低限、service code, service table, boot context, shared heap を割り当てる。
- stack の初期値は shared service tail の直前にする。
- stack は shared service tail より低いアドレスへ下向きに伸びる。
- shared heap は shared service tail 内に置く。
- shared heap は `heap_alloc` / `heap_free` / `heap_realloc` を提供する。
- shared heap は free-list allocator とする。block header は heap 内に置き、16 byte align を基本にする。
- blob 展開用 staging や scratch は固定予約しない。必要なときに shared heap から確保し、処理後に free する。
- shared heap は stage をまたいで生きる boot-only data と、一時的な staging/scratch の両方に使う。
- stage-local temporary は、小さいものは stack、大きいものは shared heap を使う。
- Linux/OS へ渡す ACPI table, boot params, command line などは shared heap に置かない。shared service 解放後も残す必要があるため、最終配置先へ別途コピーする。
- Linux 起動直前には、shared service table pointer を DRAM 最後 4 byte から消し、shared service 領域を不要にする。
- OS へ渡す E820 では、Linux 起動直前に解放できるなら shared service 領域を usable として扱ってよい。
- shared service table pointer を消した後の panic/debug serial は残さない。

## app

`app/` は stage3 から起動される機能モジュールとして扱う。app は blob ごとにロードアドレスを持つ。

app ABI:

- app blob format は上記 `app blob format` に従う。
- stage3 は `load_addr` へ app を展開して起動する。
- stage3 との overlap を避けるため、原則として app は `0x00200000` 未満に置く。
- `legacy` と `linux_loader` は `0x000F0000` でよい。

最初は stage3 と同じ ROM image に含めてもよいが、リンク単位と配置は分ける。将来的には app ごとに blob 化して、必要なものだけ展開する。

### legacy

伝統的 BIOS service を提供する。

配置:

- load address: `0x000F0000`
- real-mode thunk/INT handler stub はこの範囲に置く。
- 可能なら C の本体は 32-bit protected mode に戻して実行する。

- INT 10h serial console / minimal video
- INT 11h equipment
- INT 12h memory size
- INT 13h disk
- INT 15h E820/ACPI
- INT 16h keyboard
- INT 1Ah RTC/tick

### linux_loader

storage 上の Linux kernel/initrd をロードして起動する。

配置:

- load address: `0x000F0000`
- Linux boot protocol へ渡す低位構造体と衝突しないように、ロード完了後は legacy/Linux loader app slot を破棄可能にする。

- パーティションなしで先頭が Linux ELF の場合も読む。
- パーティション 0/1 選択は NVRAM の `partition` に従う。
- initrd は optional。
- command line は NVRAM の値と default flags から作る。

### selftest

ROM 上の test ELF blob をロードして実行する。

- CMOS/NVRAM の run-test bit で起動する。
- memtest bit とは分離する。
- S3 test は uACPI を使って namespace load/initialize と `_PTS(3)` などを実行する。

## boot_context

現状の `BOOT_AUX_*` dword 配列は廃止する。boot context は shared service table から pointer で辿る構造体にする。

最低限:

```
magic
version
size
total_dram_bytes
flags
platform_id
acpi_input_ptr
acpi_input_size
rsdp_linear
pci_io_base
pci_io_limit
pci_mem_base
pci_mem_limit
pci_prefetch_mem_base
pci_prefetch_mem_limit
```

flags:

- `shadow_ready`
- `maintenance_requested`
- `platform_qemu`
- `platform_p2b98_xv`

payload blob の場所は boot context ではなく、shared service table の `payload_manifest_ptr` から辿る。

`boot_context` と shared service table の ABI は C/asm 両方から使うので、offset を生成する仕組みにする。asm に直書きした定数は減らす。

## build output

最終的な成果物名は当面維持する。

- `src/start`: 実機用 256KiB ROM image
- `src/qemu_bios.bin`: QEMU 用 256KiB ROM image
- `src/stage2.bin`: P2B98-XV stage2
- `src/qemu_stage2.bin`: QEMU stage2
- `src/stage3.bin`: high DRAM stage3
- `src/legacy_app.bin`: legacy app
- `src/linux_loader_app.bin`: Linux loader app
- `src/test_elf_blob.bin`: selftest blob

内部名は整理後に変更してよいが、外部から使う `make -C src start qemu_bios.bin test` は壊さない。互換のため、移行中は `bios.bin` を `stage3.bin` の alias として残してよい。

## 移行手順

1. shared service table ABI を定義する。
2. shared service table offset を C/asm 用に生成する。
3. stage1 が DRAM 末尾へ PIC shared service と table を動的配置し、DRAM 最後 4 byte に table pointer を置く。
4. `BLOB_SERVICE_LINEAR` と `BOOT_AUX_*` の直接参照を shared service table 経由に置き換える。
5. stage 間の linker symbol 参照をなくす。blob pointer は shared service table 上の payload manifest で渡す。
6. stage3 のリンク先を `0x00200000` に移し、`0x000F0000` を legacy/Linux loader 用に空ける。
7. legacy BIOS service を `app/legacy/` に切り出し、`0x000F0000` に配置する。
8. Linux loader を `app/linux_loader/` に切り出し、`0x000F0000` に配置する。
9. `blob_service.c` を `shared_service/` に移す。
10. P2B98-XV/QEMU の stage1/stage2 を `platform/` 以下に移す。
11. `bios_main.c` から serial/pci/storage/nvram/maintenance を切り出す。
12. S3/uACPI selftest を `app/selftest/s3test/` に切り出す。
13. generator scripts を `scripts/build/`、QEMU test runner を `scripts/test/`、実機更新系を `scripts/board/` に移す。
14. それぞれの移動後に `make -C src test` を通す。

## 決定事項

- stage3 は `0x00200000` に置く。stage2 と重ならないようにする。
- app は blob ごとにロードアドレスを持つ。
- `legacy` と `linux_loader` は `0x000F0000` に置く。
- 他の app は app ごとにロードアドレスを決める。ただし stage3 と overlap しないように、原則 `0x00200000` 未満に置く。
- shared service tail は固定予約サイズにしない。DRAM size と必要量から stage1 が動的に末尾へ配置する。
- shared heap は free 可能にする。blob staging/scratch は固定予約せず、必要時に heap から取って処理後に返す。
- shared service table pointer と shared service 領域は Linux 起動直前に解放する。残す理由はない。
- uACPI は selftest だけで使う。stage3 には常駐させない。
- ACPI table は stage3 で完成させる。stage2 は board 固有入力を shared service table 経由で渡す。
- DOS 用 tools は `tools/dos/` へ移す。
- app blob は先頭 4 byte に `load_addr` を持ち、その後ろに既存 BLZ4 blob を置く。
- shared service table pointer を消した後の panic/debug serial は不要。
