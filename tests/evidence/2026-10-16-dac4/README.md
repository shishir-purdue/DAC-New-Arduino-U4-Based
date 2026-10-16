# DAC 4 as-is and modular-controller evidence

Hardware notebook: `modular_dac_acceptance.ipynb`, run
`modular_run_20261016T212408Z`; data `modules_20261016T212412Z`.
The full executed notebook is retained under `malab/notebooks/results/`.
All four outputs were isolated and the Keysight was physically on DAC 4.

**Absolute accuracy fails ±0.5 mV.** Maximum request error is 3.382058 mV,
maximum displayed-register error 3.397317 mV. Nominal zero and the physical
safe-zero target code measure approximately +2.87 mV. These failures are retained
in the notebook and manifest; the runner exits nonzero.

Functional ramp paths/timing, network reset/hold, disconnect/stop hold, safe-zero
slew, four stored zero codes, latch and rearm pass. The two safe-zero slopes are
approximately −0.009987 and +0.010001 V/s (sampling/noise allowance applies).
The diagnostic offset/gain fit is recorded but **not applied**. Firmware and
calibration were unchanged; the manifest retains the same firmware hash as the
earlier CH1 dashboard test. No analog accuracy is claimed for other channels in
this run. No second physical unit was tested.

- `voltage.csv`: 11 absolute SET/GET/meter comparisons.
- `ramps.csv`, `ramps_trace.csv`, `ramps.png`: six ramps at three speeds.
- `network.json`: reset, disconnect and abort states.
- `safe_zero.csv`, `safe_zero_trace.csv`: both signs, independent USB recovery.
- `diagnosis.json`: unapplied linear fit; `final.json`: stored zero codes/final meter.
- `manifest.json`: every PASS/FAIL, paths and unchanged binary hash.
- `offline.log`: seven passing multi-unit routing/failure groups using fake units.

The seven groups cover mapping, configuration rejection, partial failures,
independent operations and USB preemption, GUI dispatch/request deduplication,
per-unit stale/busy/latch isolation and independent worker threads. All fake
behavior and assertions are directly in `multidac_offline_tests.ipynb`.
