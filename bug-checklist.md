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

- [ ] **C4: No `Ethernet.maintain()` -> link drops never recovered**
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

- [ ] **C5: `AD5780_ramp()` is a long blocking call that freezes all I/O**
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

- [ ] **H1: Shadowed global `AD5780_spi_settings` in SPI init error path**
  - `main.cpp:79` - local declaration shadows global; global left uninitialized on failure
  - **Codex clarification:** The global is default-constructed, not indeterminate;
    the local fallback does not update it. The configured valid parameters do not
    currently enter this error branch. Test H1 is a source audit.
- [ ] **H2: Negative `step_size` silently becomes ~4 billion (unsigned overflow)**
  - `int p3` -> `unsigned long step_size` conversion; `-5` becomes 4,294,967,291
  - Instant uncontrolled voltage jump on industrial equipment
- [ ] **H3: Negative `del_t` becomes a ~71-minute freeze**
  - `delayMicroseconds()` takes `unsigned long`; `-1` -> 71 minutes per step
  - **Codex clarification:** Exact duration depends on the target core's argument
    type and implementation; the confirmed defect is missing negative-delay validation.
    The host test uses a virtual clock and never sleeps for this converted delay.
- [ ] **H4: `delayMicroseconds()` used for potentially large delays - inaccurate**
  - Only reliable for short durations; use `delay()` for ms-range waits
- [ ] **H5: Header/implementation signature mismatch on `AD5780_EnableOutput`**
  - `AD5780.h:98` missing `SPISettings*` parameter vs `AD5780.cpp:84`
- [ ] **H6: `isInteger()` accepts bare sign characters (`"-"`, `"+"`)**
  - `atoi("-")` returns 0, feeds `DAC_sync[-1]` -> out-of-bounds
  - **Codex clarification:** Existing C1 validation now rejects channel zero, so
    that historical out-of-bounds route is blocked. Bare signs still pass numeric
    validation in value/delay fields; test H6 fails on the parser itself.

---

## Medium (M1-M11) — Fix After High

Robustness, usability, and code quality improvements. Not yet addressed.

- [ ] **M1:** No client disconnect timeout (ungraceful disconnect blocks new connections)
- [ ] **M2:** Clamping warning goes to Serial only; remote operator uninformed
- [ ] **M3:** Input buffer overflow silently truncated (could form wrong command)
- [ ] **M4:** Single-byte-per-loop read risks data loss during blocking operations
- [ ] **M5:** Include filename case mismatch (`ad5780.h` vs `AD5780.h`)
- [ ] **M6:** Dead code - `writeCommand[0]`/`registerWord[0] = 0x01` never transmitted
- [ ] **M7:** Undefined GPIO macros (`GPIO1_PIN_OUT`, etc.) from ADI template
- [ ] **M8:** No SPI interrupt arbitration (Ethernet + DACs share bus)
  - **Codex review addition:** DAC transfers already use `beginTransaction()` /
    `endTransaction()`. No application ISR uses SPI in this source. The test checks
    balanced framing, then reports REVIEW for real interrupt contention; missing
    `usingInterrupt()` alone does not prove a current bug.
- [ ] **M9:** `func_INIT` sets DAC to mid-scale during init (possible transient)
  - **Codex test note:** The offline test detects the code overwrite under a proposed
    preserve-output policy. Actual analog transient acceptability needs an agreed
    startup policy and scope capture; a midscale write alone does not prove a glitch.
- [ ] **M10:** `atoi` overflow undefined for large values (use `strtol`)
- [ ] **M11:** RAMP debug/response sent AFTER ramp completes (client may be gone)

---

## Additional Findings — Codex additions (2026-10-13)

- [x] **A1 — Codex addition: Active client mistaken for a second client**
  - `EthernetServer::available()` can return the same socket again while it has
    unread bytes. The previous loop then sent a busy error and stopped its owner,
    often before a multi-character command was fully read.
  - Fixed as part of preliminary **Mem-2**, not as a separate out-of-scope repair.
    Test A1 feeds multiple commands over one connection and verifies it remains open.

- [ ] **A2 — Codex addition: INIT reports success without verifying initialization**
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
    This remains outside the preliminary fixes.

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
