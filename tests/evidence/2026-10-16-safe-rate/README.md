# Safe-zero rate change — 2026-10-16

Both units were successfully uploaded with a nominal safe-zero ceiling of
**0.02 V/s**. The scheduler uses one code per 3815 us (nominal profile), increases
the period when calibration requires it, and never issues catch-up jumps.
Firmware regression tests cover full-range duration, per-code paths, calibrated
period, cancellation, latch/rearm and unavailable Ethernet. All 87 pass.
Both production-target bench builds and both serial-selected uploads succeeded.

Local-configuration, driver and seven multi-unit notebook groups pass; nine
dashboard regression tests pass. A third fixture unit is added by configuration
alone. Real identities/calibration are read from an ignored local file; generated
headers and binaries remain ignored. The dashboard has eight channels and meter 5.
Unit 1's measured calibration is preserved; Unit 2 remains nominal.

**Live rate validation is not established by this run.** Its physical preflight
requested 0, +0.05, then 0 V on DAC 5. The meter stayed near +0.46 mV instead of
following. The preflight assertion failed, so later safe-zero commands were
blocked by the test guard. The source notebook and raw executed run preserve
those failures. The user was asked to confirm the supply and meter connections.
No new calibration was applied. Unit 2 was left at stored-zero codes.

`preflight.json` preserves the readings; `manifest.json` records firmware hashes,
upload/offline status and failed live case results. The earlier 0.01 V/s physical
acceptance remains historical evidence and is not relabeled as a new-rate pass.
