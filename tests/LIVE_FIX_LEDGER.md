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
| BASE-USB | Preserve prepared optional USB action interface and host regression | firmware `ec00fad` | N01/S01 live passes plus B22; firmware uploaded |
| BASE-API | Preserve scoped socket/AD5780/Keysight drivers and notebook tests | malab `7cafaac` | 18 offline groups; meter identity/read succeeds |
| LIVE-1 | Ramp delay counts sleeping time but omits service overhead | firmware `59db0a8` | D02 and R01 main-API rate/path tests pass; see dated measurements |
| LIVE-2 | Analog voltage differs from nominal code conversion | firmware `92c0308`, malab `65253ab` | V01 passes through main API; maximum request error about 0.050 mV |
| LIVE-3 | Recovery actions need public AD5780 API access | firmware `22d6d65`, malab `3aca28a` | N01/S01 main-API live tests pass; calibrated safe-zero, both signs, latch/priority/rearm |
| LIVE-4 | Codex-introduced: nano printf does not support `%lld` in new CAL reply | firmware `f37169b` | Main API reads exact integer profile; V01 and D02 pass |
| LIVE-5 | Fine-step ramp slows under frequent STATUS polling | firmware `81ac8a4` | R01 passes after batching STATUS into one W5500 send; see timing caveat below |
| TEST-1 | Visible invalid-input, connection and cancellation cases | malab `6203d70` | F01/C01/C02 pass |
| TEST-2 | Controlled faults, leases, initialization and sustained commands | firmware `263b32d`, malab `70d2f5d` | B26 and E01/E02/L01/I01/Q01/P01 passes |
| TEST-3 | Codex-introduced independent-cell initialization omission | malab `a905e98` | Explicit INIT/baseline; corrected E01/E02 pass; Jupyter runner added |

## Live case coverage

The [coverage table](LIVE_COVERAGE.md) maps all 36 original IDs plus A10–A18.
Source-only H1/H5/M5/M6/M7/M8/A3 and fault-specific paths retain supporting
build/audit/fault evidence alongside observable live behavior. Historical pending
notes below record the state at each run; current results are appended at the end.

## Runs

Dated run identifiers below retain measured results and failures, including those
introduced during this work. All final behavioral calls use voltsource.AD5780.

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

### 20261016T192111Z — calibrated main-API acceptance

Firmware `f37169b` uploaded (74,356 bytes flash, 3,924 RAM). D02 passes at
2.031 s vs 2.006427 s. V01 passes all 11 independent validation points from -9 V
to +10 V, including small voltages of both signs and repeated zero. Maximum error
to requested voltage is about 0.050 mV. +10 V measured 9.999981683 V. An impossible
-10 V request raises ValueError and leaves the code unchanged. Calibrated CH1 zero
code is 130996; other three channels still have nominal, unmeasured calibration.

Recovery API integration adds `network_reset`, `safe_zero` / `safe_ramp`, and
`get_recovery_status` on `voltsource.AD5780`. The optional serial port stays
independent of TCP. A separate FAILED state distinguishes safe-zero readback
failure from successful completion. Offline suite: 85 outer tests, 24 API groups.
Live recovery and full checklist coverage remain pending.

### 20261016T192536Z — full rate matrix and recovery actions

Firmware `22d6d65` uploaded successfully. N01 passes: all DAC codes unchanged
after NETWORK, a fresh connection works, interrupted ramp remains at its held
intermediate voltage. S01 passes: independent USB action with TCP closed, both
signs, measured zero about +15 to +17 µV, 5.032/5.000 s vs 5.004/4.997 s expected,
NETWORK rejection while active, persistent latch and deliberate rearm. All four
final register codes match their calibration targets; analog evidence is CH1 only.

R01 passes ten-code steps at 0.001/0.01/0.1 V/s in both directions (duration ratios
1.00065–1.00477), but fails the one-code/0.1 V/s path under frequent STATUS traffic:
2.234 s vs 2.000586 s, with late path samples. The status handler performs seven
separate Ethernet sends. Batch it into one framed send; retain the failed trace.
The remaining R01 cases and complete checklist acceptance are still required.

