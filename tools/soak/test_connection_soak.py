"""Unit tests for the connection soak tool's helpers (no device needed)."""

import base64
import io
import json
import socket
import sys
import unittest
import urllib.error
from pathlib import Path
from unittest import mock

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

    def test_http_errors_are_failures(self):
        error = urllib.error.HTTPError("http://x/", 500, "Internal Server Error", {}, io.BytesIO(b""))
        self.addCleanup(error.close)
        self.assertEqual(soak.classify(error), "http-500")
        self.assertEqual(soak.classify(urllib.error.URLError(ConnectionRefusedError("Connection refused"))), "refused")

    def test_alpaca_results(self):
        self.assertIsNone(soak.alpaca_result({"Value": 3.2, "ErrorNumber": 0}))
        self.assertIsNone(soak.alpaca_result({"Value": True}))
        self.assertEqual(soak.alpaca_result({"ErrorNumber": 1024}), "not-implemented")
        self.assertEqual(soak.alpaca_result({"ErrorNumber": 1031}), "alpaca-1031")

    def poll(self, replies, stats):
        """poll_once against canned replies: a dict per path, or an exception."""

        def urlopen(url, timeout):
            path = url.split("//", 1)[1].split("?", 1)[0]
            path = path[path.index("/") :]
            reply = replies.get(path, {"Value": 1, "ErrorNumber": 0})
            if isinstance(reply, BaseException):
                raise reply
            return io.BytesIO(json.dumps(reply).encode())

        with mock.patch.object(soak.urllib.request, "urlopen", urlopen), mock.patch("builtins.print") as printed:
            result = soak.poll_once("10.0.0.5", 1, 5, stats)
        return result[1], printed

    def test_not_implemented_is_not_a_failure_and_is_reported_once(self):
        stats = soak.Stats()
        replies = {
            "/api/v1/observingconditions/0/cloudcover": {"ErrorNumber": 1024, "ErrorMessage": "No IR sensor"},
            "/api/v1/observingconditions/0/skytemperature": {"ErrorNumber": 0x400},
        }
        error, printed = self.poll(replies, stats)
        self.assertIsNone(error)
        self.assertEqual(printed.call_count, 2)
        self.assertEqual(
            stats.not_implemented,
            {"/api/v1/observingconditions/0/cloudcover", "/api/v1/observingconditions/0/skytemperature"},
        )
        error, printed = self.poll(replies, stats)
        self.assertIsNone(error)
        printed.assert_not_called()

    def test_other_alpaca_errors_and_transport_failures_count(self):
        stats = soak.Stats()
        error, _ = self.poll({"/api/v1/safetymonitor/0/issafe": {"ErrorNumber": 1031}}, stats)
        self.assertEqual(error, "alpaca-1031")
        error, _ = self.poll({"/api/v1/safetymonitor/0/issafe": socket.timeout("timed out")}, stats)
        self.assertEqual(error, "timeout")
        error, _ = self.poll({"/api/v1/safetymonitor/0/connected": ConnectionResetError("reset by peer")}, stats)
        self.assertEqual(error, "reset")

    def test_paths_cover_both_devices(self):
        self.assertTrue(any("safetymonitor" in p for p in soak.NINA_PATHS))
        self.assertTrue(any("observingconditions" in p for p in soak.NINA_PATHS))


if __name__ == "__main__":
    unittest.main()
