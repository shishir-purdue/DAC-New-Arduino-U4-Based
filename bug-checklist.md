# DAC firmware bug checklist

Target: Arduino Uno R4 Minima, W5500 Ethernet shield, four AD5780 DACs.
Updated 2026-10-16 after calibration, ramp timing, recovery API and live acceptance work.

All **36 software checklist cases pass offline**. The complete regression suite
has **86 passing tests**; both pinned Uno R4 variants build successfully.
Checked items mean the stated software behavior is verified, not that physical
voltages, electrical faults, or timing have been measured. M8 was reviewed as
not a current defect. See [validation history](tests/VALIDATION.md) for earlier
results and repair commits.

Run the setup cell followed by any individual case in
[bug_checklist.ipynb](tests/bug_checklist.ipynb). Each ID below has its own case;
failures raise an assertion with details. The separate
[physical_controls.ipynb](tests/physical_controls.ipynb) covers B1–B26 and N1–N12.
[Test instructions](tests/README.md) include the full offline command and optional
bench checks. All A items are explicitly identified as Codex additions.

## Preliminary fixes

- [x] **Mem-1: Command heap allocation.** Existing command dispatch is allocation-free;
  source audit and 6,000-command allocation regression pass. No new firmware fix needed.
- [x] **Mem-2: Second-client socket leaks.** Accept each new socket once, including
  silent connections, and stop rejected/disconnected sockets. Also fixes A1.
- [x] **Mem-3: Disconnect cleanup.** Stop the client, reset its handle, and discard
  partial input before accepting a replacement connection.
- [x] **Robust-1: Single-client enforcement.** Reject extra clients both while idle
  and during ramps without disconnecting the owner.
- [x] **Robust-2: DAC operation lock.** Connection events cannot release `dacBusy`;
  only the owning operation releases it. Busy DAC commands perform no SPI work.

## Critical — C1–C5

- [x] **C1: Invalid channel indexing.** Validate channels 1–4 before dispatch;
  invalid channels cannot access DAC arrays or SPI.
- [x] **C2: Missing SPI startup.** Call `SPI.begin()` before DAC transactions.
- [x] **C3: Second client crashes/hangs.** Covered by Mem-2 and Robust-1, including
  connection attempts during a ramp.
- [x] **C4: Network loss leaves stale state.** Check link and MAC/IP configuration
  every 250 ms; invalidate failed connections and partial commands. Retry full
  network recovery at most once per five seconds without changing DAC codes.
  Defer resets during DAC operations and preserve the SAFE ZERO latch. The former
  claim that `Ethernet.maintain()` alone recovers static-IP links was incorrect:
  it handles DHCP leases. C4 tests recovery policy and fresh connections offline;
  E02 now verifies real controller configuration loss and automatic recovery through
  the main API; cable removal and an electrically failed controller are separate checks.
  - **Codex-introduced regression, fixed 2026-10-16:** the new health check
    treated LinkOFF immediately after PHY reset as a fault, disabling command
    servicing and repeatedly resetting a link that needed time to negotiate.
    Allow up to ten seconds for the first link, measured after initialization;
    once LinkON is observed, later loss still faults promptly. B21 failed before
    the fix and covers delayed startup/ID, permanent cable loss, retry bounds,
    initialization latency, timer rollover, configuration faults, and unchanged
    DAC codes. The user confirmed connection recovery after upload. N01/E02 now record
    successful reset/configuration recovery; cable negotiation was not electrically traced.
- [x] **C5: Ramp blocks servicing.** Ramp callbacks process bounded input batches,
  connection/network health, and buttons between steps and during waits of at most
  1 ms. Busy DAC commands are rejected immediately; informational commands work.
  Cancellation is checked again before each output write. C5 also runs M4, A4,
  and Robust-1 regressions.

## High — H1–H6

- [x] **H1: Shadowed SPI fallback settings.** Assign explicit 1 MHz, MSB-first,
  MODE1 settings to the global on initialization failure. The old global was
  default-constructed, not uninitialized; the shadow prevented the intended fallback.
- [x] **H2: Negative ramp step converted to unsigned.** Require step size 1–262143
  before dispatch, with a driver-side guard as well.
- [x] **H3: Negative delay converted to a huge wait.** Reject negative delays before
  DAC access. Maximum accepted delay is signed-32-bit microseconds.
- [x] **H4: Long unsliced microsecond waits.** Wait in slices no longer than 1000 us,
  servicing cancellation and I/O between slices. H4 and B2/B9/B19/B20 verify
  bounds, cancellation, increments, and virtual timing. R01 now records actual meter timing/path evidence; C02/L01 exercise long waits live.
