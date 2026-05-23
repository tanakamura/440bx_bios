#!/usr/bin/env python3

import argparse
import os
import selectors
import signal
import socket
import sys
import termios
from pathlib import Path


def configure_serial(fd: int, baud: int) -> None:
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
    attrs[3] = 0
    speed = {
        9600: termios.B9600,
        19200: termios.B19200,
        38400: termios.B38400,
        57600: termios.B57600,
        115200: termios.B115200,
    }.get(baud)
    if speed is None:
        raise ValueError(f"unsupported baud: {baud}")
    attrs[4] = speed
    attrs[5] = speed
    attrs[6][termios.VMIN] = 1
    attrs[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)


def parse_args():
    parser = argparse.ArgumentParser(
        description="Broadcast ttyS0 to multiple AF_UNIX clients."
    )
    parser.add_argument(
        "--path",
        help="AF_UNIX socket path",
        default="/tmp/ttyS0_bcast.sock",
    )
    parser.add_argument("--device", default="/dev/ttyS0", help="serial device path")
    parser.add_argument("--baud", default=115200, type=int, help="baud rate")
    return parser.parse_args()


def cleanup_socket(path: Path) -> None:
    try:
        path.unlink()
    except FileNotFoundError:
        pass


def close_client(sel: selectors.BaseSelector, conn: socket.socket) -> None:
    try:
        sel.unregister(conn)
    except Exception:
        pass
    try:
        conn.close()
    except Exception:
        pass


def main() -> int:
    args = parse_args()
    socket_path = Path(args.path)
    socket_path.parent.mkdir(parents=True, exist_ok=True)
    cleanup_socket(socket_path)

    tty_fd = os.open(args.device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.setblocking(False)
    sel = selectors.DefaultSelector()
    clients = set()
    stop = False

    def request_stop(_signum, _frame):
        nonlocal stop
        stop = True

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    try:
        configure_serial(tty_fd, args.baud)
        server.bind(str(socket_path))
        server.listen()

        sel.register(server, selectors.EVENT_READ, "server")
        sel.register(tty_fd, selectors.EVENT_READ, "tty")

        while not stop:
            for key, _mask in sel.select(timeout=0.5):
                if key.data == "server":
                    conn, _addr = server.accept()
                    conn.setblocking(False)
                    clients.add(conn)
                    sel.register(conn, selectors.EVENT_READ, "client")
                    continue

                if key.data == "tty":
                    try:
                        data = os.read(tty_fd, 4096)
                    except BlockingIOError:
                        continue
                    if not data:
                        continue
                    dead = []
                    for conn in clients:
                        try:
                            conn.sendall(data)
                        except OSError:
                            dead.append(conn)
                    for conn in dead:
                        clients.discard(conn)
                        close_client(sel, conn)
                    continue

                conn = key.fileobj
                try:
                    data = conn.recv(4096)
                except OSError:
                    data = b""
                if not data:
                    clients.discard(conn)
                    close_client(sel, conn)
                    continue
                view = memoryview(data)
                while len(view) != 0:
                    try:
                        written = os.write(tty_fd, view)
                    except BlockingIOError:
                        continue
                    view = view[written:]
    finally:
        for conn in list(clients):
            close_client(sel, conn)
        try:
            sel.unregister(server)
        except Exception:
            pass
        try:
            sel.unregister(tty_fd)
        except Exception:
            pass
        sel.close()
        server.close()
        os.close(tty_fd)
        cleanup_socket(socket_path)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
