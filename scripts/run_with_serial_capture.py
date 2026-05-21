#!/usr/bin/env python3

import argparse
import ctypes
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


def is_our_logger(pid: int, logger_script: str, log_path: str) -> bool:
    try:
        cmdline = Path(f"/proc/{pid}/cmdline").read_bytes().split(b"\0")
    except OSError:
        return False

    args = [arg.decode(errors="replace") for arg in cmdline if arg]
    if not args:
        return False
    return logger_script in args and "--log" in args and log_path in args


def wait_pid_dead(pid: int, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            os.kill(pid, 0)
        except ProcessLookupError:
            return True
        except PermissionError:
            return False
        time.sleep(0.05)
    return False


def cleanup_pid_file(pid_path: Path, pid: int):
    try:
        if pid_path.read_text().strip() == str(pid):
            pid_path.unlink()
    except FileNotFoundError:
        pass


def kill_stale_logger(pid_path: Path, logger_script: str, log_path: str):
    try:
        pid_text = pid_path.read_text().strip()
    except FileNotFoundError:
        return

    if not pid_text:
        pid_path.unlink(missing_ok=True)
        return

    try:
        pid = int(pid_text)
    except ValueError:
        pid_path.unlink(missing_ok=True)
        return

    try:
        os.kill(pid, 0)
    except ProcessLookupError:
        pid_path.unlink(missing_ok=True)
        return

    if not is_our_logger(pid, logger_script, log_path):
        raise RuntimeError(
            f"{pid_path} points to non-matching live process pid={pid}"
        )

    os.kill(pid, signal.SIGTERM)
    if not wait_pid_dead(pid, 2.0):
        os.kill(pid, signal.SIGKILL)
        wait_pid_dead(pid, 1.0)
    cleanup_pid_file(pid_path, pid)


def set_parent_death_signal():
    libc = ctypes.CDLL(None)
    pr_set_pdeathsig = 1
    libc.prctl(pr_set_pdeathsig, signal.SIGTERM, 0, 0, 0)
    if os.getppid() == 1:
        os._exit(1)


def start_logger(logger_script: str, log_path: str) -> subprocess.Popen:
    log_file = Path(log_path)
    pid_path = Path(f"{log_path}.pid")
    kill_stale_logger(pid_path, logger_script, log_path)
    log_file.parent.mkdir(parents=True, exist_ok=True)
    proc = subprocess.Popen(
        [sys.executable, logger_script, "--log", log_path],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        preexec_fn=set_parent_death_signal,
    )
    pid_path.write_text(str(proc.pid))
    time.sleep(0.2)
    return proc


def stop_logger(proc: subprocess.Popen, log_path: str):
    pid_path = Path(f"{log_path}.pid")
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait(timeout=1.0)
    cleanup_pid_file(pid_path, proc.pid)


def raise_on_termination(signum, _frame):
    raise KeyboardInterrupt(f"signal {signum}")


def main() -> int:
    signal.signal(signal.SIGTERM, raise_on_termination)
    args = parse_args()
    proc = start_logger(args.logger_script, args.log)
    try:
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
    finally:
        stop_logger(proc, args.log)


if __name__ == "__main__":
    raise SystemExit(main())