- [x] **H5: EnableOutput declaration mismatch.** Match the `SPISettings*` parameter
  and mask the readback header before rewriting the control register. H5 checks
  the public call and resulting control bits without changing the DAC code.
- [x] **H6: Bare signs accepted as integers.** Require digits after an optional sign;
  reject malformed tokens before dispatch. Shared strict parsing also fixes M10.

## Medium — M1–M11

- [x] **M1: Silent owner retains connection forever.** Adopt a 120-second idle lease
  renewed by received bytes. Active DAC operations keep the lease alive so slow
  ramps can finish. Tests cover renewal, expiry, reconnect, and timer rollover.
- [x] **M2: Remote operator misses clamping warning.** SET/RAMP replies include
  `WARN!` and the actual clamped target when a valid integer exceeds DAC range.
- [x] **M3: Truncated command can execute.** Discard an entire oversized or
  NUL-containing line through its terminator; the following valid line recovers.
- [x] **M4: Command starvation during ramps.** Consume up to 64 bytes per service
  call, including ramp callbacks. Dispatch uses separate stack storage so nested
  input processing cannot overwrite the current command.
- [x] **M5: Include filename case mismatch.** Use canonical `AD5780.h`; remove the
  historical test-only lowercase shim.
- [x] **M6: Unused frame prefixes.** Use explicit three-byte MSB-first SPI frames
  starting at byte zero, without unused prefix bytes or host-endian casts.
- [x] **M7: Undefined template GPIO aliases.** Remove unused, unimplemented ADI aliases.
- [x] **M8: Suspected SPI interrupt contention — reviewed, not a current defect.**
  Application SPI access is serialized in the foreground; no application ISR
  uses SPI. Tests verify balanced transactions, audit ISR registration, and ensure
  cancellation hooks use only GPIO/RAM. Future SPI-using interrupts need a new review.
- [x] **M9: INIT overwrites an active output.** Verify normal active channels read-only.
  Only an already ground-clamped channel may be configured and verified at exact
  calibrated zero before releasing its clamp. Incompatible active modes fail unchanged.
  Tests cover active, all-channel, cold, repeated, and incompatible configurations.
- [x] **M10: `atoi` overflow.** Use checked `strtol` parsing with explicit signed-32-bit
  limits; reject overflow/junk before any DAC access, including on wider host platforms.
- [x] **M11: RAMP reply arrives only after completion.** Return
  `RAMP ACCEPTED channel target step delay` after validation and before the first
  step. `STATUS` reports IDLE/RUNNING/COMPLETE/ABORTED/FAILED. There is no unsolicited
  completion line to corrupt a later query. **Clients must use the new protocol**;
  [the main malab API](malab/instruments/ad5780.py) polls completion by default.
  The older Python helper retains nominal conversion for legacy use.

## Additional findings — Codex additions

- [x] **A1 — Active client mistaken for a second client.** `available()` could
  return the owner repeatedly while data remained. Using `accept()` fixes this
  with Mem-2. Repeated commands retain the same connection.
- [x] **A2 — INIT falsely reports success.** Validate control/data readbacks and
  propagate failure to the client. Fault injection covers corrupt reads and ignored
  writes; a cold channel's clamp is retained until initialization is verified.
- [x] **A3 — Missing reproducible build configuration.** Pin Renesas RA 1.9.0,
  Arduino core 1.6.0, compiler package 1.70201.0, and DFU tool 1.11.0 in
  `platformio.ini`; vendor the Ethernet 2.0.2 recovery fork. Configuration audit and
  clean Uno R4 build passed on 2026-10-14. The current production build uses 73,676 bytes flash and 3,820 bytes static RAM;
  the bench fault-test build uses 74,900 bytes flash and 3,924 bytes static RAM.
- [x] **A4 — Extra write after disconnect during delay.** Recheck cancellation
  after sliced waits and before the next DAC write.
- [x] **A5 — Ethernet driver waits block local controls.** Bound command/register/
  transmit waits, add cancellation hooks and a finite poll fallback, and clear
  chip/socket state during reset. N1–N10 exercise actual driver code with W5100
  and W5500 register doubles; N11/N12 cover W5500 PHY reset/readback failure.
- [x] **A6 — USB debug backpressure blocks recovery.** Drop diagnostics when TX
  space is unavailable; B11 verifies continued command/button servicing.
