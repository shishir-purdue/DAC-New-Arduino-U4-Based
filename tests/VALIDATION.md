# Validation record

## Network startup regression — 2026-10-16

The user reported no connection/reply, including after pressing NETWORK.
Read-only checks from this PC (192.168.1.80/24) confirmed the DAC's configured
MAC A8:61:0A:AE:F2:01 at 192.168.1.200. TCP port 23 accepted connections, but
neither a welcome message nor a response to `ID\r\n` arrived within the receive
deadlines. No DAC command that changes outputs was sent. The Arduino was not
present among the PC's USB serial devices, so no board diagnostics or firmware
revision could be obtained. The live cause remains unconfirmed.

Source inspection found a C4 regression: after PHY reset, LinkOFF during initial
negotiation immediately latched a network fault. The recovery cycle could keep
resetting the PHY before software resumed accepting clients. New B21 reproduced
this failure against the previous code. The fix gives first-link negotiation a
ten-second grace measured after initialization; subsequent loss of an observed
link still faults on the normal health check. Configuration corruption is never
hidden by the grace, and permanently absent links still cause bounded retries.

B21 now passes, including delayed startup/ID, established-link loss, permanent
cable absence, initialization latency, millis rollover, configuration corruption,
and unchanged DAC codes. The full offline suite passes **81/81 tests** (36
checklist, 33 button/driver, seven fake-socket helper, five ramp-client tests).
Both regenerated notebooks pass nbformat validation and code-cell syntax checks;
their saved outputs are empty and live tests remain disabled.

Uno R4 Minima build passed: **73,304 bytes flash / 3,724 bytes static RAM**, using
the existing pinned packages. The fix has not been uploaded or tested on the
physical instrument. USB diagnostics and a subsequent isolated bench check are
needed to determine whether it resolves this particular live failure.

## Full repair phase — 2026-10-14

Current result: **80/80 offline tests pass**, including **36/36 checklist cases**
(zero FAIL or REVIEW), 32 button/actual-driver cases, seven live-helper tests with
fake sockets, and five Python ramp-client tests. Overlapping coverage is intentional.
Run from the repository root:

```powershell
python -m unittest discover -s tests -p 'test_*.py' -v
python tests/checklist_tests.py --report tests/results/current.json
pio run -e uno_r4_minima -t clean
pio run -e uno_r4_minima
```

The suite uses stdlib unittest; pytest is optional. The clean target build passed
with Renesas RA 1.9.0, Arduino core 1.6.0, compiler package 1.70201.0 (GCC 7.2.1),
DFU tool 1.11.0, and vendored Ethernet 2.0.2 with recovery changes. Size: **73,096
bytes flash / 3,716 bytes static RAM**. No firmware upload, networked bench run,
or analog measurement was performed in this repair phase.

The checklist notebook now includes A5–A9 and the current repair policies. Both
notebooks are regenerated with empty outputs and live tests disabled. Both pass
nbformat schema validation and nbclient execution: **51 code cells** in the bug
checklist and **33 code cells** in physical controls, with zero cell errors. All
live connections/output tests stayed disabled. Executed copies and the 36-case
JSON report are kept locally under ignored `tests/results/`.

| Item/group | Repair commit | Behavior verified |
|---|---|---|
| C4 | `34c6410` | Link/configuration loss, retry spacing, fresh connections, preserved DAC codes/latch |
| C5/M3/M4/A4 | `e850f38` | Cooperative input/connection service, bounded parser, pre-write abort |
| H1 | `494e5eb` | Explicit global fallback SPI settings |
| H2/H3/H6/M10 | `74e5bd3` | Strict signed parsing and ramp bounds before DAC access |
| H5 | `88454f7` | Matching public signature and masked control-register header |
| M1 | `4760db0` | 120-second idle lease, renewal, long-ramp protection, rollover |
| M2 | `dc2063c` | Remote clamping warnings and actual target |
| M5/M6/M7 | `b73d9f1` | Canonical include, three-byte wire frames, removed template aliases |
| M8 | `57ba763` | Foreground SPI ownership; reviewed as no current ISR-contention defect |
| M9/A2 | `14c76c2` | Read-only active INIT; verified zero before cold clamp release; write/read fault detection |
| M11/A9 | `0613ae4` | Early RAMP acceptance, explicit STATUS, corrected V/s helper and polling |
| A3 | `69ac5ab` | Exact package pins, configuration audit, clean target build; stdlib test discovery |

The preceding physical-control commit `892906e` supplies H4/A4 sliced waits,
A5/A6 bounded Ethernet/USB behavior, and A7/A8 GPIO/readback protection. The full
suite rechecks those behaviors alongside the new repairs. Preliminary fixes and
C1/C2/C3/A1 remain covered. The obsolete lowercase test-header shim is removed.

RAMP's response contract changed: acceptance precedes output stepping, completion
requires STATUS polling, and no unsolicited completion reply is sent. Use the
included client helper. INIT preserves normal active outputs; incompatible active
modes fail unchanged. M1's 120-second idle lease is the chosen default policy.
Electrical/analog bench evidence and isolated Ethernet-reset wiring remain outside
the offline verification; a software pass does not establish those properties.

## Historical physical-controls snapshot — 2026-10-14

The remainder records earlier snapshots. Their failure counts and limitations
describe those revisions, not the current full repair phase above.

