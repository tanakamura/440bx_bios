# refactor

stage とディレクトリを整理する。移動だけで済むものは先に機械的に移し、ABI/配置変更は手順を分けて行う。

## 現状との矛盾点

- `stage2` のロード先は現状 `0x00080000`。旧メモの `0x00100000` は誤り。
- `stage3` のロード先は `0x00200000`。`0x000F0000-0x000FFFFF` は `app/legacy` や `app/linux_loader` を置く低位 app slot として使う。
- `stage3` は高位 DRAM 上の実行基盤と platform-independent glue に絞る。legacy BIOS service / Linux loader / selftest は app payload へ分離済みで、stage3 には app 起動と fallback 用の最小 thunk だけを残す。
- `shared_service` は全 stage に静的リンクするものではなく、stage1 が DRAM 末尾に設置し、DRAM 最後 4 byte の table pointer から辿って呼ぶ runtime service。現状は blob loader と shared heap がこれに該当する。
- ACPI table は board 依存入力を持つ。P2B98-XV は `specs/dsdt.dsl` 由来、QEMU は fw_cfg 由来にする。table 構築と platform 固有 PM I/O 設定は stage2 に置き、stage3 は boot context に渡された port 値で Linux 起動直前の PM event clear / SCI enable だけ行う。
- `srcipts/` typo は使わない。スクリプトは repo root の `scripts/` に集約する。

## 方針

- stage1/stage2 は board ごとに分ける。
- 各 stage は次 stage へ移った後に破壊される前提で作る。
- stage3 は board 非依存の BIOS runtime とし、`0x000F0000` には置かない。
- `0x000F0000-0x000FFFFF` は legacy/Linux loader 用 app slot として使う。他の app は app blob 側でロードアドレスを決める。
- stage 間でシンボルを相互参照しない。共通データと関数は shared service table 経由で受け渡す。
- board 固有情報は stage2 が shared service table 上の boot context に詰めて stage3 に渡す。
- stage3 entry と stage3 runtime flow は分割し、entry は `stage3/entry.c` に置く。
- blob format / POST code / memory map / shared service table は C と asm で定数を重複させない。
- QEMU と実機で同じ stage3 を使う。違いは stage1/stage2 と shared service table の内容だけに閉じ込める。
- uACPI は自作 AML interpreter の代わりに使う。ただし stage3 には常駐させず、selftest blob 側だけで使う。

## 現在の主要ディレクトリ

```
src/
  include/
    blob.h
    legacy_app_abi.h
    legacy_rm.h
    linux_loader_abi.h
    post_code.h

  platform/
    p2b98_xv/
      stage1/
        start.asm
        stage1.ld
        bootblock.c
      stage2/
        stage2.c
        stage2.ld
        l2_service.c
        l2_service.h
    qemu/
      stage1/
        qemu_stage1.c
        qemu_start.asm
        stage1.ld
      stage2/
        stage2.c
        stage2.ld

  shared_service/
    blob_service.c
    service_table.h

  stage3/
    entry.c
    stage3.ld
    stage3.c
    bios_*.c

  app/
    legacy/
      legacy.ld
      entry.c
      legacy_*.c
    linux_loader/
      linux_loader.ld
      entry.c
      linux_loader.c
    selftest/
      s3test/
        s3test.c
        s3test_uacpi.c
        uacpi_config.h
        s3test.ld

  lib/
    rm_thunk/
      bios16.asm
    x86/
      bios_io.h
      bios_memory.c
      bios_nvram.c
      bios_rtc.c
    serial/
      bios_serial.c
      bios_serial.h
    pci/
      bios_pci.c
      bios_pci.h
    acpi/
      acpi_tables.c
      acpi_tables.h
    storage/
      bios_storage.c
      bios_storage.h
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
    flashutil_io.asm
    mem_dmp.c
    srecsave.asm
    xrecv.c
    biostest.c
    flattest.c
    shutdown.c
```

## build matrix

ROM image は motherboard と app profile の組み合わせで作る。

motherboard:

- `qemu`
- `p2b98_xv`

app profile:

- `legacy`
- `selftest`
- `linux`

したがって作る BIOS binary は 6 種類。

```
qemu      / legacy
qemu      / selftest
qemu      / linux
p2b98_xv  / legacy
p2b98_xv  / selftest
p2b98_xv  / linux
```

stage2 が使う DSDT などの board 固有 ACPI 入力はここでは別扱いにする。ROM payload area に入れる app 用 blob は以下。

