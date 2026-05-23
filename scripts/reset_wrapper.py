#!/usr/bin/env python3

import argparse
import http.client
import selectors
import socket
import sys
import time
from pathlib import Path

DEFAULT_HOST = "alarm.local"
DEFAULT_PORT = 8080
DEFAULT_SOCKET = "/tmp/ttyS0_bcast.sock"
DEFAULT_MARKER = "start stage1.5 @ "


def http_reset(host: str, port: int, action: str) -> None:
    conn = http.client.HTTPConnection(host, port, timeout=5)
    try:
        conn.request("PUT", f"/reset?action={action}", body=b"")
        resp = conn.getresponse()
        body = resp.read().decode("utf-8", errors="replace")
        if resp.status != 200:
            raise RuntimeError(
                f"reset {action} failed: status={resp.status} body={body!r}"
            )
    finally:
        conn.close()


def drain_socket(sock: socket.socket) -> None:
    while True:
        try:
            if not sock.recv(4096):
                return
        except BlockingIOError:
            return


def wait_for_marker(sock: socket.socket, marker: bytes, timeout_seconds: float) -> bool:
    sel = selectors.DefaultSelector()
    buf = bytearray()
    deadline = time.monotonic() + timeout_seconds
    try:
        sel.register(sock, selectors.EVENT_READ)
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            events = sel.select(timeout=max(0.0, remaining))
            if not events:
                continue
            try:
                data = sock.recv(4096)
            except BlockingIOError:
                continue
            if not data:
                return False
            sys.stdout.buffer.write(data)
            sys.stdout.buffer.flush()
            buf.extend(data)
            if marker in buf:
                return True
            if len(buf) > 65536:
                del buf[: -len(marker)]
        return False
    finally:
        sel.close()


def parse_args():
    parser = argparse.ArgumentParser(
        description="Release/reset ROM emu until stage1.5 banner appears."
    )
    parser.add_argument("--host", default=DEFAULT_HOST, help="HTTP updater host")
    parser.add_argument(
        "--port", default=DEFAULT_PORT, type=int, help="HTTP updater port"
    )
    parser.add_argument(
        "--socket",
        default=DEFAULT_SOCKET,
        help="tty broadcaster AF_UNIX socket path",
    )
    parser.add_argument(
        "--marker",
        default=DEFAULT_MARKER,
        help="serial marker that indicates stage1.5 started",
    )
    parser.add_argument(
        "--attempts", default=10, type=int, help="maximum reset attempts"
    )
    parser.add_argument(
        "--timeout-seconds",
        default=1.0,
        type=float,
        help="time to wait for the marker after releasing reset",
    )
    parser.add_argument(
        "--assert-seconds",
        default=0.1,
        type=float,
        help="time to hold reset asserted between attempts",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    socket_path = Path(args.socket)
    if not socket_path.exists():
        raise RuntimeError(f"tty broadcaster socket not found: {socket_path}")

    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as sock:
        sock.connect(str(socket_path))
        sock.setblocking(False)
        drain_socket(sock)
        http_reset(args.host, args.port, "assert")
        time.sleep(args.assert_seconds)
        for attempt in range(1, args.attempts + 1):
            print(f"reset attempt {attempt}/{args.attempts}", file=sys.stderr)
            http_reset(args.host, args.port, "release")
            if wait_for_marker(sock, args.marker.encode("ascii"), args.timeout_seconds):
                print("stage1.5 marker seen", file=sys.stderr)
                return 0
            http_reset(args.host, args.port, "assert")
            time.sleep(args.assert_seconds)
            drain_socket(sock)
    raise RuntimeError("stage1.5 marker not seen after reset retries")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
