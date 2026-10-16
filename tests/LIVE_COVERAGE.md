# Live checklist coverage

Date: 2026-10-16. Bench: Uno R4 Minima/W5500/AD5780 box, MAC
A8:61:0A:AE:F2:01, IP 192.168.1.200, USB COM5. Keysight 34461A
MY60041761 at 192.168.1.71 measures CH1. Other outputs are isolated.

All behavioral cases call the class exported by
`from malab.instruments.voltsource import AD5780`. The actual test commands,
measurements and assertions are in
[the notebook](../malab/notebooks/live_acceptance.ipynb), not hidden in case helpers.
See [the ledger](LIVE_FIX_LEDGER.md) for failed runs, fixes and local commits.

## Coverage and evidence type

This table maps all 36 original software checks plus additions A10–A18. A live
pass validates the stated observable behavior on this bench. Compile/source
invariants and controlled error branches retain explicit supporting evidence;
physical faults that were not created are not represented as live passes.

| Checklist item | Acceptance case / supporting check | What the evidence establishes |
|---|---|---|
| Mem-1 | Q01 | 6,000 real commands, steady output/reconnect; allocation-free invariant additionally measured by host allocation counter. |
| Mem-2 | C01, C02 | 100 rejected extra clients, silent connection churn, owner remains usable. |
| Mem-3 | C01, E02 | Partial command discarded on disconnect and configuration-loss recovery. |
| Robust-1 | C01, C02 | Single owner while idle and ramping; rejected clients cannot set outputs. |
| Robust-2 | C02 | Busy SET/READ/INIT/RAMP rejected; owner ramp remains active. |
| C1 | F01, I01 | Invalid channels rejected by client/device, all four valid channels read/write isolated registers. |
| C2 | V01, I01 | Real SPI-driven SET/READ/INIT works after upload; startup ordering also audited/host-tested. |
| C3 | C01, C02 | Repeated extra connections do not hang owner or interrupt its ramp. |
| C4 | E02, N01 | Actual W5500 IP register loss recovers automatically; fresh connection, held outputs, aborted ramp. Physical cable/chip faults are not fully reproduced. |
| C5 | C02, R01, L01 | Commands serviced during ramps and long waits; cancellation/ownership/complete state observed. |
| H1 | Source audit + V01 | Fallback settings global assignment is a compile/source invariant; live nominal SPI path works. Initialization-failure branch remains host-tested. |
| H2 | F01 | Negative, zero and excessive steps rejected unchanged before operation. |
| H3 | F01 | Negative/overflow delay rejected unchanged. |
| H4 | C02, L01 | 20-second wait cancels promptly with no extra step; 125-second wait remains cooperative. Slice-size bound is host-tested. |
| H5 | Compile/host + I01 | Public EnableOutput signature and control mask tested on host; live INIT preserves/activates outputs. No standalone remote EnableOutput API is claimed. |
| H6 | F01 | Bare plus/minus rejected, next command works, outputs unchanged. |
| M1 | L01 | Idle session expires by 123-second observation; 125-second operation survives no-heartbeat period. |
| M2 | F01 | SET/RAMP clamping warning includes actual target; code and analog output checked. |
| M3 | F01, P01 | Oversized device input discarded, following ID works; main API rejects NUL/embedded terminators. Device NUL path additionally host-tested. |
| M4 | C02, R01 | Bounded command servicing during ramp, busy rejection and STATUS traffic; no command-storage corruption. |
| M5 | Build/source + V01 | Canonical include verified by audit/build; resulting firmware exercised live. |
| M6 | Source/host + V01, R01 | Three-byte frame construction audited and transaction-tested; actual codes/analog outputs agree. |
| M7 | Build/source + V01 | Unimplemented aliases removed; both targets compile; actual target runs. |
| M8 | Source/host + full live suite | No application SPI ISR; balanced foreground transactions and SPI-free cancellation hooks. Not an existing concurrency defect. |
| M9 | I01, V01 | Repeated/single/all-channel active INIT preserves codes/CH1 voltage; first V01 INIT after upload activates cold hardware. Incompatible modes/ignored writes additionally host-tested. |
| M10 | F01 | Signed-32-bit overflow and junk rejected unchanged, including values that wrap on narrower parsing. |
| M11 | R01, C02, L01 | Prompt ACCEPTED, readable RUNNING/COMPLETE/ABORTED; following READ stays aligned, no stray completion reply. |
| A1 | C01, Q01 | Owner survives repeated commands and extra-client rejection. |
| A2 | E01, I01 | Live INIT rejects substituted invalid frames; repeated valid initialization preserves outputs. Ignored write/cold-clamp faults remain host injection. |
| A3 | Both target builds + suite | Pinned board/core/toolchain and local Ethernet fork; upload logs and firmware hashes retained. |
| A4 | C02 | Disconnect during a 20-second step wait aborts before the first write; held code verified. |
| A5 | E02, N01, S01 + N1–N12 | Actual configuration loss and recovery actions run on hardware; stuck register/SEND waits and PHY readback failures are tested with real driver code/register doubles. No claim of an electrically dead chip recovery. |
| A6 | Q01, N01, S01 + B11 | Normal commands/actions work without a USB monitor; forced TX-backpressure branch is host-tested. |
| A7 | V01, R01 + B14 | Zero/full-scale actual readbacks and meter behavior pass; R4 GPIO ordering reproduced in host test. No SPI electrical trace captured. |
| A8 | E01, F01 | Substituted all-high readback produces errors, no ramp or safe-zero output changes; real full-scale remains valid. |
| A9 | R01 | Measured 10× speed changes, both directions and step sizes; expected microsecond delays computed visibly. |
| A10 | V01 | Numeric calibrated get_voltage agrees with independent meter; get_code remains explicit raw integer. |
| A11 | P01, E02, L01 | Actual device/transport errors and timeouts raise exceptions, close uncertain sessions; no None or output-command replay. TCP fragmentation separately covered offline. |
| A12 | F01, P01, R01 | Invalid rates rejected, V/s converted correctly, completion/transport deadlines distinguished. |
| A13 | R01 | Elapsed-time repair restores measured rate/path/endpoints under tested load. |
| A14 | V01, S01 | Independent calibrated points within ±0.5 mV; safe-zero uses same device profile; unattainable −10 V rejected. |
| A15 | V01 | MCU emits exact 64-bit calibration offset with portable formatting; client parses and uses it. |
| A16 | R01 | Fine-step ramps under repeated STATUS polling meet predeclared tolerance after one-send status reply. |
| A17 | E01, S01 | FAILED from invalid readback differs from successful LATCHED; both require explicit rearm. |
| A18 | Standalone cells + full Jupyter run | Explicit per-case initialization removes hidden test ordering dependency; failure history retained. |