- [x] **A7 — Codex-introduced regression: SD left selected on Uno R4.** Configure
  OUTPUT before writing D4/D10 HIGH because this core clears the output latch on
  mode change. B14 reproduces that behavior. The user subsequently confirmed
  correct zero and no automatic +10 V readback; no electrical trace was captured.
- [x] **A8 — Invalid DAC replies become voltages.** Validate the register-address
  bits before READ or either ramp. Invalid starting frames prevent output writes;
  SAFE ZERO stays latched. B15–B18 cover malformed frames and valid full-scale
  boundaries. This detects specific frame errors, not every possible data error.
- [x] **A9 — Client ramp units.** Convert seconds to microseconds before rounding.
  With ten-code steps, 0.001/0.01 V/s now produce 762940/76294 us respectively.
  The client helper has five offline tests for conversion, validation, status
  polling, aborts, and timeouts. B19/B20 verify firmware step/delay behavior.

## Physical controls and remaining bench evidence

### Live findings — Codex additions, 2026-10-16

- [x] **A13 — Remote ramp is slow on real hardware.** The original 0.1 V/s run
  moved about 0.0785 V/s because per-step waits excluded service overhead.
  Elapsed-time scheduling fixes that delay accounting (`59db0a8`). R01 passes
  0.001/0.01/0.1 V/s in both directions with ten-code steps, and 1/100-code steps
  at 0.1 V/s, with measured paths/endpoints. Timing tolerance is 5% plus sampling
  and observation allowance; this is not a real-time guarantee under arbitrary load.
- [x] **A14 — Nominal voltage conversion misses ±0.5 mV.** Raw zero/+10 V were
  about +5.8/+4.4 mV high. The device now stores a measured CH1 offset/gain profile;
  the main API and physical SAFE ZERO share it. Independent V01 points pass the
  unchanged ±0.5 mV limit (maximum observed error about 0.050 mV in the first
  corrected run). CH1 zero code is 130996. Its physical minimum is about
  −9.99285 V: the API rejects an unattainable −10 V request. CH2–4 remain nominal.
- [x] **A15 — Codex-introduced: malformed calibration reply on the MCU.** The
  Uno R4 nano printf lacks `%lld`; the first CAL implementation produced invalid
  fields despite host tests passing. Allocation-free decimal formatting fixes it
  (`f37169b`). Live V01 reads the exact profile and checks resulting outputs.
- [x] **A16 — Fine-step ramp slows during STATUS traffic.** Seven separate
  W5500 sends per status response delayed one-code steps. A single framed send
  (`81ac8a4`) restores R01 timing/path acceptance. At 0.1 V/s the finest steps
  remain more sensitive to traffic than the default ten-code steps.
- [x] **A17 — Failed SAFE ZERO could look like successful completion.** Add a
  distinct FAILED state for invalid starting readback, exposed through the main
  API (`22d6d65`, malab `3aca28a`). E01 demonstrates failure/unchanged outputs on
  the real Arduino with substituted bad readback; S01 demonstrates successful zero.
- [x] **A18 — Codex-introduced test setup dependency.** New independent fault/
  lease/soak cells omitted INIT and could test a cold, clamped DAC after upload.
  Explicit per-cell initialization and baseline checks fix that (`malab a905e98`).
  Preserve the failed run; do not attribute its zero output to network/readback faults.

See [live coverage](tests/LIVE_COVERAGE.md) for the requirement-by-requirement
mapping and [fix/commit ledger](tests/LIVE_FIX_LEDGER.md) for measured evidence.

### Remote-driver findings — Codex additions, 2026-10-16

- [x] **A10 — The supplied AD5780 `get_voltage()` returns a raw-code string.**
  The new driver in the separate `malab` clone returns calibrated volts by default; `get_code()`
  provides the integer explicitly. Existing code must remove any extra conversion.
  The new offline notebook checks endpoints, zero and SET/READ agreement.
- [x] **A11 — Socket reads assume a whole reply and return `None` on failure.**
  The scoped AD5780/Keysight transport uses `sendall`, newline buffering, finite
  exchange deadlines and explicit exceptions. It closes uncertain sessions and
  never automatically replays output commands. Legacy drivers retain their old
  transport; this fix is scoped to the new drivers.
- [x] **A12 — Old ramp wrapper accepts invalid rates and uses incorrect time units.**
  The supplied `voltsource.py` ramp is replaced in the new AD5780 driver by finite
  positive V/s validation, microsecond conversion and ACK/STATUS completion checks.
  It retains channel-first arguments. This overlaps A9; offline tests additionally
  cover transport failures, abort/failure status and overall completion deadlines.

