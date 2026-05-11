#!/usr/bin/env python3

import argparse
import hashlib
import http.client
import sys
from pathlib import Path

DEFAULT_HOST = "alarm.local"
DEFAULT_PORT = 8080
DEFAULT_PATH = "/rom"
DEFAULT_SIZE = 256 * 1024


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def put_rom(host: str, port: int, path: str, payload: bytes) -> str:
    conn = http.client.HTTPConnection(host, port, timeout=10)
    try:
        conn.request(
            "PUT",
            path,
            body=payload,
            headers={
                "Content-Length": str(len(payload)),
                "Content-Type": "application/octet-stream",
            },
        )
        resp = conn.getresponse()
        body = resp.read().decode("utf-8", errors="replace")
        if resp.status != 200:
            raise RuntimeError(f"PUT failed: status={resp.status} body={body!r}")
        return body
    finally:
        conn.close()


def get_rom(host: str, port: int, path: str) -> bytes:
    conn = http.client.HTTPConnection(host, port, timeout=10)
    try:
        conn.request("GET", path)
        resp = conn.getresponse()
        body = resp.read()
        if resp.status != 200:
            raise RuntimeError(f"GET failed: status={resp.status} body={body!r}")
        return body
    finally:
        conn.close()


def parse_put_response(body: str) -> int:
    for line in body.splitlines():
        if line.startswith("bytes="):
            return int(line.split("=", 1)[1], 10)
    raise RuntimeError(f"PUT response missing bytes= line: {body!r}")


def parse_args():
    parser = argparse.ArgumentParser(
        description="Upload a ROM image, verify bytes written, then read it back."
    )
    parser.add_argument("image", help="ROM image path")
    parser.add_argument("--host", default=DEFAULT_HOST, help="HTTP updater host")
    parser.add_argument("--port", default=DEFAULT_PORT, type=int, help="HTTP updater port")
    parser.add_argument("--path", default=DEFAULT_PATH, help="HTTP updater path")
    parser.add_argument(
        "--expected-size",
        default=DEFAULT_SIZE,
        type=lambda x: int(x, 0),
        help="expected ROM size in bytes",
    )
    parser.add_argument(
        "--readback",
        default=None,
        help="optional path to save GET /rom output",
    )
    parser.add_argument(
        "--skip-compare",
        action="store_true",
        help="skip comparing uploaded bytes with readback bytes",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    image_path = Path(args.image)
    payload = image_path.read_bytes()
    if len(payload) != args.expected_size:
        raise RuntimeError(
            f"unexpected image size: {len(payload)} != {args.expected_size}"
        )

    put_body = put_rom(args.host, args.port, args.path, payload)
    written = parse_put_response(put_body)
    if written != args.expected_size:
        raise RuntimeError(f"unexpected written size: {written} != {args.expected_size}")

    readback = get_rom(args.host, args.port, args.path)
    if len(readback) != args.expected_size:
        raise RuntimeError(
            f"unexpected readback size: {len(readback)} != {args.expected_size}"
        )

    if args.readback is not None:
        Path(args.readback).write_bytes(readback)

    if not args.skip_compare and readback != payload:
        raise RuntimeError(
            "readback mismatch: "
            f"upload_sha256={sha256_hex(payload)} "
            f"readback_sha256={sha256_hex(readback)}"
        )

    print("upload_ok")
    print(f"image={image_path}")
    print(f"bytes={written}")
    print(f"host={args.host}:{args.port}")
    print(f"path={args.path}")
    print(f"sha256={sha256_hex(payload)}")
    if args.readback is not None:
        print(f"readback={args.readback}")
    if args.skip_compare:
        print("compare=skipped")
    else:
        print("compare=ok")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
