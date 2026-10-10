"""Unit tests for the connection soak tool's helpers (no device needed)."""

import base64
import socket
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import connection_soak as soak  # noqa: E402 - imported once its folder is on sys.path


class ConnectionSoakTest(unittest.TestCase):
    def test_alpaca_url_carries_client_ids(self):
        url = soak.alpaca_url("10.0.0.5", "/api/v1/safetymonitor/0/issafe", 7)
        self.assertEqual(url, "http://10.0.0.5/api/v1/safetymonitor/0/issafe?ClientID=4242&ClientTransactionID=7")

    def test_websocket_request_is_an_upgrade(self):
        request = soak.websocket_request("10.0.0.5", "/ws/sensors", "abc==").decode()
        self.assertTrue(request.startswith("GET /ws/sensors HTTP/1.1\r\n"))
        self.assertIn("Upgrade: websocket\r\n", request)
        self.assertIn("Sec-WebSocket-Key: abc==\r\n", request)
        self.assertTrue(request.endswith("\r\n\r\n"))

    def test_websocket_key_is_16_random_bytes(self):
        self.assertEqual(len(base64.b64decode(soak.new_websocket_key())), 16)

    def test_errors_are_classified(self):
        self.assertEqual(soak.classify(socket.timeout("timed out")), "timeout")
        self.assertEqual(soak.classify(ConnectionRefusedError("Connection refused")), "refused")
        self.assertEqual(soak.classify(ConnectionResetError("Connection reset by peer")), "reset")

    def test_stats_count_failures(self):
        stats = soak.Stats()
        stats.record(10, None)
        stats.record(900, "timeout")
        self.assertEqual((stats.requests, stats.failures, stats.slowest_ms), (2, 1, 900))
        self.assertEqual(stats.errors, {"timeout": 1})

    def test_paths_cover_both_devices(self):
        self.assertTrue(any("safetymonitor" in p for p in soak.NINA_PATHS))
        self.assertTrue(any("observingconditions" in p for p in soak.NINA_PATHS))


if __name__ == "__main__":
    unittest.main()
