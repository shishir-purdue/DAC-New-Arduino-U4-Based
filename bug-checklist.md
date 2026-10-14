# DAC Platform Bug Checklist

> **Target:** Arduino Uno R4 + Ethernet Shield + 4x AD5780 18-bit DACs
> **Priority:** Reliability/robustness first. Industrial apparatus directly connected.
> **Tracking:** Check off each item as it is fixed and verified.

> **Codex review addition — 2026-10-13:** Runnable coverage is in
> [`tests/bug_checklist.ipynb`](tests/bug_checklist.ipynb), with one cell per ID.
> See [`tests/README.md`](tests/README.md) for CLI/pytest and test limitations.
> Preliminary fixes below were verified against the real C++ source with host
> hardware doubles, **not on an Arduino or attached apparatus**. Original checked
> items are not evidence of hardware validation. Only preliminary firmware behavior
> was changed in this repair phase. New findings are explicitly labeled **Codex addition**.

---

## Preliminary Fixes (Rechecked and Repaired)

These were marked applied during initial analysis. Codex rechecked them on
2026-10-13; all five now pass their offline regression cases. Bench tests remain
optional and have not been run.

- [x] **Mem-1:** `String` heap allocation on every command -> heap fragmentation over time
  - Replaced `String` comparisons with `strcmp()` on raw `char*` tokens (zero allocation)
  - **Codex verification:** Existing implementation already passes. No firmware change;
    added source allocation audit and 6,000-command C++ allocation regression.
- [x] **Mem-2:** Socket leak from unhandled second-client connections
  - **Codex correction:** Use `telnetServer.accept()` to receive each new connection
    once, including silent clients; release rejected/already-disconnected sockets.
    `available()` could return the owner repeatedly (see A1).
- [x] **Mem-3:** `telnetClient.stop()` never called on disconnect
  - **Codex correction:** Keep explicit `stop()`, reset the client handle, and clear
    partial command position/content on disconnect. Verified reconnect after partial input.
- [x] **Robust-1:** Single-client enforcement (second client auto-kicked)
  - **Codex correction:** Share connection housekeeping between the main loop and
    existing ramp abort callback, so second clients are also rejected between ramp
    steps. Command processing remains blocking (M4); long step waits still delay service.
- [x] **Robust-2:** Single-DAC operation lock (`dacBusy` flag gates INIT/READ/SET/RAMP)
  - **Codex correction:** Connection/disconnection events cannot clear the operation
    lock; only the owning handler releases it. Reject takeover while an operation
    is still active. Verified busy rejection and normal/error/abort release paths.

---

## Critical (C1-C5) — Fix Now

### Repair phase (2026-10-14)

- C5/M3/M4: ramp callbacks now service bounded batches of input between steps
  and <=1 ms delay slices. Busy DAC commands are rejected immediately, not
  retained for execution after a ramp. Parser storage is independent of a nested
  callback; oversized/NUL-containing lines are discarded through the terminator.
  C5, M3, M4, A4, all preliminary regressions, and 32 button/driver tests pass.

- C4 implemented: foreground link and MAC/IP health checks every 250 ms, failed
  client/partial command invalidation, and full network recovery at most once per
  five seconds. No automatic reset during DAC operations or a latched SAFE ZERO.
  Host C4 covers link loss/return, lost configuration, absent hardware, fresh
  connections, and latch preservation. Physical cable/controller testing remains
  supplemental; software tests do not prove recovery of an SPI-unresponsive chip.

These cause crashes, freezes, or hardware risk. **Current phase.**

- [x] **C1: No bounds check on DAC channel index -> out-of-bounds array access**
  - `DAC_sync[param - 1]` indexed with no validation that param is in [1,4]
  - `SET 0`, `SET 5`, `SET -1`, `READ 99` all read/write out-of-bounds memory
  - Can toggle GLOBAL_RST/CLR/PWR_EN pins (7/8/9), corrupt memory, or hard crash
  - **File:** `main.cpp:296,323,367,410` (all handlers)
  - **Fix:** Add `isValidChannel()` validation in `processCommand()` before dispatch

- [x] **C2: `SPI.begin()` is never called**
  - Commented out in `Communication.cpp:137`, never called elsewhere
  - SPI hardware peripheral (SCK/MOSI/MISO) never initialized
  - Contributes to intermittent DAC communication failures
  - **File:** `main.cpp` setup()
  - **Fix:** Call `SPI.begin()` in `setup()` before any SPI operations

- [x] **C3: Second client connection crashes/hangs the system**
  - Resolved by preliminary fix Mem-2/Robust-1 (single-client policy with `stop()`)