Latest readback correction, not flashed by Codex: 32 physical-control/driver tests pass (`test_buttons.py`),
including twelve tests compiling the actual local Ethernet `socket.cpp` and `w5100.cpp`
with fault injection at the register/SPI boundary. N1-N10 run on both W5100 and
W5500 transports; N11-N12 verify W5500 PHY reset/readback failure. The other twenty test the
actual firmware/DAC code with host doubles. No electrical or analog evidence is
claimed. The Uno R4 Minima target builds successfully with the vendored recovery
library (72,184 bytes flash, 3,680 bytes RAM). All five preliminary regressions still pass.
Each host harness now uses its own source-digest stamp so rebuilding one cannot
incorrectly mark the other harness's stale executable current.

The user reported channel 1 readback stuck at +10 V despite a different meter
reading after uploading the initial button firmware; network-button operation
was observed by the user. The R4 startup ordering defect (D4 left LOW) is confirmed
in source and reproduced by B14. B15-B17 failed before the fix: READ/remote ramp/
safe ramp accepted invalid starting responses. All now pass, along with B18's
valid full-scale boundaries. Earlier GPIO doubles omitted OUTPUT's LOW-latch
behavior; this test limitation has been corrected. The user subsequently confirmed
correct zero and no automatic +10 V readback. No SPI trace was captured. Codex did
not change a live output or upload firmware during this investigation.

Remote timing follow-up: B19/B20 pass with the unchanged ramp implementation.
For a 1003-code span and 1000 us per-step delay, step sizes 1/10/100 yield
1003/101/11 writes in both directions. Simulated explicit delays are
1,013,040 / 102,020 / 11,120 us, including the DAC driver's 10 us waits but
excluding actual SPI/network execution costs. For a 1000-code span, step size 10
and delays 10/100/1000/10000 us yield 100 writes and simulated explicit delays
2,010 / 11,010 / 101,010 / 1,001,010 us. The user supplied their Python wrapper:
it sent seconds as microseconds, and both 0.001 and 0.01 V/s truncated to a 1 us
delay. The corrected wrapper is saved in `Software/Physical controls.md` and
sends 762940 and 76294 us respectively. Physical timing with that wrapper remains
to be measured. No ramp algorithm or live output was changed for this investigation.

Current complete original checklist: **12 PASS, 16 FAIL, 3 REVIEW**. H4's bounded-wait
criterion and A4 now pass because responsive button cancellation requires sliced
waits and a pre-write recheck. A3 reports REVIEW because build configuration now
exists. Other open bugs are deliberately unchanged. The full checklist still
returns exit code 1 as intended.

Use `physical_controls.ipynb` for independently runnable button/driver cases.
Manual wiring, reset isolation limitations, and bench tests are documented in
`Software/Physical controls.md`. The historical record below describes the earlier
preliminary-only snapshot, not this new firmware.

## Historical preliminary snapshot

Firmware source at `3719821` (preliminary repair phase). This is host validation;
no Uno R4 build, firmware upload, networked DAC test, or analog measurement was run.

## Before and after

The host harness was first run against original firmware revision `77b22a1`.
Mem-1 passed already; the other preliminary cases reproduced failures before
their respective repairs. They passed after their individual commits:

| Item | Original result | Repair/test commit | Final result |
|---|---|---|---|
| Mem-1 | PASS; existing parser already allocation-free | `31ea91f` (test-only) | PASS |
| Mem-2 | FAIL: silent first connection not accepted | `ea03b70` | PASS |
| Mem-3 | FAIL: disconnect retained partial input | `c54907b` | PASS |
| Robust-2 | FAIL: connection cleanup released operation lock | `7ba7824` | PASS |
| Robust-1 | FAIL: second client not rejected during ramp | `3719821` | PASS |

## Complete checklist

- PASS: Mem-1, Mem-2, Mem-3, Robust-1, Robust-2, C1, C2, C3, C5, A1.
- FAIL: H1–H6, M1–M7, M9–M11, A2–A4.
- REVIEW: C4, M8.
- Total: **10 PASS, 19 FAIL, 2 REVIEW**. C5 is a partial-fix regression only,
  not proof of nonblocking ramps. H4/M1/M9 contain explicitly stated policy criteria.

The C++ harness compiled with MSVC 14.39 and executed the real application,
DAC-driver, and communication sources against test doubles. The compiler still
reports the known H1 shadowing warning and conversion warnings; those firmware
items were intentionally not repaired.

The clean Jupyter notebook passed `nbformat` validation and was executed with
`nbclient`, allowing expected test errors to remain visible. All **46 code cells**
ran: the 19 known failed individual cases and two review cases produced their
intended exceptions; no unexpected exception was observed. Every live cell stayed
disabled. An executed local copy is generated under the ignored `tests/results/`
directory; the committed notebook has clean outputs for your own run.

Seven separate tests verify the live helpers without network access: disabled
connection/output gates, fragmented/coalesced replies, failed-handshake cleanup,
EOF/timeout errors, context cleanup, and rejection of missing/bad measurements.

## Reproduce

```powershell
python tests/checklist_tests.py --preliminary
python tests/checklist_tests.py --report tests/results/current.json
python -m pytest tests/test_live_helpers.py -q
```

The complete checklist command intentionally exits 1 while open bugs remain.
Use `tests/bug_checklist.ipynb` for per-case errors and optional isolated bench
tests. Test doubles, source audits, policy checks, and bench evidence have different
limits; see `tests/README.md` before interpreting a PASS as hardware validation.
