# S3 Suspend / Resume Plan

このメモは P2B98-XV / 440BX / PIIX4E で Linux の `mem` suspend
を動かすために必要な作業を整理する。S3 は通常起動とは違い、
DRAM 内容を壊すと復帰できないので、実装前にここを読む。

## Current State

- 現在の `specs/dsdt.dsl` には `\_S0`, `\_S1`, `\_S5` はあるが
  `\_S3` はない。
- 現在の real-board FADT は BIOS が生成している。PM I/O は
  `PM1_EVT=0xe400`, `PM1_CNT=0xe404`, `PM_TMR=0xe408`,
  `GPE0=0xe40c`, `SCI_INT=9` としている。
- 現在の BIOS は SMI handler / SMM を持っていない。したがって
  FADT の `SMI_CMD` / `ACPI_ENABLE` による ACPI mode transition に
  依存してはいけない。
- BIOS は Linux handoff 前に PIIX4E PM I/O decode を有効化し、
  `PM1_CNT.SCI_EN` を立て、PM1/GPE の既存 status を clear している。
- FACS は作っているが、S3 resume 時に FACS の Firmware Waking
  Vector を読む resume path はまだない。

## Strategy

- 最短経路は AML/FADT を BIOS 側で生成または patch して、OS に
  native ACPI として S3 を実行させること。
- AML interpreter / namespace evaluator は自作しない。ROM test blob には
  uACPI をリンクし、BIOS が作った ACPI table を uACPI で読ませて
  `_PTS`, `_WAK`, sleep-state helper などを検証する。
- SMM は作ってよい。ただし S3 の最小実装には必須ではない。
  まず SMM なしで `PM1_CNT.SCI_EN` を BIOS が直接立て、
  OS が `PM1_CNT.SLP_TYP + SLP_EN` を書いて suspend できる形を狙う。
- SMM は次の場合に使う:
  - PIIX4E/board が S3 entry 直前に SMI 経由の board-specific 処理を
    要求する場合。
  - `SMI_CMD` / `ACPI_ENABLE` に反応する互換動作が必要な場合。
  - AML から SystemIO を叩くだけでは fan / LED / wake source / GPE の
    制御が足りない場合。
- AML は書き換えてよい。元 DSDT をそのまま信用するより、BIOS が
  現在実際に初期化している PM base / GPE / IRQ routing に合わせて
  `_S3`, `_PTS`, `_WAK`, `_PRW` を生成または patch する。

## Blockers

- S3 sleep type が未確認。既存 DSDT に `_S3` がないので、PIIX4E /
  P2B98-XV が S3 を実際に許すかを確認する必要がある。
- DRAM self-refresh / standby power の扱いが未確認。S3 では DRAM
  内容が保持される必要がある。
- Wake 後の reset vector から cold boot と S3 resume を判定する
  path がない。
- S3 resume path がないため、現状では wake 後に通常 bootblock が
  DRAM init / memtest / loader 処理を走らせ、OS メモリを壊す。

## ACPI Table Requirements

- DSDT に `_S3` を追加する。ただし sleep type の値は推測で入れない。
  PIIX4E datasheet、元 BIOS の AML、または実機実験で S3 用
  `SLP_TYP` を確認してから入れる。
- `_S3` は ACPI standard の sleep package として、少なくとも
  `Package (0x04) { SLP_TYPa, SLP_TYPb, 0, 0 }` の形にする。
- 既存の `\_PTS` と `\_WAK` は S3 でも呼ばれる前提で確認する。
  現在の AML は fan / power LED / debug port `0x80` を触る。
- Wake source を使うなら `_PRW` と `\_GPE._Lxx` を実機に合わせる。
  現在の DSDT には USB, UART, PCI wake 用らしい `_PRW` / `_L08`
  / `_L09` / `_L0A` がある。
- FADT は正しい FACS physical address を指す必要がある。
- FACS は OS が Firmware Waking Vector を書ける writable RAM に置く。
  現在の top-reserved DRAM 領域を使うのが安全。
- FADT checksum, RSDT checksum, DSDT checksum を必ず更新する。
- SMM がない限り FADT の `SMI_CMD` は `0` のままにする。
- SMM を実装した場合だけ、FADT の `SMI_CMD`, `ACPI_ENABLE`,
  `ACPI_DISABLE` を有効値にしてよい。その場合でも Linux handoff 前に
  SCI/PM/GPE の状態を dump して、OS が ACPI mode に入ったことを確認する。

## PIIX4E / PM Setup Requirements

- PIIX4E PM function `00:07.3` の PM I/O base decode を有効化する。
  現在は `0xe400` base を PCI config `0x40` に書き、config `0x80`
  の PM enable 系 bit と PCI command I/O bit を立てている。
