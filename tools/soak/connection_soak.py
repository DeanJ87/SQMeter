#!/usr/bin/env python3
"""Soak test: does an imaging app keep its connection while browsers misbehave?

Acts like N.I.N.A. (polls the SafetyMonitor and ObservingConditions every few
seconds) while it opens "stuck" live-update sockets the way a browser tab in a
sleeping laptop or phone does: the socket stays open but nothing reads it.
Every failed or slow Alpaca request is counted. Spec 011 FR-008, 013 FR-012.

    python3 tools/soak/connection_soak.py --host 192.168.1.50 --minutes 30

Exit status 0 when every imaging-app request succeeded, 1 otherwise. Only the
Python standard library is used.
"""

from __future__ import annotations

import argparse
import base64
import json
import os
import socket
import sys
import threading
import time
import urllib.error
import urllib.request
from dataclasses import dataclass, field

# What N.I.N.A. asks each poll (safety monitor + weather device).
NINA_PATHS = (
    "/api/v1/safetymonitor/0/connected",
    "/api/v1/safetymonitor/0/issafe",
    "/api/v1/observingconditions/0/connected",
    "/api/v1/observingconditions/0/cloudcover",
    "/api/v1/observingconditions/0/skytemperature",
    "/api/v1/observingconditions/0/temperature",
    "/api/v1/observingconditions/0/humidity",
)
LIVE_PATHS = ("/ws/sensors", "/ws/status")


@dataclass
class Stats:
    requests: int = 0
    failures: int = 0
    slowest_ms: float = 0.0
    errors: dict[str, int] = field(default_factory=dict)
    lock: threading.Lock = field(default_factory=threading.Lock)

    def record(self, elapsed_ms: float, error: str | None) -> None:
        with self.lock:
            self.requests += 1
            self.slowest_ms = max(self.slowest_ms, elapsed_ms)
            if error:
                self.failures += 1
                self.errors[error] = self.errors.get(error, 0) + 1


def alpaca_url(host: str, path: str, transaction: int) -> str:
    return f"http://{host}{path}?ClientID=4242&ClientTransactionID={transaction}"


def websocket_request(host: str, path: str, key: str) -> bytes:
    return (
        f"GET {path} HTTP/1.1\r\nHost: {host}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n"
    ).encode()


def new_websocket_key() -> str:
    return base64.b64encode(os.urandom(16)).decode()


def classify(exc: BaseException) -> str:
    text = str(exc).lower()
    if "timed out" in text or isinstance(exc, TimeoutError | socket.timeout):
        return "timeout"
    if "refused" in text:
        return "refused"
    if "reset" in text:
        return "reset"
    return type(exc).__name__


def poll_once(host: str, transaction: int, timeout: float) -> tuple[float, str | None]:
    started = time.monotonic()
    error = None
    for path in NINA_PATHS:
        try:
            with urllib.request.urlopen(alpaca_url(host, path, transaction), timeout=timeout) as reply:
                body = json.loads(reply.read())
                if body.get("ErrorNumber", 0) != 0:
                    error = f"alpaca-{body['ErrorNumber']}"
        except (urllib.error.URLError, OSError, ValueError) as exc:
            error = classify(exc.reason if isinstance(exc, urllib.error.URLError) else exc)
        if error:
            break
    return (time.monotonic() - started) * 1000, error


def imaging_app(host: str, every: float, timeout: float, stats: Stats, stop: threading.Event) -> None:
    transaction = 0
    while not stop.is_set():
        transaction += 1
        elapsed, error = poll_once(host, transaction, timeout)
        stats.record(elapsed, error)
        if error:
            print(f"{time.strftime('%H:%M:%S')} imaging app request failed: {error}", flush=True)
        stop.wait(every)


def open_stuck_socket(host: str, path: str) -> socket.socket | None:
    """A live-update socket nobody reads: its receive window fills and stays full."""
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
        sock.settimeout(5)
        sock.connect((host, 80))
        sock.sendall(websocket_request(host, path, new_websocket_key()))
        return sock
    except OSError:
        return None


def sleeping_tabs(host: str, every: float, keep: int, stop: threading.Event, held: list[socket.socket]) -> None:
    """Every `every` seconds a tab 'wakes', opens its two live sockets, then sleeps again."""
    while not stop.wait(every):
        for path in LIVE_PATHS:
            sock = open_stuck_socket(host, path)
            if sock:
                held.append(sock)
        while len(held) > keep:
            held.pop(0).close()


def device_connections(host: str) -> dict:
    try:
        with urllib.request.urlopen(f"http://{host}/api/status", timeout=8) as reply:
            status = json.loads(reply.read())
        return {k: status.get(k) for k in ("uptime", "resetReason", "freeHeap", "connections")}
    except (urllib.error.URLError, OSError, ValueError) as exc:
        return {"error": classify(exc)}


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--host", required=True, help="device address, e.g. 192.168.1.50")
    parser.add_argument("--minutes", type=float, default=30)
    parser.add_argument("--poll-seconds", type=float, default=3, help="imaging-app poll interval")
    parser.add_argument("--timeout", type=float, default=5, help="per request; N.I.N.A. gives up about here")
    parser.add_argument("--tab-every", type=float, default=20, help="seconds between stuck tabs opening")
    parser.add_argument("--keep", type=int, default=24, help="stuck sockets held open at once")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    stats = Stats()
    stop = threading.Event()
    held: list[socket.socket] = []
    workers = [
        threading.Thread(target=imaging_app, args=(args.host, args.poll_seconds, args.timeout, stats, stop), daemon=True),
        threading.Thread(target=sleeping_tabs, args=(args.host, args.tab_every, args.keep, stop, held), daemon=True),
    ]
    for worker in workers:
        worker.start()
    before = device_connections(args.host)
    end = time.monotonic() + args.minutes * 60
    try:
        while time.monotonic() < end:
            time.sleep(60)
            print(
                json.dumps(
                    {
                        "requests": stats.requests,
                        "failures": stats.failures,
                        "stuckHeld": len(held),
                        "device": device_connections(args.host),
                    }
                ),
                flush=True,
            )
    except KeyboardInterrupt:
        pass
    stop.set()
    for sock in held:
        sock.close()
    summary = {
        "requests": stats.requests,
        "failures": stats.failures,
        "errors": stats.errors,
        "slowestMs": round(stats.slowest_ms),
        "before": before,
        "after": device_connections(args.host),
    }
    print(json.dumps(summary, indent=2))
    return 0 if stats.failures == 0 and stats.requests > 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
