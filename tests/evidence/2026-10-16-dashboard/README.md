# Dashboard acceptance evidence

Run: `20261016T205855Z`, through the localhost dashboard's HTTP actions/state.
All four cases passed in Jupyter. The visible test code is in the nested clone's
`notebooks/dashboard_acceptance.ipynb`; full executed notebooks are retained in
its ignored `notebooks/results/dashboard_20261016T205855Z/` folder.

- `set_voltage.csv`: 11 CH1 set/get/meter comparisons, ±0.5 mV limit.
- `ramps.csv` / `ramps_trace.csv` / `ramps.png`: six ramps, three speeds, live
  device telemetry and independent CH1 readings.
- `safe_zero.csv` / `safe_zero_trace.csv`: both ramp directions, USB telemetry
  with TCP disconnected, all four zero codes and measured CH1 slope.
- `final_state.json`: all channels zero, CH1 meter about +18 µV, network good.
- `manifest.json`: binary hash, source revisions and validation scope.
- Logs include expected exceptions printed by negative offline tests; the
  final PASS lines distinguish those from an actual test failure.

The user confirmed the four cards, meter and green network signal were visible
in Chrome, and subsequently confirmed the controls work after refresh. Automated evidence verifies the HTTP data/actions, not browser pixels.
Only CH1 was measured. Button action emulation does not test physical contacts.
