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
failed**, including known bugs intentionally left open. **ReviewRequired means
physical evidence is still needed**, not a pass. No hardware is accessed by the
offline tests. The preliminary-only summary should pass after the first repair phase.

The CLI and optional pytest interface in `tests/README.md` are useful for repeatable
regressions. The host harness needs a C++17 compiler; it uses the actual C++ firmware
with hardware doubles. This does not substitute for compiling/flashing the Uno R4.

Run individual cells to inspect failures, or use `run_all()` to collect everything
without stopping at the first known issue. Do not enable live output tests while
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
markdown("## Complete checklist summary\nOpen items intentionally report FAIL; bench-only evidence reports REVIEW.")
code("all_results = run_all()\n")

details = {
    "Mem-1": "Counts C++ allocations across 6,000 dispatches plus source checks for allocation APIs. It does not measure real MCU heap fragmentation.",
    "C4": "The presence of maintain() cannot prove PHY recovery. Use the optional cable test below. The current static-IP firmware does not use DHCP lease renewal.",
    "C5": "Tests only the implemented disconnect callback. M4 checks command starvation; A4 checks disconnect during the step delay.",
    "H4": "Policy test: a one-second delay must not be a single blocking microsecond wait. Use physical timing measurements below for accuracy; an exact failure duration is core-dependent.",
    "M1": "Proposed criterion: release an idle/half-open owner within 120 seconds. Confirm the desired timeout policy before implementing a fix. The virtual clock avoids a two-minute real wait.",
    "M8": "Verifies balanced SPI transactions, then requires bench review. No application ISR uses SPI in the current source. Missing interrupt registration is not by itself a confirmed bug.",
    "M9": "Proposed preserve-output criterion intentionally fails on the existing midscale initialization. Decide the startup policy and capture the analog waveform before concluding that a transient is unacceptable.",
    "M10": "Huge integers are tested on the host only. C atoi overflow behavior is not portable; the required behavior is rejection before any DAC access.",
    "A1": "Codex addition: existing connection reclassified as a second client. This overlaps Mem-2 and is repaired in that preliminary fix.",
    "A2": "Codex addition: injected corrupt readback still produces an initialization-success reply.",
    "A3": "Codex addition: no checked-in target/dependency build configuration. A configuration's presence would still need a real clean build.",
    "A4": "Codex addition: disconnect after the abort check but during delay still permits one more write. This remains outside the preliminary repair scope.",
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
Malformed ramp/overflow cases stay offline because the current firmware accepts
them and can hang or alter outputs unexpectedly.
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
markdown("### Manual cable recovery (C4)\nEnable this separately: it prompts you to unplug and reconnect the cable while preserving the original TCP socket.")
code('RUN_CABLE_TEST = False\nif bench.enabled and RUN_CABLE_TEST:\n    bench.cable_recovery()\nelse:\n    print("NOT RUN: manual cable test disabled")\n')
markdown("""## External measurement assertions

These are **not simulated measurements**. Fill in values from a scope/logic analyzer
and retain the original capture with firmware revision, instrument settings,
sample rate, and chosen acceptance limits. Empty data is reported as NOT RUN.
Use the actual relevant interrupt workload when evaluating M8. The M9 trace must
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
