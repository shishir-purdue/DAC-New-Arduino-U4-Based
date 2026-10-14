"""Optional bounded TCP checks. Nothing connects until explicitly enabled."""
from __future__ import annotations

from dataclasses import dataclass
import math
import socket
import time


class Session:
    def __init__(self, sock, timeout):
        self.sock, self.timeout, self.buffer = sock, timeout, b""

    def line(self):
        deadline = time.monotonic() + self.timeout
        while b"\n" not in self.buffer:
            remaining = deadline - time.monotonic()
            assert remaining > 0, f"Response deadline expired; partial response={self.buffer!r}"
            self.sock.settimeout(remaining)
            data = self.sock.recv(4096)
            assert data, f"Board closed connection; partial response={self.buffer!r}"
            self.buffer += data
            assert len(self.buffer) <= 65536, "Response exceeded 64 KiB"
        line, self.buffer = self.buffer.split(b"\n", 1)
        return line.decode("ascii", errors="replace").rstrip("\r")

    def request(self, command):
        self.sock.sendall((command + "\n").encode("ascii"))
        return self.line()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.sock.close()


@dataclass
class Bench:
    host: str = ""
    port: int = 23
    enabled: bool = False
    outputs_isolated: bool = False
    allow_output_changes: bool = False
    channel: int = 1
    low_code: int | None = None
    high_code: int | None = None
    timeout: float = 3.0

    def connect(self):
        assert self.enabled and self.host, "Set bench.host and bench.enabled=True explicitly before live tests"
        assert self.timeout > 0, "Set a positive socket timeout"
        session = Session(socket.create_connection((self.host, self.port), self.timeout), self.timeout)
        try:
            assert "Welcome" in session.line(), "Missing greeting (old available() behavior waits for client data)"
            assert "HELP" in session.line(), "Missing command-help greeting"
            return session
        except BaseException:
            session.sock.close()
            raise

    def output_gate(self):
        assert self.enabled and self.outputs_isolated and self.allow_output_changes, "Output tests require enabled=True, outputs_isolated=True, and allow_output_changes=True"
        assert 1 <= self.channel <= 4, "Select channel 1..4"
        assert type(self.low_code) is int and type(self.high_code) is int, "Choose explicit bench test codes"
        assert 0 <= self.low_code < self.high_code <= 262143, "Require 0 <= low_code < high_code <= 262143"
        assert self.high_code - self.low_code <= 1000, "Bench tests limit the selected excursion to 1000 codes"

    def readonly(self, repeats=20):
        assert 1 <= repeats <= 10000
        with self.connect() as client:
            for _ in range(repeats):
                assert client.request("ID") == "AD5780", "ID failed or connection was lost"
            values = []
            for channel in range(1, 5):
                value = client.request(f"READ {channel}")
                assert value.isdigit() and 0 <= int(value) <= 262143, f"Invalid READ response: {value!r}"
                values.append(int(value))
        print("PASS live ID/READ continuity; register codes:", values)
        return values

    def reject_extra_clients(self, repeats=32):
        assert 1 <= repeats <= 1000
        with self.connect() as owner:
            for _ in range(repeats):
                with socket.create_connection((self.host, self.port), self.timeout) as extra:
                    response = Session(extra, self.timeout).line()
                    assert "ERR" in response and "busy" in response.lower(), f"Silent second client not rejected: {response!r}"
                    extra.settimeout(self.timeout)
                    try:
                        assert extra.recv(1) == b"", "Rejected socket was not closed"
                    except ConnectionResetError:
                        pass  # Reset is also a released/rejected TCP connection.
                assert owner.request("ID") == "AD5780", "Owner was lost while rejecting another client"
        print("PASS live Mem-2/Robust-1/C3: silent second clients closed; owner retained")

    def reconnect_partial_command(self):
        with self.connect() as client:
            client.sock.sendall(b"ID")  # No newline: must never contaminate next session.
        time.sleep(0.25)
        with self.connect() as client:
            assert client.request("ID") == "AD5780", "Partial command contaminated the new session"
        print("PASS live Mem-3: reconnect after partial line")

    def invalid_channels(self):
        # READ only: do not try malformed SET/INIT/RAMP on attached hardware.
        with self.connect() as client:
            for channel in [0, -1, 5, 99]:
                response = client.request(f"READ {channel}")
                assert response.startswith("ERR"), f"READ {channel} was not rejected: {response!r}"
        print("PASS live C1 READ channel validation")

    def set_readback(self):
        self.output_gate()
        with self.connect() as client:
            # No INIT: it would overwrite the output with midscale (M9).
            for code in [self.low_code, self.high_code, self.low_code]:
                reply = client.request(f"SET {self.channel} {code}")
                assert "SET command received" in reply, reply
                reply = client.request(f"READ {self.channel}")
                assert reply == str(code), f"Readback mismatch: requested {code}, got {reply!r}"
        print("PASS live SET/READ register test; final code:", self.low_code)

    def ramp_and_reject(self):
        self.output_gate()
        span = self.high_code - self.low_code
        assert span >= 100, "Choose a span of 100..1000 codes so the ramp lasts long enough to test a second client"
        with self.connect() as owner:
            assert "SET command received" in owner.request(f"SET {self.channel} {self.low_code}")
            owner.sock.sendall(f"RAMP {self.channel} {self.high_code} 1 10000\n".encode("ascii"))
            time.sleep(0.1)
            with socket.create_connection((self.host, self.port), self.timeout) as extra:
                start = time.monotonic()
                reply = Session(extra, min(self.timeout, 0.5)).line()
                assert "busy" in reply.lower(), f"Client not rejected during ramp: {reply!r}"
                assert time.monotonic() - start < 0.5, "Second-client rejection exceeded 500 ms during ramp"
            owner.timeout = max(self.timeout, span * 0.01 + 5)
            reply = owner.line()
            assert reply.startswith("RAMP"), f"Ramp completion response missing: {reply!r}"
            assert owner.request(f"READ {self.channel}") == str(self.high_code), "Ramp failed to reach target"
        print("PASS live Robust-1/C3 during ramp; final code:", self.high_code)

    def abort_ramp(self):
        self.output_gate()
        assert self.high_code - self.low_code >= 100, "Abort test needs span >=100 codes"
        with self.connect() as client:
            assert "SET command received" in client.request(f"SET {self.channel} {self.low_code}")
            client.sock.sendall(f"RAMP {self.channel} {self.high_code} 1 10000\n".encode("ascii"))
            time.sleep(0.1)
        # Allow FIN processing. This measures observed recovery, not hard real time.
        time.sleep(0.25)
        with self.connect() as client:
            first = int(client.request(f"READ {self.channel}"))
            time.sleep(0.1)
            second = int(client.request(f"READ {self.channel}"))
        assert self.low_code < first < self.high_code, f"Ramp did not start or failed to stop early: {first}"
        assert second == first, f"Output kept ramping after disconnect: {first} -> {second}"
        print("PASS live C5/Robust-2: disconnect stopped ramp and reconnect worked; held code:", first)

    def cable_recovery(self, max_recovery_seconds=15):
        assert max_recovery_seconds > 0
        # Keep the original socket open while link is lost: recreating only after
        # a clean close would miss the half-open-owner recovery problem (M1).
        with self.connect() as original:
            assert original.request("ID") == "AD5780"
            input("Unplug the DAC Ethernet cable, wait five seconds, reconnect it, then press Enter. ")
            start = time.monotonic()
            original.timeout = max_recovery_seconds
            assert original.request("ID") == "AD5780", "Original socket failed after cable recovery"
            assert time.monotonic() - start <= max_recovery_seconds, "Cable recovery exceeded the chosen deadline"
        time.sleep(0.25)
        self.readonly(repeats=1)
        print("PASS live C4 controlled unplug/replug; this does not cover lost TCP peers or every PHY fault")


