# Live validation and fix ledger

Goal started 2026-10-16. Do not push either repository. Final behavioral passes
must call the AD5780 class exported by `malab.instruments.voltsource`; subtests,
source checks and fault doubles are supporting evidence, not substitute live passes.

## Required outcomes

- Every main checklist item has a named acceptance case and evidence, including
  connection ownership/cleanup, validation, initialization, command servicing,
  timing, cancellation and error reporting. Compile/source-only invariants retain
  their explicit evidence type rather than claiming direct analog proof.
- CH1 SET/GET agrees with Keysight 34461A measurements to **±0.5 mV**, as confirmed
  by the user. Investigate/calibrate; do not widen this criterion to hide errors.
- Ramps follow the requested rate/path, reach the correct endpoint, and expose
  acceptance/completion/abort through the public API.
- NETWORK preserves outputs and restores a fresh connection. SAFE ZERO moves all
  channels toward zero at <=0.01 V/s, works independently of Ethernet and latches.
  Only CH1 has an analog meter attached; other-channel evidence is register based
  until they are measured physically. USB emulates actions, not switch contacts.
- Actual commands and assertions are visible in notebooks. Keep raw measurements,
  failed attempts, fixes, commits and final execution records. No pushes.

## Starting state / evidence

- Firmware repository HEAD: `e8fac0a`; previous local repair `97cdb73` corrects
  premature PHY reset during link negotiation. These commits are not pushed.
- Nested malab origin/main: `7c5f54e`. Local drivers/notebooks and optional USB
  bench firmware are prepared but uncommitted at goal start.
- Earlier offline evidence: 82 outer tests, 18 driver notebook groups, both Uno
  build variants pass. This does not establish live checklist completion.
- Latest direct SET: CH1 code 262143, meter mean +10.004376133 V. Other three
  codes unchanged. Main import at that time was `malab.instruments`, so repeat
  acceptance through the required `voltsource.AD5780` entry point.
- `tests/results/channel1_ramp_to_10V_0p1Vps.csv`: requested 0.1 V/s; readings
  0.800774 V at 10.125 s and 3.989699 V at 50.75 s imply about 0.0785 V/s.
  This ramp was intentionally aborted at the user's request, then SET to +10 V.
- Initial raw zero measured +0.005758032 V during that run. User reports the
  earlier output connection issue was fixed. Calibration is authorized.

## Fixes and commits

| ID | Finding / action | Commit | Verification |
|---|---|---|---|
| BASE-USB | Preserve prepared optional USB action interface and host regression | firmware `ec00fad` | Builds and B22 pass; live upload pending |
| BASE-API | Preserve scoped socket/AD5780/Keysight drivers and notebook tests | malab `7cafaac` | 18 offline groups; meter identity/read succeeds |
| LIVE-1 | Ramp delay counts sleeping time but omits service overhead | firmware `59db0a8` | Main-API D02: 2.031 s vs 2.00669 s; broader acceptance pending |
| LIVE-2 | Analog voltage differs from nominal code conversion | firmware `92c0308`, malab `65253ab` | Calibration stored; live test exposed formatter defect, see LIVE-4 |
| LIVE-3 | Recovery actions need public AD5780 API access | pending | Current USB helper is separate; integration and live test pending |
| LIVE-4 | Codex-introduced: nano printf does not support `%lld` in new CAL reply | pending | Live run rejected malformed reply before output command; explicit integer formatter added |

## Live case coverage

The acceptance notebook will assign cases to each checklist ID before any claim
of completion. At goal start **no comprehensive live case is marked passed**.
Source-only H1/H5/M5/M6/M7/M8/A3 and fault-specific paths require supporting
build/audit/fault evidence in addition to their observable live behavior.

## Runs

Append dated run identifiers, imported API path, code/firmware revisions, measured
results and remaining failures here as they become available. Keep the goal open
until the requirement-by-requirement audit is complete.

### 20261016T190817Z — baseline through the required API

`malab/notebooks/results/20261016T190817Z/` contains the executed notebook,
manifest and CSV data. D01 fails ±0.5 mV. Fitted CH1 volts/code is
0.00007628355320380396; volts at code 0 is -9.992845256292616. Maximum fit residual
is 11.94 µV; candidate zero code is 130996. The minimum hardware output is about
-9.99285 V, so exact -10 V is physically unattainable without changing references.
Do not silently clamp a calibrated -10 V request and call it accurate.

D02 fails: expected 2.00669 s, observed 2.766 s. Both cases end with a nominal-zero
SET through `voltsource.AD5780`; measured true zero is still uncorrected.

LIVE-1 repair replaces sleep accounting with an elapsed-time schedule. A whole
missed interval resets the schedule instead of causing a burst of overdue writes.
B23 failed before repair and passes after repair, including service cost, timer
rollover and long stalls. All 83 offline tests pass; both target builds pass.
Live retest and the final calibrated acceptance suite remain pending.

### 20261016T191234Z — elapsed-time repair uploaded

Firmware `59db0a8` uploaded successfully through DFU. The identical D02 public-API
case passes: expected 2.00669 s, observed 2.031 s (1.21% slower including polling).

### 20261016T191940Z — calibration protocol integration failure

Firmware `92c0308` built (74,180 bytes flash / 3,924 RAM) and uploaded successfully.
The real Uno R4 nano printf emitted `ld` for the new 64-bit offset, shifting later
fields; host printf had accepted it. D02 and V01 fail before any SET/RAMP because
the main API rejects invalid calibration. Replace `%lld` with an allocation-free
decimal formatter and retain this failed run. This is a Codex-introduced defect.