### 20261016T193517Z — full ramp matrix passes

Firmware `81ac8a4` uploaded with captured DFU SUCCESS in
`tests/results/upload_81ac8a4.log`; SHA256 of the uploaded binary:
`d2368982942aa2c7cac64f73585850943bd0394dd8ea4325aff5846a523a346d`.
R01 passes all ten combinations and default blocking completion through
`voltsource.AD5780`. Ten-code steps at 0.001/0.01/0.1 V/s differ by at most 0.48%
from calculated duration. One-code steps at 0.1 V/s finish in 2.078/2.110 s vs
2.000586 s: within the predeclared 5% plus sample/observation allowance, with all
measured path/endpoints passing. This does not establish exactly 5% timing under
arbitrary traffic. Step-100 cases pass (about 1.7% including observation latency).

`20261016T192834Z` F01/C01/C02 passed on firmware `22d6d65`: input validation,
clamping warnings, 100 second-client rejections, silent churn, partial input,
busy ownership, prompt acceptance and disconnect cancellation. The notebook
cases/public fragment-write support are committed in malab `6203d70`.

Added bench-only READBACK/NETWORK/CLEAR fault controls for tests E01/E02.
READBACK substitutes invalid received frames without touching real DAC data;
NETWORK clears the real W5500 IP register so the normal health/recovery path must
recover. Production builds compile these controls out. Host B26 and 25 driver
notebook groups pass; the outer suite now passes 86 tests. Both builds pass.
These controlled faults do not simulate every possible electrical failure.

### 20261016T193640Z — initialization, leases and soak

I01/L01/Q01 pass on firmware `81ac8a4`. Seven repeated/single/all-channel INIT
operations preserved all codes and CH1 analog output. Idle connection expired
at the 123-second observation; a 125-second one-code ramp completed with no TCP
heartbeats and retained ownership at 127 seconds. 6,000 ID/READ/STATUS commands
finished in 7.31 seconds with no USB monitor, unchanged outputs and a successful
fresh connection. This soak is not a substitute for the allocation/source audit.

### 20261016T194233Z / 20261016T194324Z — controlled fault acceptance

Firmware `263b32d` uploaded successfully, binary SHA256
`5821fe6e23a2e7b5d3643ede42b12b0ebd80845b03a7298977a81e4e7dcb663f`;
log `tests/results/upload_263b32d.log`. Public fault API/tests are malab `70d2f5d`.
The first E01/E02 run failed because the new independent notebook cells omitted
INIT after uploading (the cold DAC remained clamped). This is a Codex-introduced
test setup error, not evidence that the fault altered outputs. Explicit INIT and
measured baseline checks were added in malab `a905e98`, including the other cells
that depended on earlier initialization. The failed run remains saved.

After correcting setup, E01 passes: READ, INIT and RAMP reject substituted
invalid SPI frames; SAFE ZERO reports FAILED and holds outputs; clearing the
fault and explicit NETWORK rearm restore access. Meter remained approximately
+0.019985 V throughout. E02 passes: clearing the actual W5500 IP configuration
invalidates the old connection, recovers automatically, discards a partial line,
interrupts a remote ramp, and holds other codes plus the CH1 measured output.
No NETWORK action is used to force recovery. Physical cable/chip electrical
faults are not inferred from this controlled software/configuration test.
P01 verifies completion/transport deadlines, explicit errors, closed uncertain
sessions, no automatic command replay and rejection of malformed framing.

### 20261016T194431Z — final Jupyter acceptance, all 13 cases pass

The actual Jupyter kernel used `.venv-bench/Scripts/python.exe`; every behavioral
case imported `AD5780` through `malab.instruments.voltsource`. V01, R01, N01, S01,
F01, C01, C02, I01, L01, Q01, E01, E02 and P01 all passed in one run on firmware
`263b32d`, with malab test/API revision `a905e98`. Documentation was being edited
while the suite ran; the manifest records dirty working trees. No firmware source
changed after the logged upload. The bench binary hash is retained in provenance.

- V01 maximum request error **0.050435 mV**; maximum GET-to-meter error
  **0.036611 mV**, both within the unchanged **±0.5 mV** limit.
