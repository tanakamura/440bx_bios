#!/usr/bin/env python3

import argparse
import socket
import time
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(
        description="Continuously log ttyS0 broadcaster output to a file."
    )
    parser.add_argument(
        "--socket",
        default="/tmp/ttyS0_bcast.sock",
        help="AF_UNIX broadcaster socket path",
    )
    parser.add_argument("--log", default="/tmp/ttyS0.log", help="log file path")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    Path(args.log).parent.mkdir(parents=True, exist_ok=True)
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(args.socket)
        with open(args.log, "ab", buffering=0) as logf:
            while True:
                data = sock.recv(4096)
                if not data:
                    time.sleep(0.01)
                    continue
                logf.write(data)


if __name__ == "__main__":
    raise SystemExit(main())
