`alarm.local:8080/rom` へ PUT リクエストを投げると、その内容が ROM に書かれる。

ROM の read back は `GET /rom` でできる。

## Power / Serial

実機の電源を人間が入れているあいだは、できるだけ `ttyS0` broadcaster を立てておく。

- socket path は `/tmp/ttyS0_bcast.sock`
- この socket が存在するかどうかを、実機電源 On/Off の目安として使う
- Codex が実機の電源投入を必要とするときは、その旨を明示して人間に依頼する

例:

```bash
python3 scripts/tty_bcast.py --path /tmp/ttyS0_bcast.sock
```

client 側は:

```bash
socat - /tmp/ttyS0_bcast.sock
```

## Upload

アップロードは `scripts/upload_rom.py` を使う。
この script は次をまとめて行う。

- `PUT /rom`
- 応答の `bytes=262144` 確認
- `GET /rom`
- read back と元ファイルの比較

通常の実機作業では、ROM emu 起因の stage1 起動失敗を BIOS バグと混同しないため、
まず reset を assert したまま upload する。

```bash
python3 scripts/upload_rom.py file --hold-reset
```

read back を保存したいなら:

```bash
python3 scripts/upload_rom.py file --hold-reset --readback readback.bin
```

そのあと `scripts/reset_wrapper.py` で release/reset を繰り返し、
serial に `start stage1.5 @ ` が見えるまで最大 10 回やり直す。

```bash
python3 scripts/reset_wrapper.py
```

wrapper は `/tmp/ttyS0_bcast.sock` が立っている前提で動く。
socket が無いときは、実機電源が入っていない扱いとする。

```bash
python3 scripts/tty_bcast.py --path /tmp/ttyS0_bcast.sock
```

単純に upload と readback だけしたいときは、従来どおり `--hold-reset` なしでもよい。

```bash
python3 scripts/upload_rom.py file
```

シリアル出力を取り逃がしたくないときは `scripts/run_with_serial_capture.py` を使う。
この script は broadcaster (`/tmp/ttyS0_bcast.sock`) に接続する logger を起動し、
upload の前から記録して、upload 後に増えた分だけを表示する。

したがって、これを使うときは通常 `/tmp/ttyS0_bcast.sock` が立っている前提とする。

```bash
python3 scripts/run_with_serial_capture.py file
```

## Reset

`--hold-reset` を付けない upload では、ROM 更新後に自動でリセットがかかる。
ただし実機検証では、基本的に `--hold-reset` と `scripts/reset_wrapper.py` を使う。
