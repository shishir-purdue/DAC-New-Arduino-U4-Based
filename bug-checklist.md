# DAC Platform Bug Checklist

> **Target:** Arduino Uno R4 + Ethernet Shield + 4x AD5780 18-bit DACs
> **Priority:** Reliability/robustness first. Industrial apparatus directly connected.
> **Tracking:** Check off each item as it is fixed and verified.

---

## Preliminary Fixes (Already Applied)

These were addressed during initial analysis before formal tracking:

- [x] **Mem-1:** `String` heap allocation on every command -> heap fragmentation over time
  - Replaced `String` comparisons with `strcmp()` on raw `char*` tokens (zero allocation)
- [x] **Mem-2:** Socket leak from unhandled second-client connections
  - `telnetServer.available()` now called every loop; second client rejected with `stop()`
- [x] **Mem-3:** `telnetClient.stop()` never called on disconnect
  - Added explicit `stop()` in disconnect handler
- [x] **Robust-1:** Single-client enforcement (second client auto-kicked)
- [x] **Robust-2:** Single-DAC operation lock (`dacBusy` flag gates INIT/READ/SET/RAMP)

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

- [x] **C4: No `Ethernet.maintain()` -> link drops never recovered**
  - Cable unplug/replug, transient link loss, PHY desync all go undetected
  - Board becomes unreachable after network glitch - matches "random freeze" symptom
  - **File:** `main.cpp` loop()
  - **Fix:** Call `Ethernet.maintain()` every loop iteration

- [x] **C5: `AD5780_ramp()` is a long blocking call that freezes all I/O**
  - During ramp (up to 262143 steps), loop() is completely blocked
  - Cannot detect client disconnect, accept connections, or maintain Ethernet
  - Socket RX buffer overflows, lost commands, unrecoverable link drops
  - **File:** `AD5780.cpp:254-294`, called from `main.cpp:410`
  - **Fix:** Add abort-callback to ramp; periodically check client/Ethernet state; abort on disconnect

---

## High (H1-H6) — Fix After Critical

Incorrect output or undefined behavior. Not yet addressed.

- [ ] **H1: Shadowed global `AD5780_spi_settings` in SPI init error path**
  - `main.cpp:79` - local declaration shadows global; global left uninitialized on failure
- [ ] **H2: Negative `step_size` silently becomes ~4 billion (unsigned overflow)**
  - `int p3` -> `unsigned long step_size` conversion; `-5` becomes 4,294,967,291
  - Instant uncontrolled voltage jump on industrial equipment
- [ ] **H3: Negative `del_t` becomes a ~71-minute freeze**
  - `delayMicroseconds()` takes `unsigned long`; `-1` -> 71 minutes per step
- [ ] **H4: `delayMicroseconds()` used for potentially large delays - inaccurate**
  - Only reliable for short durations; use `delay()` for ms-range waits
- [ ] **H5: Header/implementation signature mismatch on `AD5780_EnableOutput`**
  - `AD5780.h:98` missing `SPISettings*` parameter vs `AD5780.cpp:84`
- [ ] **H6: `isInteger()` accepts bare sign characters (`"-"`, `"+"`)**
  - `atoi("-")` returns 0, feeds `DAC_sync[-1]` -> out-of-bounds

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
- [ ] **M9:** `func_INIT` sets DAC to mid-scale during init (possible transient)
- [ ] **M10:** `atoi` overflow undefined for large values (use `strtol`)
- [ ] **M11:** RAMP debug/response sent AFTER ramp completes (client may be gone)

---

## Fix Order

1. **C1-C5** (Critical) - current phase
2. **H1-H6** (High) - next phase
3. **M1-M11** (Medium) - final phase