## Acceptance limits

- CH1 absolute request-to-meter and register-derived-to-meter difference: **±0.5 mV**.
  This compares against this meter under current bench conditions; it is not a
  traceable calibration certificate or a claim about long-term thermal drift.
- Ramp: the programmed discrete step/delay duration, **5% plus measured sampling/
  observation allowance**; measured monotonic path/endpoints. The rate tests span
  0.001/0.01/0.1 V/s, both directions, default step 10, plus steps 1/100 at 0.1 V/s.
- SAFE ZERO: concurrent all-channel register targets, CH1 measured path/zero and
  <=0.01 V/s schedule. CH2–4 analog accuracy is not measured/calibrated here.
- NETWORK: codes and CH1 meter held, new connection usable, interrupted ramp not
  resumed. USB invokes the same action flags as the buttons; contacts/debounce
  are host-tested and require physical verification if wiring changes.
- No claim of recovery from CPU/DAC failure, an electrically blocked SPI bus,
  or a W5500 that cannot accept SPI. These physical failure modes cannot be
  proven by the emulated/configuration faults and are outside this bench setup.
- CH1 cannot physically reach exact −10 V with present references; the API
  rejects it. Its calibrated lower limit is about −9.99285 V.

## Run records

Final run **20261016T194431Z** passes all 13 live cases in Jupyter on firmware
`263b32d` and malab API/tests `a905e98`. The [manifest](evidence/2026-10-16/acceptance_manifest.json),
[outputs](evidence/2026-10-16/acceptance_outputs.txt), [upload provenance](evidence/2026-10-16/provenance.json)
and [measurement plot](evidence/2026-10-16/channel1_acceptance.png) are committed.

V01 maximum request error is **0.050435 mV**, +10 V measures **9.999982293 V**.
Default ten-code ramp durations differ by at most **1.28%**, including observation.
The finest one-code/0.1 V/s case is **6.2% slow including observation**, within the
declared 5% plus sampling/observation allowance; arbitrary traffic is not bounded
by this test. SAFE ZERO from ±0.05 V takes about **5.03/5.02 seconds** and reaches
measured zero within 20 µV. The complete executed notebook and failed historical
runs remain under `malab/notebooks/results/`. The evidence snapshot keeps the raw
baseline survey and timing failure as well as final data.

