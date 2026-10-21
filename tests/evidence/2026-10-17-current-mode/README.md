# Resistor/current mode — 2026-10-17

All ten visible offline cases in `malab/notebooks/resistor_current_tests.ipynb`
pass. They cover finite/positive resistance, signed mA conversion, calibrated
voltage limits, mA/s-to-device timing, persistence and physical-channel identity,
independent writers, command routing/idempotency, stale resistance rejection,
network/busy gating, meter-derived current, historical values, removal, and the
actual JavaScript controls running against a DOM double. No browser layout or
physical current measurement is claimed. Tests use temporary settings only.

Existing 27 driver groups, seven modular groups and nine dashboard unit tests
also pass. See [executed notebook](executed.ipynb) and [manifest](manifest.json).

The live dashboard was restarted only after both units were idle. All eight
output codes were 131072 before and after. Both networks report good; meter
mapping remains DAC 2. No output/recovery commands were submitted. All resistor
entries remain unset for the user to enter actual loads. Runtime identities and
resistances stay in ignored local files; no firmware was changed or flashed.

Calculated mA assumes the configured resistance is across the output. The
Keysight continues measuring voltage; it does not independently measure current.
Existing analog calibration failures remain open. Python voltage APIs preserve
their units; the new explicitly named current methods use mA and mA/s.
