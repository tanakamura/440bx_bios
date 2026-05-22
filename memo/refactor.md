# refactor

このメモは、完了済みの移行履歴は書かず、いま残っている作業だけを書く。

## 方針

- app 依存コードは各 `app/` に置く
- 全 app 共通コードは `lib/` に置く
- `stage3` は app 選択と app 起動順制御だけを持つ
- app 実行中は `stage3` の SDRAM を参照しない
- app に渡す永続データは shared heap 上に置く

## 残作業

### 1. `app_boot_context` の ownership をさらに明確にする

現状:

- `ctx` は shared heap に置く前提
- `legacy` / `linux_loader` / `selftest` は同じ `app_entry(ctx)` で起動する
- `selftest` は ROM free の executable blob を読むだけの薄い layer に整理済み

やること:

- `ctx` を誰が allocate し、誰が埋め、どこまで immutable かを明文化する
- `runtime_base/runtime_size`
  `boot_params_linear`
  `work_linear/work_size`
  の ownership を app ごとに整理する

ゴール:

- app 実行中に有効なデータと scratch の境界が明確
- selftest executable blob の load/work 領域もこの ownership に含めて整理する

## 今はやらない

- 完了済みの移行履歴をこのメモへ戻さない
- すでに削除済みの旧 ABI 名
  - `legacy_runtime_config`
  - `linux_loader_config`
  - `selftest_runtime_info`
  - `legacy_app_exports`
  はここでは追わない
