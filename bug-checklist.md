# DAC firmware bug checklist

Target: Arduino Uno R4 Minima, W5500 Ethernet shield, four AD5780 DACs.
Updated 2026-10-16 after the network startup regression fix, in the priority order below.

All **36 software checklist cases pass offline**. The complete regression suite
has **81 passing tests**; the pinned Uno R4 target also builds successfully.
Checked items mean the stated software behavior is verified, not that physical
voltages, electrical faults, or timing have been measured. M8 was reviewed as
not a current defect. See [validation history](tests/VALIDATION.md) for earlier
results and repair commits.

Run the setup cell followed by any individual case in
[bug_checklist.ipynb](tests/bug_checklist.ipynb). Each ID below has its own case;
failures raise an assertion with details. The separate
[physical_controls.ipynb](tests/physical_controls.ipynb) covers B1–B21 and N1–N12.
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
  physical controller/cable recovery remains a bench check.
  - **Codex-introduced regression, fixed 2026-10-16:** the new health check
    treated LinkOFF immediately after PHY reset as a fault, disabling command
    servicing and repeatedly resetting a link that needed time to negotiate.
    Allow up to ten seconds for the first link, measured after initialization;
    once LinkON is observed, later loss still faults promptly. B21 failed before
    the fix and covers delayed startup/ID, permanent cable loss, retry bounds,
    initialization latency, timer rollover, configuration faults, and unchanged
    DAC codes. This is a confirmed software defect; its role in the reported
    live failure still needs USB/bench confirmation.
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
  bounds, cancellation, increments, and virtual timing. Actual timing needs measurement.
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
  zero before releasing its clamp. Incompatible active modes fail unchanged.
  Tests cover active, all-channel, cold, repeated, and incompatible configurations.
- [x] **M10: `atoi` overflow.** Use checked `strtol` parsing with explicit signed-32-bit
  limits; reject overflow/junk before any DAC access, including on wider host platforms.
- [x] **M11: RAMP reply arrives only after completion.** Return
  `RAMP ACCEPTED channel target step delay` after validation and before the first
  step. `STATUS` reports IDLE/RUNNING/COMPLETE/ABORTED/FAILED. There is no unsolicited
  completion line to corrupt a later query. **Clients must use the new protocol**;
  [the Python helper](Software/python/dac_client.py) polls completion by default.

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
  clean Uno R4 build passed on 2026-10-14. The 2026-10-16 build with the startup
  correction uses 73,304 bytes flash and 3,724 bytes static RAM.
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

NETWORK uses A1→GND; SAFE ZERO uses A2→GND, with pull-ups and 25 ms debounce.
SAFE ZERO moves all channels concurrently toward code 131072 at at most 0.01 V/s,
then stays latched until a fresh NETWORK press. NETWORK does not write DACs.
See [wiring and bench procedure](Software/Physical%20controls.md).

Offline tests do not prove analog accuracy, physical slew rate, bus electrical
behavior, or recovery of an SPI-unresponsive Ethernet chip. An isolated hardware
reset would require the shield schematic; its shared Arduino RESET must not be
used for an output-preserving network reset. The supplied W5500 marking alone
does not establish that wiring. These physical limits are separate from the
software checklist. No firmware upload or live output change was performed in
the full repair phase.

Test tooling also fixes separate source-digest stamps for the two host executables,
preventing stale binaries, and uses stdlib unittest so pytest is optional.
