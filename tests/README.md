# DAC checklist tests

Run `bug_checklist.ipynb` in Jupyter for one independently runnable cell per
checklist item. Run the setup cell first. `run_case("H2")` raises an assertion
showing what failed; known open bugs intentionally **fail**, not xfail/pass.
The summary cell catches failures so you can see all items in one run.

For repeatable regression checks, the same suite also works without Jupyter:

```powershell
python tests/checklist_tests.py --preliminary
python tests/checklist_tests.py --case H2
python tests/checklist_tests.py --report tests/results/current.json
python -m pytest tests/test_checklist.py -q
```

The default full run exits nonzero while open bugs remain. `REVIEW` is an
unverified bench requirement, never a pass. pytest displays these as skips.
For routine validation of this first repair phase, use `--preliminary`.

## Requirements and scope

- Offline runner: Python 3.9+ and a C++17 compiler. Windows automatically finds
  installed MSVC C++ Build Tools via `vswhere`; Linux/macOS use g++ or clang++.
- Notebook UI: Jupyter/ipykernel. `pytest` is optional. There are no Python
  dependencies for the standalone CLI or live socket helpers.
- The host executable compiles **the actual repository C++ files**, including
  the DAC driver and SPI communication implementation, against small test
  doubles. No firmware source is rewritten or patched for these tests.
- MSVC on this machine uses 32-bit `int`/`unsigned long`, like this target.
  On LP64 hosts unsigned long is wider: H2/H3 test rejection behavior, not an
  exact numerical wraparound/time prediction. This is not an MCU emulator.
- A test-only lowercase header shim lets case-sensitive hosts run unrelated
  tests; M5 separately detects the real production include mismatch.
- Each host case is a fresh process. Delays advance a virtual clock rather than
  sleeping. Runaway loops are bounded by 10,000 delay calls and a 10-second
  process timeout. Dangerous numeric inputs are exercised only in this model.
- SPI doubles check selection, transaction boundaries, register writes, and
  readback. They do not simulate analog voltages, reset timing, interrupts,
  Ethernet PHY behavior, TCP retransmission, or actual RAM fragmentation.
- Mem-1 checks allocation APIs in application source and counts C++ allocation
  calls during 6,000 command dispatches. It does not prove all library/MCU heap
  behavior. Mem-2 simulates finite sockets and silent clients, not Wiznet silicon.
- Source audits fail on a concrete current code pattern. They are not formal
  correctness proofs and should be updated if implementation structure changes.

## Interpretation of checklist claims

C5 covers the implemented disconnect callback only; M4 and A4 demonstrate
remaining responsiveness/abort limitations. H4 checks a proposed bounded wait
policy; exact timing accuracy requires physical measurement. M1 uses an explicit
**proposed 120-second idle timeout**, not an existing protocol promise. M9 tests
a proposed preserve-output policy and detects the existing midscale write; an
actual undesirable analog transient requires an oscilloscope and an agreed
startup policy. These policy tests are intentionally distinct from confirmed
implementation errors.

C4 cannot be established by the presence of `Ethernet.maintain()`. The official
[Ethernet implementation](https://github.com/arduino-libraries/Ethernet/blob/master/src/Ethernet.cpp)
uses it for DHCP leases; this firmware configures a static IP. Its broader
cable-recovery claim stays REVIEW until the bench test is run.

The socket double follows the official
[server implementation](https://github.com/arduino-libraries/Ethernet/blob/master/src/EthernetServer.cpp):
`available()` can return the same socket repeatedly when it has pending data;
`accept()` transfers a new connection once, including silent connections. The
firmware fixes require an Ethernet library with `EthernetServer::accept()`
(the standard Arduino Ethernet 2.x API). The repository does not pin a version.

M8 verifies transaction framing offline but reports REVIEW for interrupt
contention. No application SPI ISR is present; missing `usingInterrupt()` alone
is not evidence of a current failure. If an interrupt handler is introduced,
test the real core/library behavior and capture shared-bus traffic.

## Optional live bench checks

The notebook leaves all live tests disabled. Set your host and explicitly enable
the bench connection. Output-changing tests have a separate opt-in and explicit
test channel/code range. The helpers never upload firmware, initialize all
channels, or send negative/overflowing ramp parameters to hardware. They close
their PC sockets in `finally`/context managers and impose receive deadlines.
Closing a socket is not a guaranteed immediate stop on the current firmware.

Before output tests, disconnect the tested outputs from apparatus. The tests
intentionally change a channel and leave its last code in place; they do not
silently restore an unknown or potentially unsafe previous voltage. `READ`
checks a digital register, not a measured voltage. Build/flash the desired
revision separately; notebook results otherwise describe whatever firmware is
already on the board.

H1/H5/M5/M6/M7/A3 are best tested offline. H2/H3/H6/M3/M10 malformed inputs should
also stay offline until those bugs are repaired. C4 requires manual cable
unplug/replug. H4/M8/M9 require external timing/bus/analog evidence; the notebook
provides assertion cells for entered measurements rather than inventing results.