| motherboard | app      | app 用 payload |
|-------------|----------|----------------|
| qemu        | legacy   | `legacy_app`。必要なら ROM free area に `test_floppy` |
| p2b98_xv    | legacy   | `legacy_app`。必要なら ROM free area に `test_floppy` |
| qemu        | selftest | `test_elf` |
| p2b98_xv    | selftest | `test_elf` |
| qemu        | linux    | `linux_loader`, `vgabios` |
| p2b98_xv    | linux    | `linux_loader`, `vgabios` |

`test_floppy` は legacy test 用の差し替え可能 payload。通常の legacy ROM には不要。

移行中の状態:

- legacy profile は `legacy_app` を payload として link する。必要な test media だけ ROM free area へ後差しする。
- real-mode thunk asm は共通 `lib/rm_thunk/bios16.asm` に置く。legacy service の C 側は `app/legacy/` へ移動済みで、現状は `legacy_floppy`, BDA 初期化, INT 10h/11h/12h/13h/15h/16h/17h/1Ah/60h が legacy 側 module になっている。
- `bios_rm_service` dispatcher と thunk/IVT/DPT 設置は `app/legacy/` へ移動済み。legacy genrom profile では `legacy_app` payload を `0x000F0000` にロードして entry を呼ぶ。stage3 に残す legacy 直リンクは Linux 起動前の IVT/thunk 設置に必要な最小 thunk 部分だけに縮小済み。
- E820/memory map は `bios_memory.*`、RTC は `bios_rtc.*` へ分離済み。legacy app は必要な provider を app 側へ link して直接呼ぶ。
- selftest profile は `test_elf` payload を ROM に入れる。これは app slot へ直接入る app payload ではなく、stage3 の test runner が ELF として読み込む payload。
- stage3 main flow は `stage3/stage3.*` へ分離済み。`stage3/entry.c` は stage2 からロード先先頭に jump される entry と stage3 run wrapper だけを持つ。linker script も `stage3/stage3.ld` へ移動済み。
- stage3 は `0x00200000` にロードされる。app slot の `0x000F0000` とは分離済み。
- C は `-ffunction-sections -fdata-sections`、stage3 link は `--gc-sections` を使う。stage3 entry はロード先先頭に残す必要があるため `.entry` に固定して `KEEP()` する。
- stage3 専用 glue のうち context/settings/maintenance/memtest/selftest/benchmark/ACPI runtime/legacy/Linux/shadow は `src/stage3/` へ移動済み。
- low-level helper のうち ACPI table、x86 I/O/memory/RTC/NVRAM、PCI、serial、storage は `src/lib/` へ移動済み。ファイル名/API は移行中のため一旦 `bios_*` のまま。

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
- 丸ごとコピーで再配置可能な shared service code を DRAM 末尾へコピーする。
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
- ACPI table expose。table 自体は stage2 が完成させ、stage3 は RSDP と PM port 情報を boot context から受け取る。
- maintenance mode。
- boot priority に従って Linux loader / legacy boot / selftest を起動。

stage3 自体には board 固有 register 値を直接持たせない。必要な host bridge window, ACPI 入力, blob pointer は shared service table 経由で受け取る。

## shared service

shared service は stage 間 ABI の唯一の窓口にする。全 stage と app は、共通データと共通関数を shared service table 経由で使う。

配置:

- shared service 関数は ELF の `-fPIC` ではなく、`.blobsvc*` 内の PC-relative 参照だけで構成し、同一 code block を丸ごとコピーすれば動く形にする。i386 `-fPIC` は GOT/EBX 依存を持ち込みやすいので使わない。
- build 時に `scripts/build/check_blobsvc_reloc.py` で `.blobsvc*` 内 relocation を検査し、GOT/data/rodata などへの参照が混ざったら stage1 link 前に失敗させる。
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

`blob_load` は、payload id と fallback destination を受け取り、payload manifest から blob を探して block CRC と retry を shared service 内で処理する。stage2/stage3/app は LZ4 や CRC の実装を直接持たない。stage2 以降は ROM linker symbol を直接参照せず、shared service table の `payload_manifest_ptr` から payload を探す。

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

`id` は `stage2`, `stage3`, `legacy_app`, `linux_loader_app`, `selftest_app`, `vgabios`, `dsdt`, `test_floppy` など。`blob_ptr` は BLZ4 blob か app blob を指す。ロードアドレスを持つ blob は blob header の `load_addr` を使う。

`slot_size` は ROM 上でその payload に予約した最大サイズ。テスト時は `blob_size <= slot_size` の範囲で payload を書き換えてよい。BLZ4 内に CRC があるので、directory 側に payload CRC は持たせない。directory 自体には header checksum を持たせる。

## build system

Makefile は手書き header 依存をやめ、compiler generated dependency を使う。