- +10 V measured **9.999982293 V**; repeated zero measured **+0.000012092 V**.
- R01 ten-code durations differ from programmed durations by at most **1.28%**
  including observation latency. Fine one-code steps at 0.1 V/s take
  **2.125/2.110 s vs 2.000586 s**: about 6.2/5.5% longer including observation,
  within the declared 5% plus measured sampling/observation allowance. All path,
  monotonicity and endpoint assertions pass. Do not claim arbitrary-load precision.
- S01 reaches measured zero from both signs at **5.032/5.016 s** versus
  **5.004/4.997 s** expected; final meter about **−6/+19 µV**. All four zero codes,
  independence from TCP, priority, persistent latch and explicit rearm pass.
- L01 idle expiry and long-ramp ownership pass again. Q01 6,000 commands, I01
  output-preserving INIT, C01/C02 ownership/cleanup/cancellation and F01/P01
  validation/deadlines pass. E01/E02 controlled real-board faults pass.
- The final 25-group offline driver notebook also passes in Jupyter. 86 outer
  regressions pass; both target firmware builds succeed. Notebook schemas validate.

The [committed evidence snapshot](evidence/2026-10-16/) includes all CSVs,
manifest, text outputs, upload logs/hashes, calibration baseline and plot.
The full executed notebook remains at
`malab/notebooks/results/20261016T194431Z/executed.ipynb`.
The [coverage table](LIVE_COVERAGE.md) maps all original 36 IDs plus A10–A18 and
explicitly separates supporting source/fault evidence from physical measurement.
CH1 alone has analog evidence. Electrical switch contacts, cable failure and
SPI-unresponsive hardware have not been certified by this action-emulation suite.

The earlier nominal core notebook now points to the current acceptance notebook,
avoiding stale code-131072 zero assumptions. Documentation covers device
calibration, impossible −10 V, error semantics and the main recovery API.

Final read-only check: ID AD5780, recovery READY, codes
`[130996, 131072, 131072, 131072]` equal the four stored zero targets.
CH1 meter mean is **+0.000020131 V** (five readings); SCPI error queue clear.
See [final state](evidence/2026-10-16/final_state.json).

The final documentation/evidence commits update both READMEs, the checklist,
coverage table, physical-control instructions, notebook pointers and measured
snapshot. A complete local commit inventory at handoff is saved in
[local_commits.json](results/local_commits.json); it includes these final commits
without requiring a commit to contain its own hash. Nothing is pushed.

### 20261016T205855Z — local dashboard acceptance

The localhost dashboard runs on port **8765** and owns one DAC connection plus
an independent Keysight sampler. Four channels have device-code telemetry,
controls, and history; only CH1 has analog evidence. Network good requires a
fresh valid DAC reply and becomes stale without one. The user confirmed that
Chrome displayed all four cards, the meter, and the green signal. HTTP/Jupyter
tests verify the same action and state endpoints; they are not a pixel-level
browser test. The user subsequently confirmed that the controls work after refreshing.

Firmware commit **890e8bc** adds SPI-free `VALUES` and `BENCH VALUES` telemetry
with explicit readback/write/unknown sources. A completed write is not claimed
as measured voltage. The first DFU runtime upload failed with a USB status
timeout; a 1200-baud bootloader touch followed by the same PlatformIO upload
succeeded (75,380-byte bench image). Both firmware targets build; **87** host
regressions pass, including the new telemetry test B27.

The user's first Set/Ramp attempts exposed a cold output still clamped after
upload. The commands changed registers without changing the analog output.
Dashboard output actions now validate and initialize only their selected channel
before setting/ramping. Connect remains read-only. Initialization failure prevents
the output command. The unit tests cover this sequencing and rejection before
initialization. The real device's explicit initialization restored measured output.

D03 then reproduced a dashboard stall during disconnect after NETWORK. Diagnostic
stacks located it in Windows USB CloseHandle. Driver commit **c06fb4c** separates
network `disconnect()` from full `close()`: abort, reconnect and TCP errors keep
USB recovery available. This fixes that live control path, without claiming to
repair the underlying Windows USB driver. Full close still releases USB. The
offline notebook now passes **27 groups**, including telemetry parsing and USB
survival across disconnect/abort/timeout/protocol errors. Dashboard **9 unit tests**
and JavaScript syntax checks pass. Missing-token output requests are rejected.

