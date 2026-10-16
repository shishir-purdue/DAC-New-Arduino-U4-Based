# DAC regression tests

For current hardware acceptance use
[malab/notebooks/live_acceptance.ipynb](../malab/notebooks/live_acceptance.ipynb).
It calls `malab.instruments.voltsource.AD5780` and shows each command/assertion
in the cell. The notebooks below are supporting host regressions and older
nominal diagnostic helpers, not the final calibrated acceptance suite.
See [LIVE_COVERAGE.md](LIVE_COVERAGE.md) and [LIVE_FIX_LEDGER.md](LIVE_FIX_LEDGER.md).


Run the setup cell in `bug_checklist.ipynb`, then any of the 36 independent
checklist cells. Each failing requirement raises an assertion with details.
`physical_controls.ipynb` adds individual B1–B26 and N1–N12 cases. Both notebooks
are committed with clean outputs and live tests disabled.

The same checks run without Jupyter or pytest:

```powershell
python -m unittest discover -s tests -p 'test_*.py' -v
python tests/checklist_tests.py --case H2
python tests/checklist_tests.py --report tests/results/current.json
python tests/checklist_tests.py --preliminary
```

The full suite has **86 tests**: 36 checklist cases, 38 button/driver cases,
seven live-helper tests using fake sockets, and five Python ramp-client tests.
All are expected to pass. Failures are never hidden as expected successes.
Overlap between checklist and dedicated cases is intentional. pytest can also
discover these unittest classes if installed.

## Requirements and scope

- Python 3.9+ and a C++17 compiler. Windows finds MSVC C++ Build Tools through
  `vswhere`; Linux/macOS use g++ or clang++. Python tests/CLI use only the stdlib.
- Jupyter/ipykernel is needed only for the notebook UI. Regenerate notebooks with
  `python tests/make_notebook.py` and `python tests/make_button_notebook.py`.
- Firmware tests compile actual application, DAC, and communication sources with
  hardware doubles. Network tests compile the actual vendored Ethernet socket/chip
  implementation with register/SPI doubles. No firmware is rewritten for testing.
- Host cases run in fresh processes. Delays advance a virtual clock; runaway work
  has finite delay-call and process limits. Each harness has its own source hash
  so changed source rebuilds the right executable.
- Numeric parsing explicitly enforces signed-32-bit limits even on wider hosts.
  The host program is not an MCU emulator. SPI doubles verify transactions,
  selection, register values, and injected faults, not analog outputs or electrical
  bus behavior. Virtual time excludes actual SPI/network execution cost.
- Mem-1 counts C++ allocations over 6,000 commands and audits application source;
  it does not prove every library/MCU heap behavior. Source audits detect specific
  patterns and must evolve when the implementation changes.

## Implemented policies and protocol

C4 checks static-IP link/configuration recovery and five-second minimum retry spacing.
B21 checks the ten-second first-link grace after controller initialization,
delayed startup/ID, permanent link absence, timer rollover, and configuration faults;
`Ethernet.maintain()` alone is not a link-recovery mechanism. M1 uses a 120-second
idle lease, suspended by active DAC work. C5/M4 service input during ramps and
reject busy DAC commands immediately; M3 discards malformed lines entirely.
H4 uses waits of at most 1000 us and pre-write cancellation. M9 preserves normal
active outputs and only initializes a cold, already-clamped channel at verified
zero. M8 was reviewed as not a current ISR-contention defect: application SPI
work is foreground-only and cancellation hooks never perform SPI operations.

RAMP now acknowledges acceptance immediately and exposes completion through
STATUS, with no unsolicited final reply. Use `Software/python/dac_client.py` with
your existing `dac.query()` connection. The helper converts V/s to microseconds,
polls completion by default, and raises on rejection/abort/timeout. Its timeout
does not itself stop the ramp; deliberately disconnect to abort. The connection
must have a finite per-query timeout. See `Software/Physical controls.md` for
the wrapper example and the separate fixed-rate SAFE ZERO behavior.

## Target build

```powershell
pio run -e uno_r4_minima -t clean
pio run -e uno_r4_minima
```

The root configuration pins platform/core/compiler/uploader versions and uses
the vendored Ethernet 2.0.2 recovery fork. A3 audits this configuration; the target
build is a separate check. Results are recorded in `VALIDATION.md`. Neither the
offline suite nor build commands upload firmware or connect to the instrument.

## Optional live bench checks

Notebook live tests are disabled by default. Configure the host and explicitly
enable connections; output-changing tests have a separate opt-in and explicit
channel/code window. Disconnect the tested outputs from apparatus first. These
tests leave the last code in place rather than restoring an unknown output.
They do not upload firmware or initialize all channels. Record the flashed
revision: otherwise a live result does not identify the source being tested.

Helpers enforce receive deadlines and close their PC sockets on exit. A socket
close requests ramp abort when firmware detects it; it is not an instantaneous
electrical stop. READ reports a digital register, not a measured voltage.
Malformed commands and register-fault injections stay in the offline suite.

The manual C4 cable test reconnects with a fresh socket after recovery. Timing,
INIT waveform, and shared-SPI capture cells accept real measurements with chosen
tolerances; empty measurements report NOT RUN. Physical button/controller tests
and analog validation remain supplemental to the software checks. See the wiring
and bench procedure in `Software/Physical controls.md`.
