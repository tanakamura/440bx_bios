# APP Boot Interface

この文書は、`legacy` / `linux_loader` / `selftest` の 3 app を
同じ考え方で起動できるようにするための ABI / API 方針を定義する。

目的は次の 3 点。

1. `stage3` から app 依存コードをできるだけ消す
2. app 固有の glue は各 `app/` ディレクトリに置く
3. 全 app 共通のコードだけを `lib/` に置く
4. app 起動後は `stage3` の SDRAM 領域は解放済みとみなし、参照しない
5. VBIOS shadow / init は app ごとの責務にする

## 現状の問題

今は次の不揃いがあったが、整理を進めている。

- `legacy`
  - 以前は `legacy_runtime_config + exports` という双方向 ABI だった
  - いまは `app_entry(ctx)` へ移行済みで、stage3 main からは独立した
- `linux_loader`
  - `linux_loader_config` を受ける一方向 ABI
  - app 本体は self-contained だが、起動 glue は別
- `selftest`
  - `selftest_runtime_info` を受ける別 ABI
  - 以前は中で `legacy_stage3_*` を呼んでいた

つまり今は、

- 共通情報の運び方はかなり揃ってきた
- しかし entry ABI と lifecycle が app ごとに揃っていない
- 主に `legacy` の旧 callback/export 名残をどう消すかが論点だった

## 原則

今後の原則はこれにする。

### 1. app の entry ABI は全 app 共通

全 app は同じ entry prototype を持つ。

```c
struct app_boot_context;

typedef int (*app_entry_fn)(const struct app_boot_context* ctx);
```

`legacy` / `linux_loader` / `selftest` すべてこれで起動する。

返り値:

- `0`: この app は boot 完了または成功終了
- `>0`: この app は「次に進まない」成功終了
  - 例: `selftest` が結果表示後に halt
- `<0`: この app では boot できなかったので caller が fallback してよい

実際の値は enum 化してもよいが、まずは上の意味だけ揃えればよい。

### 2. app への入力は 1 個の context にまとめる

app に渡す情報は、app ごとに別 struct を増やさず、
共通の `app_boot_context` にまとめる。

重要:

- `app_boot_context` 自体は app の実行中ずっと有効な領域に置く
- その領域は `stage3` のコード/データ/スタックとは別であること
- app は `stage3` の `.data/.bss/stack` や `struct bios_stage_context` への
  ポインタを保持してはならない
- app 起動後は `stage3` の SDRAM は解放されている前提にする
- ただし `shared service` の heap / 永続領域に確保したオブジェクトへの
  ポインタは app に渡してよい

最低限必要なのは次。

```c
struct app_boot_context {
    unsigned int abi_magic;
    unsigned int abi_version;
    unsigned int app_id;

    struct app_platform_info platform;

    unsigned int runtime_base;
    unsigned int runtime_size;

    unsigned int boot_params_linear;
    unsigned int work_linear;
    unsigned int work_size;
};
```

意味:

- `platform`
  - ACPI / NVRAM snapshot / total RAM など全 app 共通入力
- `runtime_base`, `runtime_size`
  - app 本体や loader 自身が上書きしてはいけない保護領域
  - いまの `linux_loader.runtime_protect_*` はここへ統合する
- `boot_params_linear`
  - Linux boot params や selftest boot params など、
    「OS 互換の handoff buffer」を作る先
- `work_linear`, `work_size`
  - 一時作業領域
  - ELF 展開、boot sector staging、selftest image 展開に使う

注意:

- これは最小の共通断面であって、最終形の全フィールド一覧ではない
- `legacy` / `linux_loader` が app 自前で VBIOS shadow / init や payload load を
  行うなら、`shared service` や VBIOS blob 位置などの runtime data を `ctx` に追加する
- ただし追加してよいのは「永続な runtime data」だけであり、
  `stage3` ローカル state への生ポインタを渡してはならない

この `ctx` は `stage3` のローカル変数上に置いてはならない。
原則として `shared service` の heap / 永続領域に置く。

### 3. stage3 は app の中身を知らない

`stage3` の責務はこれだけに絞る。

1. `bios_stage_context` を読む
2. `bios_settings` を読む
3. 共通 `app_boot_context` を 1 個、`shared service` heap 上の永続領域に組み立てる
4. どの payload を起動するか決める
5. `app_entry_fn(ctx)` を呼ぶ
6. return code で次動作を決める

`stage3` は app ごとの struct layout や callback exports を持たない。
また app 実行中に `stage3` ローカル state を参照させない。

## レイヤ分け

### `stage3/` に置いてよいもの

- stage3 自身の main loop
- POST / maintenance / memtest / settings load
- payload 選択
- app 共通 context 構築
- app return code の解釈
- app に必要な runtime data のコピー

置いてはいけないもの:

- app が後から参照する前提の mutable state
- `struct bios_stage_context*` や `struct bios_settings*` を app にそのまま渡すこと
- app 実行継続中に必要な scratch/work buffer を `stage3` ローカルに置くこと
- app に渡すポインタを `shared service` 以外の一時 SDRAM 上へ置くこと

### `lib/` に置くもの

全 app 共通なら `lib/` に置く。

例:

- `lib/app/`
  - payload load
  - `app_boot_context` 構築 helper
  - 共通 scratch / work area helper
  - 共通 app return code 定義
- `lib/acpi/`
  - ACPI runtime install helper
- `lib/x86/`, `lib/pci/`, `lib/storage/`, `lib/serial/`
  - 既存どおり

### `app/<name>/` に置くもの

その app にしか要らないものは `app/<name>/` に置く。

例:

- `app/legacy/`
  - real-mode service 実装
  - direct thunk
  - boot sector 読み込み
  - floppy / HDD boot policy
  - 必要なら VBIOS shadow / init
- `app/linux_loader/`
  - storage scan 後の Linux image 探索
  - ELF load
  - Linux boot params 構築
  - VBIOS shadow / init
- `app/selftest/`
  - selftest image load
  - uACPI test 実行
  - selftest 専用 boot params 構築
  - VBIOS は不要なら持たない

## 共通 library

関数ポインタの service table は使わない。

理由:

- `serial` / `storage` / `memory` / `rtc` / `pci` などは static lib として
  app に直接 link すればよい
- それらは app 起動後もアドレスが変わらないので、
  `stage3` から関数ポインタを引き継ぐ必要がない
- 関数ポインタ table を作ると ABI が無駄に肥大化し、
  version 管理も複雑になる

したがって、`stage3` から app に引き継ぐべきものは
「コード」ではなく「データ」だけにする。

## stage3 から引き継ぐべき情報

`stage3` から app へ渡すべき情報は、static lib では再構築できない
runtime data に限る。

具体的には次。

- `platform`
  - total RAM
  - ACPI RSDP / PM base / table placement
  - NVRAM snapshot
- `runtime_base`, `runtime_size`
  - app 本体や blob 展開で壊してはいけない領域
- `boot_params_linear`
  - Linux / selftest が handoff buffer を置く先
- `work_linear`, `work_size`
  - app が自由に使える一時作業領域
- app 固有の永続 runtime data
  - 例: `shared service` 参照、VBIOS blob 位置、shadow 済みフラグ

逆に、次は引き継がない。

- serial 出力関数
- storage scan / HDD read 関数
- memory map helper 関数
- RTC read/write 関数
- boot success 記録関数

これらはすべて app が static lib から直接呼ぶ。

boot success 記録のような「設定更新」は、
NVRAM write lib を app から直接呼べばよい。

## app ごとの位置づけ

### legacy

`legacy` は特殊扱いに見えるが、entry ABI 自体は共通にする。

```c
int legacy_app_entry(const struct app_boot_context* ctx);
```

`legacy` は entry 内で次を行う。

1. 自分の runtime を初期化
2. 自分の direct thunk / RM service を install
3. 自分で boot sector を選ぶ
4. 必要なら NVRAM lib 経由で boot success を記録する
5. `bios_boot_freedos_pm32()` 相当へ遷移

`legacy_app_exports` は削除済み。

### linux_loader

`linux_loader` はすでにかなり理想に近い。

必要なのは、

- `linux_loader_config` をやめて `app_boot_context` に統合
- `runtime_protect_*` を `ctx` 共通フィールドへ移す
- VBIOS shadow / init は `linux_loader` 側で実行する

### selftest

`selftest` も `selftest_runtime_info` をやめて `app_boot_context` に統合する。

ただし selftest が Linux 互換 boot params を見るなら、

- `ctx->boot_params_linear`

を読む。

つまり selftest 専用 ABI は不要。
また selftest は VGA text / VBE を使わないので、VBIOS shadow / init を前提にしない。

## 推奨 ABI 断面

最終形はこれを目標にする。

### 共通 header

- `src/include/app_boot_abi.h`
  - `struct app_boot_context`
  - `enum app_id`
  - `enum app_result`

この header に出てくる型はすべて、
`stage3` private 型に依存してはならない。
`bios_stage_context` や `bios_settings` は出してはいけない。

### app entry

- `legacy`: `int app_entry(const struct app_boot_context* ctx);`
- `linux_loader`: `int app_entry(const struct app_boot_context* ctx);`
- `selftest`: `int app_entry(const struct app_boot_context* ctx);`

payload 側から見える symbol 名は統一する。
たとえば全 app とも `app_entry` に揃える。

## stage3 API

`stage3` から app を呼ぶ helper も app ごとの差を減らす。

理想形:

```c
int app_boot_run(unsigned int payload_id,
                 unsigned int app_id,
                 const struct app_boot_context* ctx,
                 const char* label);
```

これは `lib/app/` に置く。

これがやること:

1. payload を `APP_SLOT_LOAD_LINEAR` へ load
2. `app_entry(ctx)` を call
3. result を返す

`stage3` は payload id と起動順だけ決める。

`app_boot_run()` の実装も、
app に `stage3` ポインタを渡してはならない。
必要な情報は call 前に `app_boot_context` へコピーし切る。

## すぐやるべき整理

優先順はこれ。

1. `linux_loader_config` / `selftest_runtime_info` を
   `app_boot_context` へ統合する
2. `lib/app/` に `app_boot_abi.h` と `app_boot_run()` を作る
3. `stage3` は
   - context 構築
   - app 実行順制御
   だけにする
4. app 実行に必要な `ctx` / work 領域を
   `shared service` heap 上へ置く

## いまの設計に対する判断

今の配置は前進しているが、まだ最終形ではない。

良くなった点:

- app 固有コードが `stage3` からかなり減った
- 共通 helper が `lib/app` へ出始めた

まだ変な点:

- `legacy` だけ exports ベースの別 ABI
- `linux_loader` と `selftest` も専用 config struct を持っている
- app entry 名も形も揃っていない
- 一部 glue がまだ `stage3` 所有 state を前提にしている
- `ctx` / work 領域の最終配置先が ABI としてまだ固定されていない

したがって、次の正しい方向は

- 「各 app 依存コードは各 app に置く」
- 「共通コードは lib に置く」

に加えて、

- 「entry ABI そのものも 1 個に揃える」

である。