The final visible dashboard notebook passes D01–D04 in a real Jupyter kernel:

- D01: 11 settings from −9 to +10 V and back to zero. Maximum request-to-meter
  error **0.056288 mV**; maximum displayed-register-to-meter error **0.062413 mV**,
  within **±0.5 mV**. CH2–4 codes remain unchanged during CH1 tests.
- D02: six up/down ramps at **0.001, 0.01, 0.1 V/s**. Actual programmed staircase
  durations are **5.340, 5.035, 5.005 s**; observed completion **5.408–5.413,
  5.169–5.211, 5.209–5.222 s**, respectively, including HTTP/device sampling.
  All timing, monotonic path, live telemetry and endpoint assertions pass under
  the declared 5% plus 0.6 s observation allowance. This is not hard real-time.
- D03: NETWORK holds all four codes and CH1 analog voltage; disconnect remains
  disconnected and values age; Stop ramp holds the intermediate CH1 output.
- D04: SAFE ZERO operates through USB with TCP disconnected from both signs.
  Measured slope magnitudes **0.0099987 / 0.0099854 V/s**; completion observed
  **5.173 / 5.125 s**. All four zero codes, persistent latch and NETWORK rearm pass.

Final test state: network good, recovery READY, codes
`[130996, 131072, 131072, 131072]`, meter **+0.000017768 V**.
The [evidence snapshot](evidence/2026-10-16-dashboard/) contains CSV traces,
the ramp plot, manifest, logs and final state. Full executed notebooks remain
in `malab/notebooks/results/dashboard_20261016T205855Z/`. The source notebook
contains the actual test actions/assertions, with future trace storage compacted
without changing those assertions. Checklist additions A19–A21 record the findings.
No changes are pushed.

A launcher check also exposed Windows allowing duplicate HTTP listeners on the
same port. New server starts use an exclusive socket; a regression checks that
a second server cannot bind. The launcher probes IPv4 loopback explicitly.
This startup fix takes effect at the next server launch; the working bench
server is left running to preserve the user's current session.

### Modular units and DAC 4 as-is — 20261016T212408Z

Added Python `DACArray`, exported through the existing voltsource module. Each
ordered IP owns four fixed global channels. Global channel 5 routes to local 1
on unit 2; an offline unit never collapses the mapping or reroutes an output.
Duplicate endpoints/USB ports are rejected. Connections and explicit broadcast
recovery return per-unit success/failure; commands are never automatically
replayed. Unit recovery requires an explicit unit number and preserves its
four-channel physical scope. USB recovery can interrupt a caller waiting for a
normal ramp. No firmware changes were made for modular support.

The GUI now groups four channels under each unit's IP, with independent workers,
fresh-response signals, pending commands and reset/stop/safe-zero controls.
Missing USB ports disable GUI recovery on only that unit. Stale/offline/busy or
latched units reject new output requests without queueing them for later, while
another unit remains usable. Request IDs cannot be reused across units/actions.
The selected global meter channel is explicit in `dashboard/units.json` and the
page; it is now **DAC 4**. The restart launcher refuses busy units, reloads the
configuration, and was verified to preserve all four codes and the meter mapping.

Seven visible offline notebook groups pass, including a blocked first worker
while the second continues handling outputs. Nine existing dashboard regressions
and JavaScript syntax/schema checks pass. These simulated multi-unit checks do
not substitute for the second physical box the user will supply later.

The user confirmed the Keysight was on DAC 4 and all outputs were isolated.
The current notebook tests that channel through the same GUI HTTP interface,
with unchanged firmware and nominal calibration. Results:

- **V4 FAIL:** 11 points from −9 through +10 V, then zero. Maximum request error
  **3.382058 mV**, maximum register-to-meter error **3.397317 mV**, exceeding the
  unchanged **±0.5 mV** criterion.
