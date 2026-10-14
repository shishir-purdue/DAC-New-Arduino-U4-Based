# Validation record — 2026-10-13

## Physical controls update — 2026-10-14

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
