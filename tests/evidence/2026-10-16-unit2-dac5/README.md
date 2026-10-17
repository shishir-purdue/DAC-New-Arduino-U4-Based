# Unit 2 / DAC 5 acceptance — 2026-10-16 (America/Indianapolis)

Unit 2 was flashed using `uno_r4_minima_unit2_bench`, selected by USB serial
`UNIT_2_USB_SERIAL`. It uses MAC 02:00:00:00:00:02,
IP 192.0.2.20, COM6, and nominal calibration. The manifest records the exact
binary/source hashes. Upload completed successfully. Unit 1 stayed on COM5
with USB power only and received no output/recovery commands; its four analog
readbacks were unavailable while its network continued to respond.

The Keysight 34461A at 192.0.2.71 was on **global DAC 5**, Unit 2 local CH1.
The user confirmed Unit 2 was ready in response to the isolated-output upload
question. Tests exercised only Unit 2. The dashboard has eight cards, per-unit
network/recovery controls and meter channel 5. HTTP tests exercise the same
server actions/state as those controls; this is not a new browser-visual check.

Source: `malab/notebooks/unit2_dac5_acceptance.ipynb`; every action/assertion is
in its cells. Executed notebook: `modular_run_20261017T013319Z`; raw data:
`modules_20261017T013324Z` under `malab/notebooks/results/` (UTC run IDs).

| Check | Result |
| --- | --- |
| Small physical 0 / +0.05 / 0 V preflight | PASS |
| V5: requested voltage and set/get versus meter | FAIL ±0.5 mV |
| R5: six ramps, monotonic relative paths and timing | PASS |
| R5-accuracy: absolute ramp endpoints | FAIL ±0.5 mV |
| N5: network reset hold, disconnect, stop; Unit 1 stays reachable | PASS |
| S5: independent safe-zero slew, four stored codes, latch/rearm | PASS |
| S5-accuracy: physical analog zero | FAIL ±0.5 mV |
| Final zero codes and all jobs scoped to Unit 2 | PASS |

Maximum request error is **2.459040 mV**; maximum register/display-to-meter error
is **2.535334 mV**. The +10 V request measures **+10.00245904 V**. Nominal zero
during the sweep is about +0.674 mV; final stored-zero reading is **+0.654471 mV**.
Do not interpret stored zero code 131072 as verified physical 0 V.

Ramps at 0.001, 0.01 and 0.1 V/s in both directions pass the predeclared discrete
timing tolerance of 5% plus 0.6 s sampling/observation allowance. SAFE ZERO from
±0.05 V takes 5.076/5.155 s; measured slopes are −0.00999528/+0.00998956 V/s.
Both runs reach all four stored zero codes and remain latched until NETWORK.
Measured safe-zero endpoints are +0.655827/+0.653945 mV. USB emulates the button
actions; this does not test the physical contacts or wiring on this box.

The diagnostic fit is gain **1.000188783**, offset **+0.664289 mV**, maximum
residual **0.024458 mV**. It is **not applied**. Calibration must include the
device's independent safe-zero endpoint; changing only the Python conversion
would leave the physical button's zero wrong. No accuracy claim is made for
DAC 6–8, which have register/zero-target evidence only.

CSV files contain the measured points and sample-timestamped traces; JSON files
retain initial/final/network/preflight states, fit and manifest. `ramps.png`
plots actual device and meter sample timestamps. Failures are preserved in the
notebook/manifest; the test runner correctly exits nonzero.