- [x] **C4: No `Ethernet.maintain()` -> link drops never recovered**
  - Cable unplug/replug, transient link loss, PHY desync all go undetected
  - Board becomes unreachable after network glitch - matches "random freeze" symptom
  - **File:** `main.cpp` loop()
  - **Fix:** Call `Ethernet.maintain()` every loop iteration
  - **Codex review addition:** The call already exists, but the original recovery
    claim is not established. Arduino Ethernet `maintain()` services DHCP leases;
    this firmware uses static IP. It does not itself implement PHY/link recovery.
    Reopened for bench evidence, not repaired in this phase. Test `C4` reports REVIEW
    and the notebook includes a manual cable test. See the
    [official implementation](https://github.com/arduino-libraries/Ethernet/blob/master/src/Ethernet.cpp).

- [x] **C5: `AD5780_ramp()` is a long blocking call that freezes all I/O**
  - During ramp (up to 262143 steps), loop() is completely blocked
  - Cannot detect client disconnect, accept connections, or maintain Ethernet
  - Socket RX buffer overflows, lost commands, unrecoverable link drops
  - **File:** `AD5780.cpp:254-294`, called from `main.cpp:410`
  - **Fix:** Add abort-callback to ramp; periodically check client/Ethernet state; abort on disconnect
  - **Codex review addition:** Partial fix only. Test `C5` passes for disconnect
    callback/lock release, and preliminary Robust-1 now services new connections
    between steps. The ramp still blocks command parsing (M4), and a disconnect
    during delay can permit another write (A4). Reopened for the broader claim;
    the ramp algorithm itself was not changed.

---

## High (H1-H6) — Fix After Critical

Incorrect output or undefined behavior. Not yet addressed.

- [x] **H1: Shadowed global `AD5780_spi_settings` in SPI init error path**
  - Repair: explicitly assign 1 MHz, MSB first, MODE1 to the global on failure.
  - `main.cpp:79` - local declaration shadows global; global left uninitialized on failure
  - **Codex clarification:** The global is default-constructed, not indeterminate;
    the local fallback does not update it. The configured valid parameters do not
    currently enter this error branch. Test H1 is a source audit.
- [x] **H2: Negative `step_size` silently becomes ~4 billion (unsigned overflow)**
  - `int p3` -> `unsigned long step_size` conversion; `-5` becomes 4,294,967,291
  - Instant uncontrolled voltage jump on industrial equipment
- [x] **H3: Negative `del_t` becomes a ~71-minute freeze**
  - `delayMicroseconds()` takes `unsigned long`; `-1` -> 71 minutes per step
  - **Codex clarification:** Exact duration depends on the target core's argument
    type and implementation; the confirmed defect is missing negative-delay validation.
    The host test uses a virtual clock and never sleeps for this converted delay.
- [x] **H4: `delayMicroseconds()` used for potentially large delays - inaccurate**
  - Repair: bounded <=1000 us slices with cooperative input/network/button service;
    H4 and B2/B9/B19/B20 cover bounds, cancellation, and accumulated delays.
  - Only reliable for short durations; use `delay()` for ms-range waits
- [x] **H5: Header/implementation signature mismatch on `AD5780_EnableOutput`**
  - Declaration now matches; readback header is masked before the control write.
    H5 checks the public call and R/W-bit handling without any DAC code write.
  - `AD5780.h:98` missing `SPISettings*` parameter vs `AD5780.cpp:84`
- [x] **H6: `isInteger()` accepts bare sign characters (`"-"`, `"+"`)**
  - H2/H3/H6/M10 repair: strict signed-32-bit `strtol` parsing before dispatch,
    positive bounded ramp step, nonnegative delay, and driver-side guards.
    Boundary/overflow/sign/junk tests verify no SPI access on rejection.
  - `atoi("-")` returns 0, feeds `DAC_sync[-1]` -> out-of-bounds
  - **Codex clarification:** Existing C1 validation now rejects channel zero, so
    that historical out-of-bounds route is blocked. Bare signs still pass numeric
    validation in value/delay fields; test H6 fails on the parser itself.

---

## Medium (M1-M11) — Fix After High

Robustness, usability, and code quality improvements. Not yet addressed.

- [x] **M1:** No client disconnect timeout (ungraceful disconnect blocks new connections)
  - Policy: 120 seconds without received bytes while idle releases the socket and
    partial input. A running DAC operation refreshes the lease so long ramps are
    not cut off. M1 covers renewal, expiry, reconnect, long operations, and rollover.
- [x] **M2:** Clamping warning goes to Serial only; remote operator uninformed
  - SET/RAMP responses include WARN and the actual clamped target; M2 covers both bounds and operations.
- [x] **M3:** Input buffer overflow silently truncated (could form wrong command)
- [x] **M4:** Single-byte-per-loop read risks data loss during blocking operations
- [x] **M5:** Include filename case mismatch (`ad5780.h` vs `AD5780.h`)
- [x] **M6:** Dead code - `writeCommand[0]`/`registerWord[0] = 0x01` never transmitted
- [x] **M7:** Undefined GPIO macros (`GPIO1_PIN_OUT`, etc.) from ADI template
  - Legacy ADI portability cleanup: canonical header case, explicit three-byte
    MSB-first wire frames without unused prefix or host-endian casts, and removal
    of unimplemented GPIO aliases. Both transport helpers now start at byte zero.
    M5-M7 audits and all 32 actual-driver button/ramp tests pass.
- [x] **M8:** No SPI interrupt arbitration (Ethernet + DACs share bus)
  - Reviewed, not a current defect: application SPI access is serialized in the
    foreground; no application ISR uses SPI. M8 checks balanced transactions and
    audits ISR registration; B6 verifies the cancellation hook performs no SPI.
    Future SPI-using interrupts require a new ownership review.
  - **Codex review addition:** DAC transfers already use `beginTransaction()` /
    `endTransaction()`. No application ISR uses SPI in this source. The test checks
    balanced framing, then reports REVIEW for real interrupt contention; missing
    `usingInterrupt()` alone does not prove a current bug.
- [x] **M9:** `func_INIT` sets DAC to mid-scale during init (possible transient)
  - Policy: an active normal channel is verified read-only; no reset, clamp, or
    DAC-code write. A channel already ground-clamped at power-on is configured
    and verified at exact zero before releasing its clamp. Incompatible active
    modes are rejected unchanged. M9 covers active/all/cold/repeated/incompatible cases.
  - **Codex test note:** The offline test detects the code overwrite under a proposed
    preserve-output policy. Actual analog transient acceptability needs an agreed
    startup policy and scope capture; a midscale write alone does not prove a glitch.
- [x] **M10:** `atoi` overflow undefined for large values (use `strtol`)
- [x] **M11:** RAMP debug/response sent AFTER ramp completes (client may be gone)
  - After validating readback, reply `RAMP ACCEPTED` before the first step.
    `STATUS` returns RUNNING/COMPLETE/ABORTED/FAILED. No unsolicited completion
    reply can contaminate a later query. M11 tests acceptance, live status,
    completion polling, and disconnect state. The tested Python helper implements
    the new protocol and corrected V/s units; legacy callers must be updated.

---

## Additional Findings — Codex additions (2026-10-13)

- [x] **A1 — Codex addition: Active client mistaken for a second client**
  - `EthernetServer::available()` can return the same socket again while it has
    unread bytes. The previous loop then sent a busy error and stopped its owner,
    often before a multi-character command was fully read.
  - Fixed as part of preliminary **Mem-2**, not as a separate out-of-scope repair.
    Test A1 feeds multiple commands over one connection and verifies it remains open.

- [x] **A2 — Codex addition: INIT reports success without verifying initialization**
  - Control/data readbacks are checked before releasing a cold channel's clamp;
    failed control or DAC writes return ERR. No resets are used. A2 fault injection passes.
  - `AD5780_Init()` discards control-register readbacks and always returns 1;
    `func_INIT()` prints success without checking a result. A missing/misconfigured
    DAC can therefore appear initialized.
  - Test A2 injects corrupt register readback and expects an error instead of success.

- [ ] **A3 — Codex addition: No reproducible firmware build configuration**
  - No `platformio.ini` or Arduino CLI sketch profile is checked in; the target,
    core, and Ethernet library versions must be selected manually. This also makes
    the exact library contract used by a flashed board uncertain.
  - Test A3 audits configuration presence. A future configuration still needs a
    clean target build and dependency review before this item is closed.

- [ ] **A4 — Codex addition: Disconnect during a ramp delay permits another write**
  - The callback runs before `delayMicroseconds(del_t)`. If the client disconnects
    during that delay, the ramp writes another DAC value before checking again.
    A long delay also postpones connection handling for the full step wait.
  - Test A4 disconnects during the simulated step delay and detects the extra write.
    This remained outside the preliminary fixes; it is addressed by the physical-button follow-up below.

## Physical recovery follow-up — Codex additions (2026-10-14)

- **A5 — Codex addition: Ethernet driver can prevent button servicing.** Upstream
  2.0.2 has unbounded command/register/transmit waits. The local fork adds bounded
  waits and cancellation hooks, plus complete chip/socket reinitialization. Actual
  driver fault-injection tests N1-N10 pass; controller-specific bench recovery is
  still pending. An SPI-unresponsive chip may need an isolated hardware-reset wire;
  the shared shield/Arduino RESET line must not be used to preserve DAC outputs.
- **A6 — Codex addition: USB debug output can block local recovery.** The Minima
  core can wait indefinitely for USB TX space. Application/driver diagnostics now
  drop messages if they cannot fit immediately. B11 covers debug backpressure.
- **Requested physical controls:** A1 NETWORK and A2 SAFE ZERO, active-low buttons
  to GND. SAFE ZERO operates all channels concurrently at <=0.01 V/s, stays latched,
  and requires a fresh NETWORK press after completion to rearm remote commands.
  B1-B13 pass offline; physical button and analog tests remain pending.
- **A4 follow-up:** fixed in this newly authorized scope: slice remote ramp waits
  into <=1000 us segments and check cancellation again before the next write.
  A4 and H4's bounded-wait criterion now pass. Analog timing still needs measurement.
- **A3 follow-up:** PlatformIO configuration and a local Ethernet fork are present
  and the Uno R4 target builds. The original source-audit test still returns REVIEW.
- **W5500 follow-up:** NETWORK now also resets the PHY and restores auto-negotiation.
  N1-N10 run with both W5100 and W5500 SPI/register doubles; N11-N12 verify PHY
  recovery and failed readback. Isolated hardware reset still needs the shield schematic.
- **Test tooling addition (fixed):** host build hashes were shared between the
  firmware and network executables, allowing a stale executable after rebuilding
  only one harness. Each executable now has its own hash file.
- **A7 — Codex-introduced regression, fixed locally: SD left selected on Uno R4.**
  New startup code wrote D4 HIGH before configuring OUTPUT; the R4 core's
  `pinMode(OUTPUT)` clears the latch to LOW. That selected the shield's SD device
  on the DAC/Ethernet SPI bus. Set OUTPUT before HIGH for D4 and D10. B14 now
  models the actual core behavior and fails with the previous ordering. The user
  subsequently confirmed correct zero and no automatic +10 V readback; no SPI
  trace was captured to establish the electrical mechanism.
- **A8 — Codex addition, guarded locally: invalid DAC responses became voltages.**
  Masking a raw all-high response produced 262143 and could start either ramp
  near +10 V despite a different actual output. READ and both ramp paths now
  validate the returned DAC-register address before decoding. Invalid readback
  gives an error and prevents ramp writes; SAFE ZERO remains latched. B15-B18
  cover all-high/all-low/wrong-register responses and valid full-scale boundaries.
  This is frame validation, not analog verification or comprehensive error detection.
- **A9 — Client wrapper units (identified, corrected example supplied):** changing
  speed from 0.001 to 0.01 V/s sent the same 1 us firmware delay. The supplied
  Python wrapper computed seconds without multiplying by 1,000,000, then truncated
  to zero and substituted 1. The corrected example in `Software/Physical controls.md`
  sends 762940 / 76294 us respectively. B19/B20 verify firmware step/delay scaling;
  physical timing with the corrected client remains to be measured. SAFE ZERO is
  independently fixed at <=0.01 V/s and ignores remote ramp parameters.
- Current original-checklist result: **12 PASS, 16 FAIL, 3 REVIEW**, plus **32/32**
  dedicated button/driver cases passing. The historical results below describe
  the earlier preliminary-only phase. The user tested the initial button firmware;
  Codex has not flashed the latest readback correction.

## Test interpretation — Codex addition

- Each of the 27 original items and four additions has an independently runnable
  notebook case, with assertion details when it fails. Overlap is intentional.
- Expected current offline results: **10 PASS, 19 FAIL, 2 REVIEW**. The five
  preliminary items all pass; open bugs are not hidden as expected successes.
- M1 uses a proposed 120-second timeout; H4 uses a bounded-wait criterion; M9 uses
  a proposed preserve-output criterion. Confirm these policies before fixing them.
- Optional live tests cover connection continuity, silent-client/socket stress,
  reconnect cleanup, invalid READ channels, SET/READ, ramp rejection/abort, and
  manual cable recovery. H4/M8/M9 also have external-measurement assertion cells.
- No board was flashed or exercised during this review. Historical line numbers
  above refer to earlier revisions; use function names and notebook IDs to locate
  current behavior.

---

## Fix Order

0. **Preliminary items** — repaired/verified offline in this phase; separate local commits
1. **C1-C5** (Critical) - remaining work, including reopened C4/C5 claims
2. **H1-H6** (High) - next phase
3. **M1-M11** (Medium) - final phase
