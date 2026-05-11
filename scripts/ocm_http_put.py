#!/usr/bin/env python3
# Usage:
#   sudo python3 zynq/ocm_http_put.py --uio /dev/uio0 --port 8080
#   curl http://BOARD_IP:8080/healthz
#   curl -X PUT --data-binary @build/jmp.bin http://BOARD_IP:8080/rom
#
# Optional:
#   sudo python3 zynq/ocm_http_put.py --uio /dev/uio0 --size 0x40000 --port 8080

import argparse
import mmap
import os
import sys
import time
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

DEFAULT_UIO = "/dev/uio0"
DEFAULT_SIZE = 256 * 1024
DEFAULT_GPIO_VALUE_PATH = "/sys/class/gpio/gpio524/value"


def read_sysfs_text(path: str) -> str:
    with open(path, "r", encoding="utf-8") as f:
        return f.read().strip()


class UioWriter:
    def __init__(self, uio_path: str, size: int | None):
        self.uio_path = uio_path
        self.uio_name = os.path.basename(uio_path)
        self.sysfs_base = f"/sys/class/uio/{self.uio_name}/maps/map0"
        self.addr = None
        self.name = None

        try:
            self.addr = int(read_sysfs_text(f"{self.sysfs_base}/addr"), 0)
        except OSError:
            self.addr = None

        try:
            map_size = int(read_sysfs_text(f"{self.sysfs_base}/size"), 0)
        except OSError as exc:
            raise RuntimeError(f"failed to read UIO map size: {exc}") from exc

        try:
            self.name = read_sysfs_text(f"{self.sysfs_base}/name")
        except OSError:
            self.name = None

        self.size = map_size if size is None else size
        if self.size > map_size:
            raise RuntimeError(
                f"requested size 0x{self.size:x} exceeds UIO map size 0x{map_size:x}"
            )

        self.fd = os.open(uio_path, os.O_RDWR | os.O_SYNC)
        self.mm = mmap.mmap(
            self.fd,
            self.size,
            flags=mmap.MAP_SHARED,
            prot=mmap.PROT_READ | mmap.PROT_WRITE,
            offset=0,
        )

    def close(self):
        self.mm.close()
        os.close(self.fd)

    def write(self, data: bytes, offset: int = 0) -> int:
        if offset < 0:
            raise ValueError("offset must be >= 0")
        if offset + len(data) > self.size:
            raise ValueError("payload does not fit in configured UIO window")
        self.mm[offset : offset + len(data)] = data
        try:
            self.mm.flush(offset, len(data))
        except OSError as exc:
            # UIO/device-backed mappings can reject flush with EINVAL even
            # though the shared write has already landed.
            if exc.errno != 22:
                raise
        return len(data)

    def read(self, offset: int = 0, size: int | None = None) -> bytes:
        if offset < 0:
            raise ValueError("offset must be >= 0")
        read_size = self.size - offset if size is None else size
        if read_size < 0:
            raise ValueError("size must be >= 0")
        if offset + read_size > self.size:
            raise ValueError("requested range does not fit in configured UIO window")
        return self.mm[offset : offset + read_size]


class GpioWriter:
    def __init__(self, value_path: str):
        self.value_path = value_path

    def write(self, value: int):
        if value not in (0, 1):
            raise ValueError("gpio value must be 0 or 1")
        with open(self.value_path, "w", encoding="utf-8") as f:
            f.write(f"{value}\n")
            f.flush()

    def pulse_for_write(self):
        self.write(1)

    def release_after_write(self):
        time.sleep(0.1)
        self.write(0)


