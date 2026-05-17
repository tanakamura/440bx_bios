440BX で動くBIOSを作る

BIOS更新方法は docs/update_bios.md を参照

テスト方針と `make -C src test` の使い方は `TESTING.md` を参照する。BIOS service / thunk / DOS runtime を触ったら、まず `TESTING.md` に従って QEMU テストを回す。

update_bios.md にある手順をやると、f000:fff0 から命令が開始する。

specs/dsdt.dsl がこのマザボのdsdt
ハードウェア仕様や、作業中に確定したチップセット/マザーボード依存の知識は `specs/` 以下に書く。
Intel 82371AB/EB (PIIX4/PIIX4E) の仕様メモは `specs/piix4e.md` に、Intel 440BX の仕様メモは `specs/440bx.md` に蓄積する。毎回 web を引き直さず、一度確認したことはそこへ追記して再利用する。
S3 suspend / resume を触るときは `specs/s3_suspend.md` を参照し、DRAM 内容を壊す cold boot path と resume path の混同を避ける。

POST CODE の定義は `src/post_code.def` を唯一の正とし、`specs/POST_CODE.md` と C/asm 用ヘッダはそこから同期する。
必要になったら `src/post_code.def` に追加する。カテゴリ分けも codex が自分で好きなようにしていい。
人間は必要になったら `src/post_code.def` を参照するので、書いてある即値に意味を持たせる必要はない。単なるインデクスとして運用していい。
不要になった POST code は `src/post_code.def` から削除する。

DRAM 初期化前は RAM を使えない。
したがって初期コードではスタック使用禁止。`call` `ret` `push` `pop` を使わず、RAM への読み書きもしてはいけない。
