"""Regenerate the clean notebook from the case registry using the Python stdlib."""
import json
from pathlib import Path
from checklist_tests import DESCRIPTIONS, MODES

cells = []


def markdown(text):
    cells.append({"cell_type": "markdown", "metadata": {}, "source": text.splitlines(True)})


def code(text):
    cells.append({"cell_type": "code", "execution_count": None, "metadata": {}, "outputs": [], "source": text.splitlines(True)})


markdown("""# DAC firmware bug checklist

Run the setup cell, then any individual case. Each case is independent and runs
against the current checked-out source. **AssertionError means the requirement
failed**. All 36 checklist cases are expected to pass after the repair phase.
No hardware is accessed by the offline tests. Physical measurements below are
separate, optional evidence; an offline pass does not establish analog behavior.

The CLI and standard-library unittest runner in `tests/README.md` are useful for repeatable
regressions. The host harness needs a C++17 compiler; it uses the actual C++ firmware
with hardware doubles. This does not substitute for compiling/flashing the Uno R4.

Run individual cells to inspect failures, or use `run_all()` to collect everything
without stopping at the first failure. Do not enable live output tests while
outputs are connected to apparatus.
""")
code("""from pathlib import Path
import sys

# Works with a Jupyter working directory at the repository root or under tests/.
# Set ROOT explicitly if you launch the notebook from a different folder.
ROOT = next((p for p in [Path.cwd(), *Path.cwd().parents]
             if (p / "Software/src/main.cpp").is_file()), None)
assert ROOT is not None, "Set ROOT to the DAC Box repository directory"
sys.path.insert(0, str(ROOT / "tests"))
from checklist_tests import run_case, run_all, PRELIMINARY, DESCRIPTIONS, MODES
print("Testing source in:", ROOT)
""")
markdown("## Preliminary regression summary")
code("preliminary_results = run_all(PRELIMINARY, fail_on_error=True)\n")
markdown("## Complete checklist summary\nAll 36 software checks should pass. Failures remain visible and are never marked expected-success.")
code("all_results = run_all()\n")

details = {
    "Mem-1": "Counts C++ allocations across 6,000 dispatches plus source checks for allocation APIs. It does not measure real MCU heap fragmentation.",
    "C4": "Injects link/configuration loss, absent hardware, and delayed PHY startup. Verifies ten-second startup grace, retry spacing, fresh connections, preserved DAC codes, timer rollover, and SAFE ZERO priority. Use the optional cable test for physical evidence.",
    "C5": "Runs disconnect, command-service, pre-write abort, and second-client regressions. Long waits service input/network/buttons in bounded slices.",
    "H4": "Checks bounded waits and cooperative service. B19/B20 separately verify virtual step/delay scaling; physical timing measurements below test actual accuracy.",
    "M1": "The implemented policy releases an idle owner after 120 seconds without received bytes, while keeping long active ramps alive. Covers renewal, reconnect, and timer rollover.",
    "M8": "Reviewed as no current ISR contention defect: verifies balanced foreground SPI transactions, absence of application SPI ISRs, and SPI-free cancellation hooks. A future SPI ISR needs a new ownership review.",
    "M9": "INIT preserves normal active channels without writes. Cold ground-clamped channels are configured and verified at zero before release; incompatible active modes fail unchanged. Scope capture remains supplemental.",
    "M10": "Huge integers are tested on the host only. Strict signed-32-bit parsing must reject overflow before any DAC access.",
    "A1": "Codex addition: existing connection reclassified as a second client. This overlaps Mem-2 and is repaired in that preliminary fix.",
    "A2": "Codex addition: injects corrupt readback and ignored register writes; INIT must report failure and retain a cold channel's clamp.",
    "A3": "Codex addition: audits exact Uno R4/platform/core/compiler/uploader versions and the vendored Ethernet fork. A separate clean target build is recorded in VALIDATION.md.",
    "A4": "Codex addition: disconnect during the simulated step delay must prevent the next DAC write.",
    "A5": "Codex addition: bounded/cancellable actual Ethernet-driver waits, chip/PHY reset, socket-cache cleanup, and a stopped-timebase fallback.",
    "A6": "Codex addition: USB diagnostic backpressure must not prevent commands or local button servicing.",
    "A7": "Codex-introduced regression, fixed: models Uno R4 OUTPUT mode clearing the latch; SD must be deselected before SPI traffic.",
    "A8": "Codex addition: invalid register frames must fail READ and both ramp paths without output writes. Valid full-scale codes remain accepted.",
    "A9": "Client-wrapper addition: checks V/s-to-microseconds conversion, voltage boundaries, input rejection, acceptance/status polling, aborts, and timeouts without a connection.",
}
for case_id, description in DESCRIPTIONS.items():
    markdown(f"## {case_id} — {description}\n\nMode: **{MODES[case_id]}**. " + details.get(case_id, "Failure output states the observed discrepancy."))
    code(f'run_case("{case_id}")\n')

