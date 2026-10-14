"""Validate the notebook's live-test helpers without accessing any network."""
import socket
import unittest
from unittest.mock import Mock, patch
from live_bench import Bench, Session, verify_step_intervals, verify_init_waveform, verify_bus_capture


class TestLiveHelpers(unittest.TestCase):
    def test_disabled_bench_never_connects(self):
        with patch("live_bench.socket.create_connection") as connect:
            with self.assertRaisesRegex(AssertionError, "explicitly"):
                Bench(host="example.invalid").readonly()
            connect.assert_not_called()

    def test_output_gate_precedes_network(self):
        with patch("live_bench.socket.create_connection") as connect:
            for method in ["set_readback", "ramp_and_reject", "abort_ramp"]:
                with self.assertRaises(AssertionError):
                    getattr(Bench(host="example.invalid", enabled=True), method)()
            connect.assert_not_called()

    def test_fragmented_and_coalesced_responses(self):
        sock = Mock()
        sock.recv.side_effect = [b"AD", b"5780\r\n42\r\n"]
        session = Session(sock, 1)
        self.assertEqual(session.request("ID"), "AD5780")
        self.assertEqual(session.line(), "42")
        sock.sendall.assert_called_once_with(b"ID\n")
        self.assertEqual(sock.recv.call_count, 2)

    def test_bad_greeting_closes_socket(self):
        sock = Mock()
        sock.recv.return_value = b"ERR! Server busy\r\n"
        with patch("live_bench.socket.create_connection", return_value=sock):
            with self.assertRaisesRegex(AssertionError, "greeting"):
                Bench(host="example.invalid", enabled=True).connect()
        sock.close.assert_called_once()

    def test_closed_connection_and_timeout_surface_errors(self):
        sock = Mock()
        sock.recv.return_value = b""
        with self.assertRaisesRegex(AssertionError, "closed"):
            Session(sock, 1).line()
        sock.recv.side_effect = socket.timeout("deadline")
        with self.assertRaises(socket.timeout):
            Session(sock, 1).line()

    def test_context_always_closes(self):
        sock = Mock()
        with self.assertRaisesRegex(RuntimeError, "injected"):
            with Session(sock, 1):
                raise RuntimeError("injected")
        sock.close.assert_called_once()

    def test_evidence_assertions_fail_on_bad_or_missing_data(self):
        for args in [([], 100, 10), ([200], 100, 10), ([float("nan")], 100, 10)]:
            with self.assertRaises(AssertionError):
                verify_step_intervals(*args)
        for args in [([], -1, 1), ([2], -1, 1), ([float("nan")], -1, 1)]:
            with self.assertRaises(AssertionError):
                verify_init_waveform(*args)
        for args in [(0, 0, 0), (1, 0, 10), (0, 1, 10), (-1, 0, 10)]:
            with self.assertRaises(AssertionError):
                verify_bus_capture(*args)


if __name__ == "__main__":
    unittest.main()
