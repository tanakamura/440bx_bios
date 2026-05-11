#!/usr/bin/env python3

import argparse
import os
import termios
import time
from pathlib import Path


def configure_serial(fd: int, baud: int):
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    attrs[4] = termios.B115200 if baud == 115200 else attrs[4]
    attrs[5] = termios.B115200 if baud == 115200 else attrs[5]
    attrs[6][termios.VMIN] = 1
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def parse_args():
    parser = argparse.ArgumentParser(description="Continuously log ttyS0 output to a file.")
    parser.add_argument("--device", default="/dev/ttyS0", help="serial device path")
    parser.add_argument("--baud", default=115200, type=int, help="baud rate")
    parser.add_argument("--log", default="/tmp/ttyS0.log", help="log file path")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    Path(args.log).parent.mkdir(parents=True, exist_ok=True)
    fd = os.open(args.device, os.O_RDONLY | os.O_NOCTTY)
    try:
        configure_serial(fd, args.baud)
        with open(args.log, "ab", buffering=0) as logf:
            while True:
                data = os.read(fd, 4096)
                if not data:
                    time.sleep(0.01)
                    continue
                logf.write(data)
    finally:
        os.close(fd)


if __name__ == "__main__":
    raise SystemExit(main())
