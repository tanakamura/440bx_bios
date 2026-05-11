#!/usr/bin/env python3

import argparse
import os
import signal
import subprocess
import sys
import time
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Upload a ROM while capturing newly appended ttyS0 output."
    )
    parser.add_argument("image", help="ROM image path")
    parser.add_argument("--log", default="/tmp/ttyS0.log", help="serial log path")
    parser.add_argument(
        "--logger-script",
        default="scripts/serial_logger.py",
        help="path to serial logger script",
    )
    parser.add_argument(
        "--wait-seconds",
        default=3.0,
        type=float,
        help="seconds to wait after upload before reading the log",
    )
    parser.add_argument(
        "--readback",
        default="/tmp/rom.readback.bin",
        help="readback path passed to upload_rom.py",
    )
    parser.add_argument(
        "--upload-script",
        default="scripts/upload_rom.py",
        help="path to upload helper script",
    )
    return parser.parse_args()


def ensure_logger(logger_script: str, log_path: str) -> subprocess.Popen | None:
    log_file = Path(log_path)
    pid_path = Path(f"{log_path}.pid")
    try:
        pid_text = pid_path.read_text().strip()
        if pid_text:
            os.kill(int(pid_text), 0)
            return None
    except Exception:
        pass
    log_file.parent.mkdir(parents=True, exist_ok=True)
    proc = subprocess.Popen(
        [sys.executable, logger_script, "--log", log_path],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
    )
    pid_path.write_text(str(proc.pid))
    time.sleep(0.2)
    return proc


def main() -> int:
    args = parse_args()
    ensure_logger(args.logger_script, args.log)
    log_path = Path(args.log)
    start_size = log_path.stat().st_size if log_path.exists() else 0

    subprocess.run(
        [
            sys.executable,
            args.upload_script,
            args.image,
            "--readback",
            args.readback,
        ],
        check=True,
    )

    time.sleep(args.wait_seconds)
    if not log_path.exists():
        print("no serial log", file=sys.stderr)
        return 1

    with log_path.open("rb") as f:
        f.seek(start_size)
        new_data = f.read()
    sys.stdout.buffer.write(new_data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
