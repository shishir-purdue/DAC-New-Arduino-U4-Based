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