- FADT の `SCI_INT` と実際の SCI routing を一致させる。現在は
  `SCI_INT=9` なので、IRQ9 が level-triggered / active-low として
  OS から見える必要がある。
- Linux handoff 前に `PM1_STS`, `PM1_EN`, `GPE_STS`, `GPE_EN` を
  既知状態にする。現在の `acpi_clear_pm_events()` は初期 clear 用。
- S3 entry は OS が行う。OS は `_PTS(3)` を実行し、FACS waking
  vector を書き、`PM1_CNT` に S3 の `SLP_TYP` と `SLP_EN` を書く。
  BIOS 側で通常起動中に S3 へ落とす必要はない。
- Wake source を有効にする場合、対象 GPE enable bit を OS/AML が
  制御できる必要がある。SMM handler がないので SMI 経由の wake
  処理は使わない。

## Optional SMM Path

- SMM を作る場合、まず S3 とは独立に「SMI が入って handler から戻る」
  だけを確認する。
- SMRAM base/size/open/close/lock の手順を `specs/piix4e.md` か
  専用メモに記録する。未確認のまま SMRAM lock しない。
- SMI handler は最初は最小にする:
  - POST code / serial で entry を確認する。
  - SMI source status を読む。
  - 必要な status を clear する。
  - RSM で戻る。
- SMM handler 内で通常 BIOS の C runtime / stack / global state を
  共有しない。SMM 用の専用 stack と小さい hand-written entry を使う。
- SMM を使うなら FADT の `SMI_CMD=0xb2`, `ACPI_ENABLE=0xa1`,
  `ACPI_DISABLE=0xa0` のような元 BIOS 互換値を復活させる候補がある。
  ただし実際の値は現 DSDT/FADT と PIIX4E config で確認する。
- SMM は resume path の代替ではない。S3 wake 後は結局 reset vector
  から入り、FACS Firmware Waking Vector へ戻す firmware resume path
  が必要。

## Resume Path Requirements

- Reset vector から入った直後に cold boot / S3 resume を判定する。
  候補は PM1 status の `WAK_STS` と、top-reserved DRAM に置く
  BIOS resume mailbox の magic/checksum。
- S3 resume と判定したら通常の DRAM init をしてはいけない。
  DRAM controller 設定は保持されている前提で、OS メモリを壊さない。
- S3 resume path では full memtest を絶対に走らせない。
- Stage1/Stage2 の blob 展開先、scratch、stack は OS 使用 RAM を
  壊さない場所に限定する。top-reserved DRAM だけを使う。
- 最小限の chipset 状態だけ復元する。A20, GDT, protected-mode
  entry, cache/MTRR, PAM shadow, serial debug は BIOS 実行に必要。
- PCI BAR 再割当、storage scan、USB enumeration、Linux loader は
  resume path では実行しない。OS が自分で device resume する。
- FACS の Firmware Waking Vector を読み、ACPI の waking-vector
  calling convention に従う小さい assembly trampoline で OS へ戻る。
  C 関数呼び出し扱いにしない。
- WAK_STS などの wake status clear は OS handoff 直前または OS
  要求に合わせて行う。早すぎる clear は resume 判定を壊す。

## Resume Mailbox

- top-reserved DRAM に BIOS resume mailbox を置く。
- mailbox には magic, version, checksum, detected DRAM size,
  FACS address, BIOS runtime entry, saved MTRR/PAM assumptions を入れる。
- cold boot 時に mailbox を作る。
- S3 resume 時は mailbox checksum を検証してから使う。
- mailbox 領域は E820 reserved に含め、Linux に通常 RAM として
  渡さない。

## Implementation Order

1. BIOS maintenance mode に S3/PM selftest command を追加する。
   Linux 起動を毎回使わず、BIOS だけで PM register dump, fake FACS,
   resume trampoline, S3 entry を確認できるようにする。
2. PM1/GPE register dump を追加し、BIOS maintenance から `PM1_CNT`,
   `PM1_STS`, `GPE_STS`, `GPE_EN` を読めるようにする。Linux 起動後の
   dump は最後の整合性確認だけに使う。
3. FACS を top-reserved DRAM に固定配置し、FADT がその address を
   指すようにする。
4. cold boot 時に resume mailbox を作る。
5. reset vector 直後に S3 resume 判定だけを追加し、判定結果を POST
   code / serial に出す。まだ OS へ戻らない。
6. AML patch/generation path を作り、S3 sleep type を確認して DSDT に
   `_S3` を追加する。