- C compile は `-MMD -MP` で `.d` を生成し、top-level `src/Makefile` が module object list から `-include` する。
- asm 依存も `nasm -MD -MT -MP` で `.d` を生成する。生成 `.inc` は初回生成のため order-only prerequisite に残すが、include 依存の追跡は `.d` に寄せる。
- top-level `src/Makefile` は orchestration だけに寄せる。stage/app ごとの object list、linker script、link rule は各 directory の Makefile に持たせる。
- stage/app の build artifact は各 directory 配下または共通 `build/<target>/...` に出し、root `src/` 直下の object 増殖を止める。
- 外部互換の target 名 `make -C src start qemu_bios.bin test` は維持する。

stage ごとの単位:

- `platform/p2b98_xv/stage1/Makefile` は `stage1.elf` を作る。linker script も同 directory に置く。
- `platform/p2b98_xv/stage2/Makefile` は `stage2.elf` を作る。
- `platform/qemu/stage1/Makefile` は `stage1.elf` を作る。
- `platform/qemu/stage2/Makefile` は `stage2.elf` を作る。
- `stage3/Makefile` は `stage3.elf` を作る。
- `app/legacy/Makefile`, `app/linux_loader/Makefile`, `app/selftest/.../Makefile` はそれぞれ独立 ELF を作る。

各 ELF は自分の linker script で load address / entry / section を決める。ROM へ詰める処理は linker script ではなく `gen_rom.py` に寄せる。

## ROM generation

ROM image は blob list から `scripts/build/gen_rom.py` で作る。

blob list は 1 行 1 payload とする。

```
<payload type>,<filename>
```

例:

```
stage2,platform/p2b98_xv/stage2/stage2.elf
stage3,stage3/stage3.elf
vgabios,../images/gbsmc.088
stage1,platform/p2b98_xv/stage1/stage1.elf
```

`gen_rom.py` の責務:

- blob list を読み、payload directory を作って ROM 先頭側に入れる。
- blob list に書かれた payload を ROM payload area に詰める。
- path が ELF なら `objcopy -O binary` 相当で loadable binary だけ取り出す。
- ELF の load address は program header から取得し、blob header の `load_addr` に入れる。
- raw binary で load address が不要な payload は `HAS_LOAD_ADDR` を立てない。
- `stage1` だけは special payload として ROM 末尾に置く。reset vector を含むため、通常 payload area に詰めない。
- `stage1` 以外は payload directory に登録する。stage1 が起動後に directory を読み、shared service table 上の payload manifest へコピーする。
- payload directory は 32bit word sum が 0 になる checksum を持つ。stage1 は manifest 作成前に directory header と entry 配列の checksum を検証する。
- ROM 末尾 8 byte の `rom_free_first`, `rom_free_end` descriptor は `gen_rom.py` が最終 ROM 配置から埋める。
- `gen_rom_deps.py` は blob list から ROM target の `.d` を生成する。profile ごとの make 依存は、実際に blob list に書かれた payload だけにする。

blob list は build matrix ごとに持つ。

- `platform/qemu/legacy.blobs`
- `platform/qemu/selftest.blobs`
- `platform/qemu/linux.blobs`
- `platform/p2b98_xv/legacy.blobs`
- `platform/p2b98_xv/selftest.blobs`
- `platform/p2b98_xv/linux.blobs`

top-level target は board/profile を選んで対応する blob list を `gen_rom.py` に渡す。これにより「どの ROM に何を入れるか」を linker script から切り離す。

### legacy test media

legacy BIOS service のテスト用に `test_floppy` payload を予約する。

- ROM 末尾 `0xfffffff8-0xffffffff` の 8 byte に `rom_free_first`, `rom_free_end` を置く。
- `rom_free_first/end` は CPU から見える top-alias linear address で持つ。
- `test_floppy` は `rom_free_first` に差し込む。
- `test_floppy` は `FDS0` sparse floppy format とする。先頭 magic が `FDS0` なら有効。
- stage3/legacy は `rom_free_first` の `FDS0` magic を見つけたら、INT 13h floppy image として起動する。
- floppy image は RAM に全展開しない。INT 13h read のたびに `FDS0` run table を見て ROM から sector 単位で読む。run にない sector は zero-fill。
- test runner や手動差し替えツールは ROM 末尾 descriptor を読んで、`blob_size <= rom_free_end - rom_free_first` の範囲で `test_floppy` だけを書き換える。
- 小さい regression floppy は通常 ROM の空き slot に入れる。
- `vgabios` や `selftest_app` は legacy app に含まれているわけではない。同じ ROM payload area に並ぶ別 payload。
- FreeDOS など大きい floppy image を使う場合は、legacy ROM に `test_floppy` slot を大きく取り、そこへ差し替える。

