"""Helpers for an existing DAC connection exposing query(command) -> str.

No connection or output change occurs on import. Requires firmware with the
RAMP ACCEPTED / STATUS protocol. Voltage conversion assumes +/-10 V references.
"""
import math
import operator
import time

V_PER_BIT = 20.0 / 2**18


def ramp_parameters(channel, voltage, speed):
    channel = operator.index(channel)
    if channel not in (1, 2, 3, 4):
        raise ValueError("channel must be 1-4")
    if not math.isfinite(voltage) or not -10 <= voltage <= 10:
        raise ValueError("voltage must be between -10 and +10 V")
    if not math.isfinite(speed) or speed <= 0:
        raise ValueError("speed must be finite and positive, in V/s")
    target = min(262143, int((voltage + 10) / V_PER_BIT))
    step = 10
    delay_us = step * V_PER_BIT / speed * 1_000_000
    if not math.isfinite(delay_us) or delay_us > 2_147_483_647:
        raise ValueError("speed is too slow for the firmware delay range")
    return channel, target, step, max(1, math.ceil(delay_us))


def ramp(dac, channel, voltage, speed, *, wait=True, timeout=3600, poll_interval=0.1):
    """Ramp using speed in V/s; optionally poll STATUS until completion.

    timeout bounds the overall wait; the connection's own query timeout must also
    be finite. On timeout this raises without changing outputs or disconnecting;
    the ramp may still be running. Disconnect deliberately to abort it.
    """
    channel, target, step, delay_us = ramp_parameters(channel, voltage, speed)
    if not math.isfinite(timeout) or timeout <= 0:
        raise ValueError("timeout must be finite and positive")
    if not math.isfinite(poll_interval) or poll_interval <= 0:
        raise ValueError("poll_interval must be finite and positive")
    deadline = time.monotonic() + timeout
    response = dac.query(f"RAMP {channel} {target} {step} {delay_us}")
    expected = f"RAMP ACCEPTED {channel} {target} {step} {delay_us}"
    if response.strip() != expected:
        raise RuntimeError(f"Ramp was not accepted: {response!r}")
    if not wait:
        return response
    while True:
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            raise TimeoutError("Ramp completion deadline expired; ramp may still be running")
        status = dac.query("STATUS").strip()
        parts = status.split()
        if len(parts) != 4 or parts[0] != "RAMP" or parts[2:] != [str(channel), str(target)]:
            raise RuntimeError(f"Unexpected ramp status: {status!r}")
        if parts[1] == "COMPLETE":
            return status
        if parts[1] != "RUNNING":
            raise RuntimeError(f"Ramp did not complete: {status!r}")
        time.sleep(min(poll_interval, remaining))
