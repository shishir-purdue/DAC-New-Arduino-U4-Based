"""Offline tests; does not connect to or modify the instrument.

Run: python -m unittest discover -s tests -p test_buttons.py -v
In Jupyter: from checklist_tests import run_host; run_host("B1")
"""
import unittest
from checklist_tests import run_host

CASES = {
    "B1": "network reset/debounce/reconnect preserves DACs",
    "B2": "network button interrupts a long remote ramp without another write",
    "B3": "concurrent zero ramp, both polarities, no catch-up jumps",
    "B4": "full-scale zero ramp rate and exact endpoint",
    "B5": "safe ramp with Ethernet hardware missing",
    "B6": "driver timeouts and SPI-free button cancellation hook",
    "B7": "simultaneous buttons, latched safe state, deliberate rearm",
    "B8": "safe button preempts network recovery",
    "B9": "safe button interrupts a long remote ramp",
    "B10": "micros and millis rollover",
    "B11": "USB debug backpressure does not block commands/buttons",
    "B12": "failed network recovery leaves DAC unchanged and safe button usable",
    "B13": "driver wait has iteration fallback for a stopped timebase",
    "B14": "Uno R4 startup leaves SD deselected and DAC readback intact",
    "B15": "READ rejects all-high, all-low and wrong-register responses",
    "B16": "remote ramp rejects invalid starting readback without output writes",
    "B17": "safe ramp validates all four starting values before any output write",
    "B18": "valid DAC full-scale responses remain accepted",
    "B19": "remote step sizes 1/10/100 produce exact increments and write counts in both directions",
    "B20": "remote delay scales in microseconds per step; repeated same target does no writes",
    "B21": "delayed PHY link startup can connect without repeated network resets",
    "B22": "opt-in USB actions share network/safe-zero paths and preserve priority",
    "B23": "ramp elapsed-time schedule includes service overhead, wraps safely, and avoids catch-up bursts",
    "B24": "device calibration drives readout, cold INIT, safe-zero endpoint and slew limit",
    "B25": "USB status distinguishes failed safe-zero validation from successful completion",
    "N1": "actual Ethernet driver: stuck socket command times out",
    "N2": "actual Ethernet driver: socket command cancelled by local request",
    "N3": "actual Ethernet driver: unstable RX size times out",
    "N4": "actual Ethernet driver: unstable TX size times out",
    "N5": "actual Ethernet driver: full TCP transmit buffer times out",
    "N6": "actual Ethernet driver: missing SEND_OK times out",
    "N7": "actual Ethernet driver: local request cancels TCP send",
    "N8": "actual Ethernet driver: reset/redetection and socket cache clearing",
    "N9": "actual Ethernet driver: local request cancels initialization delay",
    "N10": "actual Ethernet driver: missing UDP SEND_OK times out",
    "N11": "W5500 PHY reset restores auto-negotiation from power-down",
    "N12": "W5500 PHY readback failure leaves network offline",
}

class ButtonTests(unittest.TestCase):
    pass

for case, description in CASES.items():
    def test(self, case=case):
        run_host(case)
    test.__doc__ = description
    setattr(ButtonTests, f"test_{case}", test)

if __name__ == "__main__":
    unittest.main()