class PutHandler(BaseHTTPRequestHandler):
    server_version = "uio-put/0.1"

    def _write_text(self, status: HTTPStatus, body: str):
        encoded = body.encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(encoded)))
        self.end_headers()
        self.wfile.write(encoded)

    def do_GET(self):
        if self.path == "/" or self.path == "/healthz":
            addr_text = (
                "unknown"
                if self.server.writer.addr is None
                else f"0x{self.server.writer.addr:08x}"
            )
            name_text = (
                ""
                if self.server.writer.name is None
                else f"uio_name={self.server.writer.name}\n"
            )
            self._write_text(
                HTTPStatus.OK,
                (
                    "ok\n"
                    f"uio={self.server.writer.uio_path}\n"
                    f"phys_addr={addr_text}\n"
                    f"map_size={self.server.writer.size}\n"
                    f"{name_text}"
                ),
            )
            return
        if self.path == "/rom":
            try:
                body = self.server.writer.read()
            except (ValueError, OSError) as exc:
                self._write_text(HTTPStatus.INTERNAL_SERVER_ERROR, f"{exc}\n")
                return
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return
        self._write_text(HTTPStatus.NOT_FOUND, "not found\n")

    def do_PUT(self):
        if self.path != "/rom":
            self._write_text(HTTPStatus.NOT_FOUND, "not found\n")
            return

        content_length = self.headers.get("Content-Length")
        if content_length is None:
            self._write_text(HTTPStatus.LENGTH_REQUIRED, "Content-Length required\n")
            return

        try:
            size = int(content_length, 10)
        except ValueError:
            self._write_text(HTTPStatus.BAD_REQUEST, "invalid Content-Length\n")
            return

        if size < 0:
            self._write_text(HTTPStatus.BAD_REQUEST, "negative Content-Length\n")
            return
        if size > self.server.writer.size:
            self._write_text(
                HTTPStatus.REQUEST_ENTITY_TOO_LARGE,
                f"payload too large: {size} > {self.server.writer.size}\n",
            )
            return

        body = self.rfile.read(size)
        if len(body) != size:
            self._write_text(HTTPStatus.BAD_REQUEST, "short request body\n")
            return

        try:
            self.server.gpio.pulse_for_write()
            written = self.server.writer.write(body)
        except (ValueError, OSError) as exc:
            self._write_text(HTTPStatus.INTERNAL_SERVER_ERROR, f"{exc}\n")
            return
        finally:
            try:
                self.server.gpio.release_after_write()
            except OSError as exc:
                sys.stderr.write(f"failed to drive gpio low: {exc}\n")

        self._write_text(
            HTTPStatus.OK,
            (
                "written\n"
                f"bytes={written}\n"
                f"uio={self.server.writer.uio_path}\n"
                f"gpio_value_path={self.server.gpio.value_path}\n"
            ),
        )

    def log_message(self, fmt, *args):
        sys.stderr.write(
            "%s - - [%s] %s\n"
            % (self.address_string(), self.log_date_time_string(), fmt % args)
        )


class UioHttpServer(ThreadingHTTPServer):
    def __init__(
        self,
        server_address,
        handler_class,
        writer: UioWriter,
        gpio: GpioWriter,
    ):
        super().__init__(server_address, handler_class)
        self.writer = writer
        self.gpio = gpio


def parse_args():
    parser = argparse.ArgumentParser(
        description="Accept HTTP PUT and write payload into a UIO-mapped OCM window."
    )
    parser.add_argument("--bind", default="0.0.0.0", help="listen address")
    parser.add_argument("--port", default=8080, type=int, help="listen port")
    parser.add_argument("--uio", default=DEFAULT_UIO, help="UIO device path")
    parser.add_argument(
        "--size",
        default=None,
        type=lambda x: int(x, 0),
        help="active writable size in bytes; defaults to UIO map0 size",
    )
    parser.add_argument(
        "--gpio-value-path",
        default=DEFAULT_GPIO_VALUE_PATH,
        help="sysfs path used to drive gpio12 high before write and low after write",
    )
    return parser.parse_args()


def main():
    args = parse_args()
    writer = UioWriter(args.uio, args.size)
    gpio = GpioWriter(args.gpio_value_path)
    try:
        gpio.release_after_write()
        httpd = UioHttpServer((args.bind, args.port), PutHandler, writer, gpio)
        addr_text = "unknown" if writer.addr is None else f"0x{writer.addr:08x}"
        print(
            f"listening on http://{args.bind}:{args.port} "
            f"for PUT /rom -> {args.uio} phys {addr_text} size {writer.size} "
            f"gpio {gpio.value_path}"
        )
        httpd.serve_forever()
    finally:
        writer.close()


if __name__ == "__main__":
    main()