markdown("""## Optional live bench setup — disabled by default

Set the address yourself. These cells do not upload firmware. Record the revision
flashed on your board; otherwise the test does not establish which source it ran.
Only set `outputs_isolated` and `allow_output_changes` after disconnecting the
test output from apparatus. Choose a small explicit code window; no default
voltage is assumed. Output tests leave the last code in place and do not call INIT.
Malformed ramp/overflow and driver fault-injection cases stay in the offline suite.
""")
code("""from live_bench import (Bench, verify_step_intervals, verify_init_waveform,
                        verify_bus_capture)
bench = Bench(
    host="",                       # e.g. the configured static board IP
    enabled=False,                 # opt in to real TCP connections
    outputs_isolated=False,
    allow_output_changes=False,    # separate opt-in to output changes
    channel=1,
    low_code=None,                 # choose approved codes, not voltages
    high_code=None,                # span 100..1000 for the ramp checks
    timeout=3.0,
)
flashed_revision = ""              # record manually; ID does not expose firmware version
print("Live tests enabled:", bench.enabled)
""")
live_cases = [
    ("Read-only continuity (Mem-1 soak supplement, C2, A1)", "bench.readonly(repeats=1000)", False),
    ("Silent second-client/socket stress (Mem-2, Robust-1, C3)", "bench.reject_extra_clients()", False),
    ("Reconnect after partial command (Mem-3)", "bench.reconnect_partial_command()", False),
    ("Invalid READ channels (C1)", "bench.invalid_channels()", False),
    ("Output register SET/READ", "bench.set_readback()", True),
    ("Second-client rejection during ramp (Robust-1, C3)", "bench.ramp_and_reject()", True),
    ("Ramp disconnect and lock release (C5, Robust-2)", "bench.abort_ramp()", True),
]
for title, call, changes in live_cases:
    markdown("### " + title + ("\nChanges output and leaves its final code in place." if changes else ""))
    guard = "bench.enabled and bench.allow_output_changes" if changes else "bench.enabled"
    code(f'if {guard}:\n    {call}\nelse:\n    print("NOT RUN: live test disabled")\n')
markdown("### Manual cable recovery (C4)\nEnable this separately: it prompts you to unplug and reconnect the cable, then verifies recovery using a fresh TCP connection.")
code('RUN_CABLE_TEST = False\nif bench.enabled and RUN_CABLE_TEST:\n    bench.cable_recovery()\nelse:\n    print("NOT RUN: manual cable test disabled")\n')
markdown("""## External measurement assertions

These are **not simulated measurements**. Fill in values from a scope/logic analyzer
and retain the original capture with firmware revision, instrument settings,
sample rate, and chosen acceptance limits. Empty data is reported as NOT RUN.
For M8, capture shared-bus activity during network and DAC work; include any SPI
interrupt workload added in future firmware. The M9 trace must
cover before INIT through settling; choose capture resolution suitable for glitches.
""")
code("""# H4: measured time between actual DAC updates, in microseconds.
step_intervals_us = []
requested_step_us = None
timing_tolerance_us = None
if step_intervals_us:
    verify_step_intervals(step_intervals_us, requested_step_us, timing_tolerance_us)
else:
    print("NOT RUN H4: supply measured step intervals and chosen tolerance")
""")
code("""# M9: scope samples spanning INIT, with application-approved bounds.
init_waveform_volts = []
approved_min_volts = None
approved_max_volts = None
if init_waveform_volts:
    verify_init_waveform(init_waveform_volts, approved_min_volts, approved_max_volts)
else:
    print("NOT RUN M9: supply measured INIT waveform and approved limits")
""")
code("""# M8: analyzed shared-SPI capture under the relevant interrupt workload.
bus_capture = None  # e.g. dict(overlapping_cs_frames=0, malformed_frames=0, tested_frames=1000)
if bus_capture is not None:
    verify_bus_capture(**bus_capture)
else:
    print("NOT RUN M8: supply analyzed bus-capture results")
""")

notebook = {
    "cells": cells,
    "metadata": {"kernelspec": {"display_name": "Python 3", "language": "python", "name": "python3"}, "language_info": {"name": "python", "version": "3.11"}},
    "nbformat": 4, "nbformat_minor": 5,
}
for i, cell in enumerate(cells):
    cell["id"] = f"dac-cell-{i:03d}"
path = Path(__file__).with_name("bug_checklist.ipynb")
path.write_text(json.dumps(notebook, indent=1) + "\n", encoding="utf-8")
print(path)