7. Linux で `cat /sys/power/state` に `mem` が出ることを確認する。
8. 必要ならここで SMM 最小 handler を追加し、SMI entry/return だけを
   単体確認する。
9. resume trampoline を追加し、FACS Firmware Waking Vector へ戻す。
10. BIOS maintenance から S3 entry を直接実行し、wake 後に resume
    判定と fake waking vector への復帰を確認する。
11. `echo mem > /sys/power/state` で suspend し、power button / keyboard
   / configured wake source で復帰を試す。
12. 復帰後に `dmesg`, clocksource, IDE/USB, network, filesystem を確認する。

## BIOS-Only Test Harness

Linux 起動を毎回使うと遅く、手順も不安定になる。S3 実装中は
maintenance mode から直接テストできる BIOS-only harness を優先する。
さらに、CMOS `flags0` bit3 の run test blob bit が 1 の場合は、
serial 入力なしで ROM 内蔵 ELF test blob を自動起動する。BIOS は
ACPI table を作り、Linux handoff 直前と同じ PM/ACPI/boot params
状態を作ってから ELF entry を呼ぶ。test blob bit は実行前に BIOS が clear
する one-shot trigger として扱う。
この test blob は uACPI を使い、現時点では RSDP から table/namespace を
load / initialize して `\_PTS(3)` を実行する。

### Maintenance Commands

候補 command:

- `M> pm` : PM/ACPI register dump
- `M> facs` : FACS address, firmware waking vector, mailbox dump
- `M> wtest` : fake FACS waking vector に戻る trampoline 単体テスト
- `M> rtest` : resume 判定だけを forced path で実行する
- `M> s3 <type>` : 指定した `SLP_TYP` で S3 entry を試す

`s3 <type>` は危険なので、S3 sleep type が確認できるまで実装しても
hidden command 扱いにする。`_S0/_S1/_S5` の値から未確認の `_S3`
を推測して自動試行しない。

### PM Register Dump

`M> pm` は少なくとも以下を出す:

- `PM1_STS` at `0xe400`
- `PM1_EN` at `0xe402`
- `PM1_CNT` at `0xe404`
- `PM_TMR` at `0xe408`
- `GPE_STS` at `0xe40c`
- `GPE_EN` at `0xe40e`
- PIIX4E PM PCI config `00:07.3 0x40`, `0x80`, command register

これにより Linux 起動なしで PM decode / SCI_EN / stale status を確認する。

### Trampoline Test

`M> wtest` は実 suspend しない。

1. top-reserved DRAM に fake FACS を作る。
2. fake Firmware Waking Vector に小さい test stub を置く。
3. test stub は serial に `WAKETEST` を出して maintenance に戻る。
4. resume trampoline を呼び、FACS vector へ戻れることを確認する。

これで ACPI table や Linux なしに、最も危険な OS handoff 部分を
単体確認できる。

### Resume Detection Test

`M> rtest` は実 suspend しない。

1. resume mailbox に magic/checksum を書く。
2. forced resume flag を立てる。
3. reset-vector 相当の early path を呼ぶか、次回 reset で判定だけ行う。
4. `S3 resume detected` を出し、DRAM init / memtest / storage scan へ
   進まないことを確認する。

この段階では FACS vector へ戻らない。cold boot path と resume path の
分岐だけを見る。

### Direct S3 Entry Test

`M> s3 <type>` は Linux を使わずに実 suspend する最終前テスト。

1. PM/GPE status を clear する。
2. wake source を最小構成にする。最初は power button を優先する。
3. resume mailbox と fake FACS waking vector を準備する。
4. `PM1_CNT` に `SLP_TYP=<type>` と `SLP_EN` を書く。
5. wake 後、BIOS が S3 resume と判定し、fake waking vector へ戻って
   `WAKETEST` を出すことを確認する。

このテストが通るまで Linux の `echo mem` は使わない。

## Linux Debug Commands

Linux は ACPI table の OS 解釈と最終 resume 確認だけに使う。

```sh
dmesg | grep -iE 'acpi|suspend|resume|wakeup|gpe|sci'
cat /sys/power/state
cat /proc/interrupts | grep -E 'acpi| 9:| 11:'
grep -R . /proc/acpi/wakeup 2>/dev/null
echo mem > /sys/power/state
```

## Do Not Do

- `_S3` の `SLP_TYP` を未確認のまま適当に入れない。
- S3 resume path で DRAM init を再実行しない。
- S3 resume path で `0x00100000` 以降の OS RAM を scratch として使わない。
- S3 resume path で PCI resource assignment や storage scan をしない。
- SMI handler がない状態で SMI delivery を有効にしない。
- SMM を作ったとしても、S3 resume path を SMM だけで済ませようとしない。