The nested clone contains 27 offline driver groups and the current
[visible acceptance notebook](malab/notebooks/live_acceptance.ipynb). Final
behavioral cases import `AD5780` through `malab.instruments.voltsource`. Their
commands/assertions are directly in cells, with executed notebooks and CSV data
saved per run. The earlier core-notebook draft now points to this calibrated suite.

### Physical verification

NETWORK uses A1→GND; SAFE ZERO uses A2→GND, with pull-ups and 25 ms debounce.
SAFE ZERO moves all channels concurrently toward their stored calibrated zero
codes (CH1 130996, nominal CH2–4 131072) at at most 0.01 V/s,
then stays latched until a fresh NETWORK press. NETWORK does not write DACs.
See [wiring and bench procedure](Software/Physical%20controls.md).

Offline tests do not prove analog accuracy, physical slew rate, bus electrical
behavior, or recovery of an SPI-unresponsive Ethernet chip. An isolated hardware
reset would require the shield schematic; its shared Arduino RESET must not be
used for an output-preserving network reset. The supplied W5500 marking alone
does not establish that wiring. These physical limits are separate from the
software checklist. Live uploads and output tests were performed on the authorized isolated bench.
CH1 has meter evidence; CH2–4 have register evidence. Button action emulation
does not establish electrical contacts/debounce or recovery of a damaged chip.

### Dashboard findings — Codex additions, 2026-10-16

- [x] **A19 — A cold output can accept SET/RAMP while still clamped to ground.**
  Observed after the dashboard firmware upload: register values changed but the
  meter stayed near zero. The dashboard now initializes only the selected channel
  before an explicit Set/Ramp, with validation before initialization. Connecting
  remains read-only; already active channels preserve their value during INIT.
  Driver callers still have the explicit `initialize()` API. The independent
  meter is displayed separately from register-derived values.
- [x] **A20 — Ethernet disconnect could stall the dashboard while closing USB.**
  Reproduced after NETWORK; a diagnostic stack identified Windows' USB CloseHandle.
  Network disconnect, ramp abort, and network transport/protocol errors now close
  only TCP, preserving independent USB recovery. Full `close()` still releases
  both transports. This avoids the observed close stall in the live control path;
  it does not claim to repair the underlying Windows USB driver. Offline group 27
  checks transport separation; dashboard D03/D04 cover the real recovery flow.
- [x] **A21 — Busy READ prevented live register monitoring during ramps.**
  New read-only `VALUES` telemetry reports four tracked codes and their source
  without touching SPI. `BENCH VALUES` supplies the same data while SAFE ZERO has
  disabled Ethernet. Unknown data is explicit; a completed write is not presented
  as checked readback. B27 and offline group 26 cover the protocol; dashboard D02
  compares live progress to the CH1 meter.

The dashboard notebook contains each test's commands and assertions directly.
The browser's visible layout is checked separately from HTTP state/action tests.

- [x] **A22 — Duplicate Windows localhost listeners could share a dashboard port.**
  A launcher probe from a restricted process failed to find the running server
  and started a second instance. Windows allowed both HTTP sockets to bind.
  New server starts use an exclusive socket; a regression rejects a second bind.
  The launcher probes IPv4 loopback explicitly and was verified to reuse the
  running server. The exclusive-bind fix applies at the next launch; the user's
  working dashboard session is preserved.

Test tooling also fixes separate source-digest stamps for the two host executables,
preventing stale binaries, and uses stdlib unittest so pytest is optional.

### Second-channel measurement — Codex addition, 2026-10-16

- [ ] **A23 — DAC 4 nominal calibration fails the ±0.5 mV analog requirement.**
  Tested as-is with the Keysight moved to DAC 4, unchanged firmware and nominal
  channel-4 profile. Eleven set/get points produce maximum request error
  **3.382 mV** and register-to-meter error **3.397 mV**. Stored zero code 131072
  measures about **+2.87 mV**, including after SAFE ZERO. Absolute ramp endpoints
  also fail. Relative ramp path/timing, network reset/hold, abort, safe-zero slew,
  all four stored zero codes, latch and rearm pass. A diagnostic fit suggests
  offset **+2.865 mV** and gain **0.9999388**, with maximum residual **0.0181 mV**;
  it was not applied. Calibration must also cover the device's independent
  safe-zero target; Python-only correction would not correct the physical button.
  See [DAC 4 evidence](tests/evidence/2026-10-16-dac4/). This remains open.
