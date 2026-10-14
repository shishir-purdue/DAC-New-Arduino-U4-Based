# Validation record — 2026-10-13

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
