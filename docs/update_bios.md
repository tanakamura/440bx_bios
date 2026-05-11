`alarm.local:8080/rom` へ PUT リクエストを投げると、その内容が ROM に書かれる。

ROM の read back は `GET /rom` でできる。

## Upload

アップロードは `scripts/upload_rom.py` を使う。
この script は次をまとめて行う。

- `PUT /rom`
- 応答の `bytes=262144` 確認
- `GET /rom`
- read back と元ファイルの比較

```bash
python3 scripts/upload_rom.py file
```

read back を保存したいなら:

```bash
python3 scripts/upload_rom.py file --readback readback.bin
```

シリアル出力を取り逃がしたくないときは `scripts/run_with_serial_capture.py` を使う。
この script は必要なら `ttyS0` logger を起動し、upload の前から記録して、upload 後に増えた分だけを表示する。

```bash
python3 scripts/run_with_serial_capture.py file
```

## Reset

ROM 更新後のリセットは自動でかかる。
人間への通知や確認は不要。