- **R4 PASS:** Six ramps at 0.001, 0.01, 0.1 V/s have monotonic relative paths,
  changing telemetry and completion within the declared 5% plus 0.6 s observation
  allowance. **R4-accuracy FAIL:** measured absolute endpoints remain offset.
- **N4 PASS:** Network reset preserves all four codes and measured output;
  manual disconnect remains disconnected, stale data ages, and abort holds an
  intermediate voltage.
- **S4 PASS:** With TCP disconnected, USB safe zero reaches all four stored zero
  codes, stays latched and rearms after NETWORK. Observed durations **5.586/5.127 s**;
  measured slopes **−0.009987/+0.010001 V/s**, within the declared sampling/noise
  allowance. **S4-accuracy FAIL:** analog zero is **+2.874/+2.871 mV**.
- Final stored codes **[130996, 131072, 131072, 131072]**, network good and recovery
  READY; CH4 meter **+0.002878393 V**. This is stored nominal zero, not analog zero
  within the required tolerance.

An unapplied diagnostic fit gives gain **0.999938805**, offset **+0.002864604 V**,
maximum residual **0.018043 mV**. This suggests calibration is needed, but it does
not establish performance after calibration. Physical safe zero needs a device
calibration/zero target; a Python-only offset would not repair that button's
analog endpoint. A23 remains open. The runner retains all failures and executes
the independent remaining cases and cleanup instead of hiding them.

[Committed evidence](evidence/2026-10-16-dac4/) includes CSV traces, states, plot,
diagnosis, logs and the unchanged firmware hash. Full executed live/offline
notebooks are under `malab/notebooks/results/modular_run_20261016T212408Z/` and
`modular_run_20261016T212316Z/`. The source notebooks contain all test code directly.
No changes are pushed.


## Unit 2 / global DAC 5 — 2026-10-16

Unit 2 was uploaded with its own MAC/IP and nominal calibration, selected by
USB serial so the attached Unit 1 was not flashed. IP 192.0.2.20 / COM6 maps
to DAC 5–8. The Keysight is on DAC 5, and the external supply is on Unit 2.
Unit 1 has USB power only; its unavailable analog readbacks do not block Unit 2.
Every output/recovery action in this run targeted Unit 2, checked from the jobs.

`unit2_dac5_acceptance.ipynb` contains all tests directly. Preflight, R5 timing/
relative paths, N5 network/reset/disconnect/stop and cross-unit network isolation,
S5 safe-zero behavior, and final stored zeros pass. **V5, R5-accuracy and
S5-accuracy FAIL the unchanged ±0.5 mV limit.** Maximum request/display errors
are 2.459040/2.535334 mV. SAFE ZERO slopes −0.00999528/+0.00998956 V/s and times
5.076/5.155 s pass; physical endpoints +0.655827/+0.653945 mV fail accuracy.
Final Unit 2 codes are [131072, 131072, 131072, 131072], meter +0.654471 mV.
The diagnostic gain/offset fit is **unapplied**. A25 tracks the calibration issue.
Only DAC 5 has independent meter evidence on Unit 2; DAC 6–8 have code evidence.
USB emulation tests action logic, not the Unit 2 buttons' physical contacts.

[Evidence](evidence/2026-10-16-unit2-dac5/) includes upload log, source/binary
hashes, pass/fail manifest, CSV traces, plot and initial/final/network states.
UTC run IDs are `modular_run_20261017T013319Z` and `modules_20261017T013324Z`;
the bench-local date is October 16. Nine dashboard regression tests also pass.


## Safe-zero rate and private configuration — 2026-10-16

The requested rate is now **0.02 V/s**. Both units were built and uploaded from
ignored local configuration with their original per-unit calibration preserved.
All 87 firmware regressions, 27 driver groups, seven multi-unit groups, local
configuration notebook checks and nine dashboard regressions pass. A fixture
third unit needs no source edit. [Current evidence](evidence/2026-10-16-safe-rate/)
records upload hashes and the physical preflight failure: the meter did not
follow DAC 5's small step, so **new-rate analog validation remains unverified**.
The test guard prevented the remaining output tests. Earlier rate measurements
remain 0.01 V/s history. Real network identities are redacted from public records;
the original local records remain in ignored results storage.