## blob format

BLZ4 blob header に flags と load address を持たせる。

```
flags:
  BLOB_FLAG_LZ4_BLOCKS
  BLOB_FLAG_HAS_LOAD_ADDR

header:
  magic
  header_size
  version
  flags
  load_addr
  uncompressed_size
  compressed_size
  block_size
  block_count
  block_table_off
  data_off
  uncompressed_crc32
  compressed_crc32
```

CRC と block retry は既存の block 機構を使う。`BLOB_FLAG_HAS_LOAD_ADDR` が立っている blob は `load_addr` に展開する。立っていない blob は caller が destination を指定する。

原則:

- `stage2`, `stage3`, `legacy`, `linux_loader`, `selftest` は `BLOB_FLAG_HAS_LOAD_ADDR` を持つ。
- `vgabios`, `dsdt`, `test_floppy` は原則 `BLOB_FLAG_HAS_LOAD_ADDR` を持たない。展開先は stage/app 側が決める。
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

- app blob format は上記 `blob format` の `BLOB_FLAG_HAS_LOAD_ADDR` に従う。
- stage3 は `load_addr` へ app を展開して起動する。
- stage3 との overlap を避けるため、原則として app は `0x00200000` 未満に置く。
- `legacy` と `linux_loader` は `0x000F0000` でよい。
- stage3 から app slot へ payload を読む共通処理は `stage3/bios_app_loader.*` に置く。slot の C 定数は `include/blob.h` の `APP_SLOT_LOAD_LINEAR/APP_SLOT_LOAD_CAPACITY` を使う。

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

legacy app 切り出し方針:

- 標準 BIOS との比較は media transport ではなく INT 13h service level で合わせる。標準 BIOS は `-fda testfd.img`、自作 BIOS は `FDS0 in ROM free area` でよい。
- `FDS0` は custom BIOS に test floppy を渡す transport であり、test floppy の中身は BIOS INT 13h/10h だけを使う。test の期待値は stage3 の log ではなく boot sector が出す `SQ` などの guest-visible 結果に寄せる。
- real-mode thunk asm は共通 `lib/rm_thunk/bios16.asm` に置く。legacy app は `bios_rm_service` と full INT service を持ち、stage3 は legacy app をロードして entry を呼ぶ。Linux/VBIOS/legacy fallback 用に stage3 は同じ thunk object の direct section だけを使う。
- storage は `lib/storage` 相当として共有し、legacy app から直接使う。難所は storage driver ではなく、現在同一 ELF 内に残る BDA 初期化、INT 10h/11h/12h/13h/15h/16h/1Ah、tick、keyboard、boot drive、`0x7c00` への real-mode jump を legacy app 側へ独立 link/load すること。
- `bios16.asm` の thunk は runtime では `0x000FE000` にコピーして使う。ELF 内の `bios16_thunk_start` はコピー元であり、runtime base とは分けて考える。
- legacy app は `0x000F0000` に配置する。したがって先に stage3 を `0x00200000` へ移して `0x000F0000-0x000FFFFF` を app slot として空ける。

移行中の残依存:

- `lib/rm_thunk/bios16.asm` から呼ぶ `bios_rm_service` は、legacy app では `app/legacy/legacy_service.c`、stage3 では `stage3/bios_legacy.c` がそれぞれ定義する。legacy app は独立 ELF として link し、serial/storage/RTC/E820 provider も app 側で持つ。
- thunk/IVT/DPT 設置は `legacy_thunk.*` に移動済み。legacy app runtime は shadow install callback だけ stage3 から受け取る。
- stage3 は Linux/VBIOS/legacy fallback 用に共通 `lib/rm_thunk/bios16.o` をまだ直接 link する。stage3 側の direct thunk install は `bios_direct_thunk.*` に分離済み。`bios16.asm` の legacy INT stubs は `.thunk16_legacy` section へ分け、stage3 linker は direct/VBE/boot に必要な `.thunk16` だけを入れ、`.thunk16_legacy` は discard する。
- INT19 boot sector 選択は `legacy_boot.*` に移動済み。ただし FreeDOS へ落ちる protected-mode-to-real-mode jump は `bios16.asm` の `bios_boot_freedos_pm32` symbol を参照している。
- INT13 HDD path は legacy app 側の `lib/storage` scan を直接使う。stage3 から HDD state は渡さない。
- INT15 E820 は legacy app 側の `bios_memory.*` を直接使う。stage3 から E820 callback は渡さない。
- RTC read/write は `bios_rtc.*`、INT 1Ah 本体は `legacy_time.*`、PIT/tick counter は `legacy_timer.*` へ分離済み。
- shadow PAM/MTRR/GDT setup と VBIOS shadow/init は `bios_shadow.*` へ分離済み。`stage3/stage3.c` は stage2 の shadow-ready flag と payload/blob service を渡すだけ。

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

