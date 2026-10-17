# DAC-New-AD5780-Arduino-Uno-R4
4 channel DAC(New) box based on AD5780 and Arduino Uno R4 and custom circuit boards.

This version is designed to comply with maximum compatibility, where Python wrappers for *DAC-Old-Original-Arduino-Mega-Based* can directly operate on this hardware.

Note:
There are three different generations of DAC Box.
- DAC(Old): The first version, designed by Yisheng & Yupeng, archived version.
- DAC(New): The second version, similar to DAC(Old), using UNO R4 and rewrites the software. This iteration aims to maximize compatibility in software, with full Python package support, and it is directly swappable with the DAC(Old) hardware. **This is the current repo version**
- DAC(Linux): The third version, using Raspberry Pi and supports AD5791 & AD5780.


## Current software and bench validation

The firmware now uses the RAMP ACCEPTED/STATUS protocol and a per-channel CAL
profile. The updated AD5780 API lives in the separate `malab` clone and is exported
from `malab.instruments.voltsource`. Existing clients must migrate their ramp and
voltage conversion; the original broad compatibility description above predates
these protocol changes.

- [Bug checklist](bug-checklist.md)
- [Live requirement coverage](tests/LIVE_COVERAGE.md)
- [Fixes, commits and measured evidence](tests/LIVE_FIX_LEDGER.md)
- [Physical controls and optional bench firmware](Software/Physical%20controls.md)
- [Current visible acceptance notebook](malab/notebooks/live_acceptance.ipynb)

The `malab` folder is a separate ignored Git checkout, not bundled by this repo.
Both repositories' local commits are needed on the bench computer. No pushes
were performed for this validation work. CH1 is calibrated for this labeled box;
CH2–4 remain nominal. See the ledger for the actual hardware coverage limits.

## Local dashboard

Run `./malab/tools/start_dashboard.ps1` from this folder, then use Chrome at
**http://localhost:8765**. The modular dashboard shows per-unit network
health signals, live device values, the configured channel's Keysight measurement,
voltage/ramp controls, and USB recovery actions. See
[dashboard instructions](malab/dashboard/README.md) and its
[visible acceptance notebook](malab/notebooks/dashboard_acceptance.ipynb).
The firmware's `VALUES` command supplies ramp telemetry without SPI reads;
`BENCH VALUES` supplies safe-zero telemetry while Ethernet is disabled.

The dashboard now accepts an ordered IP list in the ignored `config/bench.local.json`.
Each unit adds four global channels, with its own connection and recovery
controls. The Python [DACArray API](malab/instruments/DAC_ARRAY.md) uses the same
mapping. The prior **DAC 4 (Unit 1)** functional checks pass, but
its unchanged nominal calibration fails ±0.5 mV accuracy (zero is about +2.87 mV).
See [DAC 4 evidence](tests/evidence/2026-10-16-dac4/) and the
[current visible notebook](malab/notebooks/modular_dac_acceptance.ipynb).
Global numbering is implemented in Python; each unit retains local channels 1–4.
Unit-specific firmware builds select the labeled MAC/IP and calibration profile.


Unit identities now live only in ignored local configuration.
The dashboard now maps it to **DAC 5–8**, with the meter physically on **DAC 5**.
Unit 1 remains the first configured unit; it has USB power only during Unit 2 tests.
See the [Unit 2 / DAC 5 notebook](malab/notebooks/unit2_dac5_acceptance.ipynb).
The `uno_r4_minima_unit2_bench` environment uses nominal calibration and selects
Unit 2 by USB serial for DFU uploads, independently of COM-port numbering.

See [local bench configuration](config/README.md) to add units, build/upload safely, and keep identities out of Git. Safe zero is now **0.02 V/s** after uploading each unit.
