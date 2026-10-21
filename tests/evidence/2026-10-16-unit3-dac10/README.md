# Unit 3 / DAC 10 acceptance — 2026-10-16 (America/Indianapolis)

Unit 3 was built and successfully flashed through COM7 using the generic
`uno_r4_minima_bench` environment with `DAC_UNIT=unit3`. DFU selected its unique
USB serial. The ignored local configuration supplies its identity and four
nominal calibration profiles; no firmware source edit or calibration transfer
was needed. The manifest records the binary, generated-header and source hashes.
All network identities in this evidence are redacted documentation values.

The dashboard now reads three ordered units, displays DAC 1–12, and assigns the
Keysight to **global DAC 10 = Unit 3, local channel 2**. Only Unit 3 has external
analog power. Units 1 and 2 remain network-accessible with unavailable DAC
readbacks. All recorded output/recovery jobs targeted Unit 3. These tests use
the dashboard's HTTP actions and state; no new browser visual check is claimed.

Every action and assertion is visible in
`malab/notebooks/unit3_dac10_acceptance.ipynb`. This directory also contains a
sanitized [executed notebook](executed.ipynb), [results](manifest.json),
[cell output](outputs.txt), [ramp plot](ramps.png), and CSV samples. Private
originals remain under ignored `malab/notebooks/results/`: run
`modular_run_20261017T023338Z`, data `modules_20261017T023342Z` (UTC timestamps).

| Check | Result |
| --- | --- |
| Physical 0 / +0.05 / 0 V preflight and matching device codes | PASS |
| V10: eleven set/get points, −9 to +10 V, ±0.5 mV accuracy | FAIL |
| R10: six ramps, three speeds, both directions, relative path/timing | PASS |
| R10-accuracy: absolute ramp endpoints within ±0.5 mV | FAIL |
| N10: network reset holds output; disconnect/abort; other units reachable | PASS |
| S10: safe zero at 0.02 V/s, all four stored codes, latch/rearm | PASS |
| S10-accuracy: physical zero within ±0.5 mV | FAIL |
| Final stored zero codes and all jobs scoped to Unit 3 | PASS |

The unchanged accuracy limit is **±0.5 mV**. Maximum request error is
**4.134382 mV**; maximum register/display-to-meter error is **4.149641 mV**.
A +10 V request measures **+10.00319006 V**. The final nominal zero measures
**+3.744997 mV**, with all four codes at 131072. Stored zero is not verified
physical 0 V. The other three channels have register evidence only.

Ramps at 0.001, 0.01 and 0.1 V/s take 5.59, 5.15–5.17 and 5.17–5.18 s for
their respective spans, passing the declared discrete-duration tolerance of
5% plus 0.6 s observation allowance. Measured and device paths are monotonic;
the absolute offset remains visible in the plot. Absolute endpoints fail.

SAFE ZERO from ±0.05 V takes **2.811 / 2.626 s**. Meter slopes are
**−0.01924173 / +0.02003242 V/s**, inside the predeclared measured band
0.019–0.0202 V/s. Both runs reach all four stored zero codes with TCP
disconnected, remain latched, and rearm through NETWORK. Physical endpoints
are **+3.736584 / +3.734619 mV**, failing absolute accuracy. USB button emulation
checks firmware action paths, not physical contacts or wiring. This establishes
the new-rate analog behavior on DAC 10; it does not retroactively validate the
previous failed DAC 5 preflight.

A diagnostic fit gives gain **0.9999532137**, offset **+3.739858 mV** and maximum
residual **0.012795 mV**. It suggests a gain/offset calibration issue and remains
**unapplied**. Calibration must cover both normal voltage conversion and the
firmware's independent safe-zero target. Bug checklist addition **A26** records
the open accuracy issue. The runner correctly exited nonzero for these failures.