旧 aux dword 配列は廃止済み。boot context は shared service table から pointer で辿る構造体にする。

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
acpi_pm1_evt
acpi_pm1_cnt
acpi_gpe0
acpi_gpe0_len
acpi_flags
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
- `src/start_legacy`: 実機用 legacy profile 256KiB ROM image
- `src/qemu_legacy_bios.bin`: QEMU 用 legacy profile 256KiB ROM image
- `src/platform/p2b98_xv/stage1/stage1.elf`: P2B98-XV stage1
- `src/platform/p2b98_xv/stage2/stage2.elf`: P2B98-XV stage2
- `src/platform/qemu/stage1/stage1.elf`: QEMU stage1
- `src/platform/qemu/stage2/stage2.elf`: QEMU stage2
- `src/stage3/stage3.elf`: high DRAM stage3
- `src/app/legacy/legacy_app.elf`: legacy app
- `src/app/linux_loader/linux_loader_app.elf`: Linux loader app
- `src/test_floppy_blob.bin`: legacy test 用差し替え payload

内部名は整理後に変更してよいが、外部から使う `make -C src start qemu_bios.bin test` は壊さない。

## 移行手順

1. shared service table ABI を定義する。
2. shared service table offset を C/asm 用に生成する。
3. stage1 が DRAM 末尾へ PIC shared service と table を動的配置し、DRAM 最後 4 byte に table pointer を置く。
4. `BLOB_SERVICE_LINEAR` と旧 aux dword 配列の直接参照を shared service table 経由に置き換える。
5. stage 間の linker symbol 参照をなくす。blob pointer は shared service table 上の payload manifest で渡す。
6. stage3 のリンク先を `0x00200000` に移し、`0x000F0000` を legacy/Linux loader 用に空ける。
7. legacy BIOS service を `app/legacy/` に切り出し、`0x000F0000` に配置する。
8. Linux loader を `app/linux_loader/` に切り出し、`0x000F0000` に配置する。
9. `blob_service.c` を `shared_service/` に移す。
10. P2B98-XV/QEMU の stage1/stage2 を `platform/` 以下に移す。
11. `stage3/stage3.c` から serial/pci/storage/nvram/maintenance glue を切り出す。
12. S3/uACPI selftest を `app/selftest/s3test/` に切り出す。
13. generator scripts を `scripts/build/`、QEMU test runner を `scripts/test/`、実機更新系を `scripts/board/` に移す。
14. それぞれの移動後に `make -C src test` を通す。

## 現在の移行状態