def verify_step_intervals(measured_us, requested_us, tolerance_us):
    """H4: enter oscilloscope/logic-analyzer intervals between actual output updates."""
    assert measured_us and isinstance(requested_us, (int, float)) and isinstance(tolerance_us, (int, float)), "Provide measured intervals and an explicit timing tolerance"
    assert all(math.isfinite(v) and v >= 0 for v in [*measured_us, requested_us, tolerance_us]) and requested_us > 0, "Timing values must be finite and nonnegative, with a positive requested period"
    errors = [abs(value - requested_us) for value in measured_us]
    assert max(errors) <= tolerance_us, f"H4: worst step timing error {max(errors)} us exceeds {tolerance_us} us"
    print("PASS H4 measured step timing within chosen tolerance")


def verify_init_waveform(measured_volts, minimum_volts, maximum_volts):
    """M9: capture from before INIT through settling; limits come from apparatus policy."""
    assert measured_volts and isinstance(minimum_volts, (int, float)) and isinstance(maximum_volts, (int, float)), "Provide captured waveform samples and approved voltage limits"
    assert all(math.isfinite(v) for v in [*measured_volts, minimum_volts, maximum_volts]) and minimum_volts <= maximum_volts, "Waveform and limits must be finite and ordered"
    assert min(measured_volts) >= minimum_volts and max(measured_volts) <= maximum_volts, f"M9: captured range [{min(measured_volts)}, {max(measured_volts)}] V exceeds approved limits"
    print("PASS M9 measured INIT waveform within chosen limits")


def verify_bus_capture(overlapping_cs_frames, malformed_frames, tested_frames):
    """M8: enter counts from shared-bus capture with the actual relevant ISR enabled."""
    assert all(type(v) is int and v >= 0 for v in [overlapping_cs_frames, malformed_frames, tested_frames]) and tested_frames > 0, "Supply nonnegative integer counts and a positive analyzed frame count from the target bench capture"
    assert overlapping_cs_frames == 0 and malformed_frames == 0, f"M8: {overlapping_cs_frames} overlapping-CS frames, {malformed_frames} malformed frames"
    print("PASS M8 captured bus framing; record the ISR/core/library and trace file used")