- P2B98-XV stage1/stage2 と QEMU stage1/stage2 は `src/platform/` へ移動済み。P2B98-XV stage1 と QEMU stage1 は DRAM 末尾に shared service code / shared service table / boot context / payload manifest を置き、DRAM 最後 4 byte の pointer から辿れる。stack 初期値は shared service code の直前に置く。
- shared service code と table ABI は `src/shared_service/` へ移動済み。
- stage2 は stage3 payload を shared service table の `blob_load` から読む。payload pointer の旧 aux fallback は削除済み。
- stage2 -> stage3 の entry ABI は `total_dram_bytes` のみを渡す。旧 `aux_blob_linear` は使わない。
- shared service table の `blob_load` は実装済み。stage1/stage2 は次 stage payload の検索、header `load_addr` 適用、展開 staging の選択を `blob_load` に委譲する。stage1/stage2 の旧 linker-symbol/direct `blob_expand` fallback は削除済み。
- QEMU stage1 の shadow + stage2 load helper も payload manifest と shared service table の `blob_load` だけを使う。旧 blob pointer / staging pointer fallback は削除済み。
- P2B98-XV stage2 の DSDT 展開も `blob_load` 経由へ移行済み。
- shared tail は 16KiB に広げ、shared heap は tail 内の free-list allocator として初期化済み。`heap_alloc` / `heap_free` / `heap_realloc` は shared service table 経由で呼べる。
- `blob_load` は blob 展開用 staging を shared heap から一時確保し、展開後に free する。旧 `blob_stage` 固定予約は使わない。
- blob 展開中の maintenance key は blob service が DRAM 末尾の shared service table pointer から boot context を辿り、`SHARED_BOOT_FLAG_MAINTENANCE_REQUESTED` を直接立てる。旧 aux dword 配列は削除済み。
- stage3 は VGA BIOS / test ELF payload を `blob_load` で読む。旧 aux fallback、固定 `BLOB_SERVICE_LINEAR` fallback、table 上の直接 `blob_expand` entry は削除済み。
- ACPI table 構築は stage2 へ移動済み。P2B98-XV stage2 は DSDT blob を展開して RSDT/FADT/FACS/RSDP を作る。QEMU stage2 は fw_cfg の ACPI tables を取得/patch して RSDP を作る。stage3 は board 非依存の ACPI PM event clear / SCI enable だけを持つ。
- stage2 は ACPI table の実配置範囲を boot context の `acpi_table_base/acpi_table_size` に記録する。これを使えば、後続で top reserved 1MiB のうち ACPI table 以外を E820 usable に戻せる。
- legacy BIOS service の dispatcher / thunk / timer / runtime glue は `app/legacy/` へ移動済み。legacy genrom profile は `legacy_app` を ROM payload に入れ、stage3 が `0x000F0000` へロードして entry を呼ぶ。
- legacy service は serial/storage/RTC/E820 provider を app 側に直接 link する。`legacy_app_exports` で boot sector 選択 / boot drive 書き込み / PM stack 設定も app 側関数を呼ぶ。stage3 直リンクから legacy service 本体と `legacy_thunk.o` は外し、Linux profile が使う low thunk/VBE 呼び出し用に共通 `lib/rm_thunk/bios16.o` だけを残している。
- stage3 側へ移した direct thunk installer は legacy app から削除済み。legacy app の `legacy_thunk.*` は full BIOS INT thunk installation だけを持つ。
- legacy app entry ABI は `include/legacy_app_abi.h`、real-mode service frame は `include/legacy_rm.h` へ切り出し済み。stage3 は legacy app 内部の `legacy_runtime.h` / `legacy_rm.h` を include しない。
- floppy test image probe/state は legacy runtime 側へ移動済み。stage3 は floppy の有無を保持せず、legacy app が自分で BDA/INT13 用 state を作る。
- Linux kernel/initrd loader と Linux boot params/VBE setup は `app/linux_loader/` へ移動済み。serial/storage/E820 は `linux_loader_config` callback 経由になり、stage3 は NVRAM 設定と ACPI/RTC/VBIOS/storage/memory callback を渡す glue だけ持つ。
- Linux loader entry ABI は `include/linux_loader_abi.h` へ切り出し済み。stage3 は `app/linux_loader/linux_loader.h` を include しない。
- 通常 boot path では `linux_loader` payload がある profile だけ Linux boot を試す。payload が無い legacy profile では direct fallback せず legacy boot へ進む。
- NVRAM raw access と設定 decode/save は `bios_nvram.*` へ分離済み。`stage3/stage3.c` には stage3 global へ反映する薄い glue だけ残っている。
- maintenance prompt は `bios_maintenance.*` へ分離済み。`stage3/stage3.c` は NVRAM 設定ポインタと save callback を渡すだけ。
- optional memtest とその一時 MTRR UC 化は `bios_memtest.*` へ分離済み。`stage3/stage3.c` は enable flag / DRAM size / shared service table を渡すだけ。
- memtest は shared service code / blob staging / shared service table page を skip する。shared table 内の boot context, payload manifest, heap metadata を壊さない。
- Linux 起動直前の ACPI PM event clear / SCI enable は `bios_acpi_runtime.*` へ分離済み。stage3 は stage2 由来の RSDP/PM port/flag を渡すだけ。
- manual bandwidth benchmark は `bios_benchmark.*` へ分離済み。stage3 main flow からは fallback diagnostic として呼ぶだけ。
- shared service table / boot context / payload manifest の stage3 decode は `bios_stage_context.*` へ分離済み。`stage3/stage3.c` は `struct bios_stage_context` を保持して各 module へ渡すだけ。
- NVRAM 設定 state / maintenance prompt glue / boot priority learn は `bios_settings.*` へ分離済み。`stage3/stage3.c` は `struct bios_settings` を各 app config に渡すだけ。
- legacy platform ops / runtime config / boot drive glue は `bios_legacy.*` へ分離済み。`stage3/stage3.c` は boot priority と callbacks を渡して legacy app を呼び出すだけ。
- Linux loader config / platform callback glue は `bios_linux.*` へ分離済み。`stage3/stage3.c` は stage/settings/VBE callback を渡して Linux loader を呼び出すだけ。
- Linux 起動直前に Linux loader callback から DRAM 末尾の shared service table pointer を消す。Linux boot path の E820 は stage2 が記録した `acpi_table_base/acpi_table_size` だけを reserved にし、ACPI table より上の旧 shared service/scratch 領域は usable として返す。legacy INT15 E820 は互換性優先で従来の top reserved 1MiB を維持する。
- Linux loader は `runtime_protect_base/size` を受け取り、kernel `PT_LOAD` が stage3 runtime window と重なる場合は高位 DRAM へ一旦読み込む。storage/serial/ACPI/VBE/NVRAM callbacks が終わった直後、Linux entry へ飛ぶ直前に app 側 code だけで本来の物理アドレスへコピーする。これで kernel load 中に stage3 callback 本体を上書きしない。
- ROM test ELF 起動 glue は `bios_selftest.*` へ分離済み。`stage3/stage3.c` は run-test bit を見て selftest config を渡すだけ。
- DOS tool / DOS test helper の source は `tools/dos/` へ移動済み。build output は互換のため引き続き `src/*.exe` / `src/*.com` に出す。
- QEMU regression 用 boot sector / Linux probe source は `src/tests/boot/` へ移動済み。生成物名は互換のため引き続き `src/*.img` / `src/linuxprobe.elf` に出す。
- ボード用の単発 ROM smoke test source は `src/tests/board/` へ移動済み。生成物名 `src/romtest` は維持する。
- build generator scripts は `scripts/build/`、QEMU test runner は `scripts/test/` へ移動済み。`make -C src` から呼ぶ前提で、生成物の基準 directory は引き続き `src/`。
- S3/uACPI selftest source は `src/app/selftest/s3test/` へ移動済み。selftest ROM には `src/app/selftest/s3test/s3test.elf` を full ELF のまま `test_elf` payload として入れる。
- stage3 の実体出力は `stage3/stage3.elf` へ移行済み。互換用に `bios.elf` alias だけを残し、旧 linker-symbol ROM 用の `stage3_blob.bin` / `bios_blob.bin` / `bios_blob.o` 経路は削除済み。
- BLZ4 blob header は `load_addr` と `BLOB_FLAG_HAS_LOAD_ADDR` を持つ。stage2 / stage3 blob 生成時に load address を埋め、stage1/stage2 は header の load address を優先して展開する。互換のため load address がない blob は caller 指定 destination へ展開する。
- `scripts/build/gen_rom.py` は追加済み。blob list から payload directory 付き ROM を生成できる。stage1 は ROM 先頭の payload directory を読んで manifest を作れる。`make -C src genrom` で board/profile 6 種の gen_rom 版 ROM を生成できる。既存 `start` / `qemu_bios.bin` target は linux profile の genrom 版をコピーする。`start_legacy` / `qemu_legacy_bios.bin` は legacy profile の genrom 版をコピーする。
- board/profile ごとの blob list は `platform/qemu/*.blobs` と `platform/p2b98_xv/*.blobs` に追加済み。`genrom` target はこの blob list を入力にする。
- `.blobsvc` は ROM payload area から stage1 tail 側へ移動済み。payload area は stage2/stage3/app/blob 用に寄せ、stage1 が必要とする shared service code は stage1 image の一部として持つ。
- gen_rom 用に stage directory 配下の ELF alias を作る target を追加済み。stage1 ELF は payload symbol なしでも link できるようにし、`gen_rom.py` は stage1 ELF の alloc section だけを ROM 末尾へ overlay して payload directory を壊さない。
- ROM payload directory checksum は実装済み。`gen_rom.py` が header/entry の 32bit word sum を 0 にし、stage1 の manifest reader が検証する。
- `make -C src test` は自作 BIOS の通常 boot path を genrom ROM で確認する。legacy boot/USB MBR/floppy は `qemu_legacy_genrom.bin`、Linux probe は `qemu_linux_genrom.bin` を使う。
- stage3、P2B98-XV/QEMU stage2、P2B98-XV/QEMU stage1-only ELF の link rule と object list は各 stage directory の `Makefile` へ切り出し済み。現状は top-level `src/Makefile` から include する非再帰 make。通常 ROM target は genrom 版をコピーし、旧 linker-symbol ROM の `start.elf` / `qemu_start.elf` 生成 rule は削除済み。
- legacy と linux_loader の object list / compile rule は各 app directory の `Makefile` へ切り出し済み。`app/legacy/legacy_app.elf` と `app/linux_loader/linux_loader_app.elf` は独立 app ELF として作れる。genrom profile ではそれぞれ ROM payload としてロードできる。stage3 は full `linux_loader.o` を直リンクせず、selftest 用の最小 ELF loader/boot_params builder と、Linux/VBE/boot jump 用の共通 `lib/rm_thunk/bios16.o` だけを直接持つ。
- `app/linux_loader/linux_loader_app.elf` の単体 build target は追加済み。Linux profile の genrom では ROM blob list に入り、stage3 は payload があれば `0x000F0000` へロードして app entry を呼ぶ。payload がない profile では Linux boot を試さない。
- `app/legacy/legacy_app.elf` の単体 build target は追加済み。legacy genrom profile の ROM blob list に入り、stage3 は payload があれば `0x000F0000` へロードして app entry を呼ぶ。boot sector 選択などの後続操作は `legacy_app_exports` 経由で app 側関数を呼ぶ。payload がない profile では Linux 用 direct thunk だけを設置する。
- legacy app は serial/storage/RTC/E820 provider を app 側に直接 link し、`legacy_platform_ops` を自前で構成する。stage3 からの platform callback table と `hdd_present` は `legacy_runtime_config` から削除済み。
- selftest/uACPI の object list / compile rule は `app/selftest/s3test/Makefile` へ切り出し済み。旧 linker-symbol ROM 用の `test_elf_blob.o` と旧 `test_elf_blob.bin` target は削除済み。
- genrom の `test_elf` payload は ELF の PT_LOAD だけを抜かず、ELF file 全体を BLZ4 化する。stage3 の test runner が ELF header を見て `0x00180000` へロードする。
- stage3 固有 `.c`、platform stage 固有 source、lib/shared helper の compile rule は各 directory の `Makefile` へ切り出し済み。top-level の `CORE_C_OBJS` は module 変数の合成になっている。旧 `start` / `qemu_bios.bin` ROM build rule は platform stage1 `Makefile` へ、genrom board/profile rule は platform board `Makefile` へ移動済み。
- regression boot image / smoke test rule は `src/tests/Makefile`、DOS tool rule は `tools/dos/Makefile` へ切り出し済み。top-level `src/Makefile` は module include と共通 target に寄せる。
- P2B98-XV/QEMU stage2 と stage3 は root 直下の中間 ELF/map を作らず、各 stage directory の ELF/map へ直接 link する。
- legacy/linux_loader/selftest app ELF/map も root 直下ではなく各 app directory へ直接 link する。
- stage/app/lib/shared/platform の C/asm object と generated dependency / stack-usage file は、それぞれの source directory 配下へ出す。root `src/` 直下に残る旧 artifact は互換 target や過去 build 由来の生成物だけに寄せる。
- board/profile genrom target の依存は `scripts/build/gen_rom_deps.py` で blob list から生成する。不要な profile が VGA BIOS など未使用 payload に依存する状態は解消済み。
- 通常の同名 `.c` -> `.o` compile は top-level の共通 pattern rule に寄せる。module Makefile は object list/link rule と、asm/uACPI/別名 object などの例外だけを持つ。
- `blob.h` は `src/include/` へ移動済み。root 直下の `bios_pci.h` forwarding header も削除し、PCI header は `lib/pci/` の実体を include path から解決する。
- `shared_service/service_table.inc` は `shared_service/service_table.h` から、`include/blob.inc` は `include/blob.h` から生成する。QEMU stage1 asm はこれらを include し、shared table size / stage2 load address / blob status size の C/asm 二重定義を避ける。
- ROM high alias delta も `shared_service/service_table.h` から `service_table.inc` へ生成し、stage1 C/asm の重複即値を避ける。
- asm object / boot sector / board smoke test は `nasm -MD` で dependency file を生成する。`post_code.inc`, `include/blob.inc`, `shared_service/service_table.inc` は初回生成用の order-only prerequisite にしている。

## 決定事項

- stage3 は `0x00200000` に置く。stage2 と重ならないようにする。
- app は blob ごとにロードアドレスを持つ。
- `legacy` と `linux_loader` は `0x000F0000` に置く。
- 他の app は app ごとにロードアドレスを決める。ただし stage3 と overlap しないように、原則 `0x00200000` 未満に置く。
- shared service tail は固定予約サイズにしない。DRAM size と必要量から stage1 が動的に末尾へ配置する。
- shared heap は free 可能にする。blob staging/scratch は固定予約せず、必要時に heap から取って処理後に返す。
- shared service table pointer と shared service 領域は Linux 起動直前に解放する。残す理由はない。
- uACPI は selftest だけで使う。stage3 には常駐させない。
- ACPI table は stage2 で完成させる。stage3 は boot context の `rsdp_linear` と ACPI PM port 情報だけを使う。
- DOS 用 tools は `tools/dos/` へ移す。
- load address は app blob 先頭ではなく BLZ4 blob header の `load_addr` に持つ。`BLOB_FLAG_HAS_LOAD_ADDR` が立っている場合だけ有効。
- shared service table pointer を消した後の panic/debug serial は不要。
