# Physical recovery controls

Updated 2026-10-16. Target: Arduino Uno R4 Minima, W5500 Ethernet shield and
AD5780 extension board. The isolated bench has live CH1 meter evidence for NETWORK
and SAFE ZERO through the shared USB action path; CH2–4 are register checks.
The user has also tried physical buttons. Emulation does not prove electrical
contacts/debounce; see [live evidence](../tests/LIVE_FIX_LEDGER.md).

## Wiring

| Button | Arduino header pin | Other contact |
|---|---|---|
| NETWORK reset | **A1** (digital alias D15 / MCU P000) | **GND** |
| SAFE ZERO | **A2** (digital alias D16 / MCU P001) | **GND** |

Use normally-open, momentary contacts. Firmware configures `INPUT_PULLUP`: an open
button reads HIGH and a pressed button connects the pin to GND. No external
resistor is needed for short bench leads. Use Arduino/digital ground, not a supply
rail. For four-leg tactile switches, use terminals on opposite switched contacts;
two legs on the same side can already be internally connected.

The extension-board schematic marks A0-A5 unconnected. A1/A2 can be accessed at
the stacked analog header or its solder joints; check continuity to the Arduino
pin before powering the assembly. Do not assume an unlabeled test pad is A1/A2.
The Ethernet shield uses D10 and SPI; its SD slot uses D4. The firmware holds D4
HIGH to deselect SD. DAC select pins are D2/D3/D5/D6, and DAC reset/clear/power
controls use D7/D8/D9. Do not connect either new button to RESET, D7, D8, or D9.

Sources: [extension-board schematic](../Hardware/Motherboard/SCH_Schematic1_Motherboard.pdf),
[Arduino Minima pinout](https://docs.arduino.cc/resources/pinouts/ABX00080-full-pinout.pdf),
[Arduino Ethernet Shield 2](https://docs.arduino.cc/hardware/ethernet-shield-2/).
Confirm the physical shield variant before any separate Ethernet-reset wiring.

## NETWORK button

A stable press (25 ms debounce) cancels a running remote ramp at its current
code, discards the active client and partial command, clears every socket cache,
forces WIZnet chip reset/redetection, reapplies MAC `A8:61:0A:AE:F2:01` and static
IP `192.168.1.200`, and opens a fresh TCP port 23 listener. Old buffered commands
are not resumed. The TCP peer may notice closure only on its next operation or
timeout; reconnect rather than reusing that old socket.

For the confirmed W5500 controller, recovery also asserts/releases the separate
PHY software reset and restores full auto-negotiation, including recovery from
PHY power-down. PHY configuration readback must succeed before initialization
is accepted. Link negotiation can continue after the TCP listener is configured.
See the [W5500 datasheet, PHYCFGR, page 42](https://docs.wiznet.io/assets/files/W5500_ds_v110e-226ffec190c588b69f88d629789585e1.pdf).

No DAC registers, clear/reset pins, or DAC power pins are changed by NETWORK.
Holding the button performs one reset; release and press again for another try.
Outside SAFE ZERO, link or MAC/IP configuration loss also triggers automatic
recovery: health checks run every 250 ms and reset attempts are spaced at least
five seconds apart. The interrupted ramp is aborted, never resumed automatically.
After each reset, the first link has up to ten seconds to negotiate before another
reset is attempted. Once a link has been observed, loss is detected at the normal
250 ms health check. This avoids a startup reset loop. Register/configuration
faults and the physical controls remain active during the startup grace period.
No automatic attempt clears the SAFE ZERO latch. The button remains an immediate
manual retry/rearm path.
If Ethernet remains unavailable, normal network work stays disabled and SAFE ZERO
remains available. A typical reset includes a 560 ms shield startup delay.

This is a **controller software reset over SPI**, not an electrical reset or power
cycle. It recovers stale sockets and recoverable controller/software state. A chip
that cannot accept SPI commands may require an isolated hardware-reset line. Many
stacked shields share RESET with the Arduino; asserting that shared line would
restart the controller and could disturb the DACs. Do not use it for this feature.
The exact shield/reset wiring must be identified before adding that recovery path.
The supplied `W5500Dsm1271204` marking identifies the controller family but does
not establish the shield PCB's reset routing. A clear photo of the Ethernet
shield's top/bottom or its exact schematic is needed for isolated-reset wiring.

## SAFE ZERO button

A stable press interrupts remote operation and takes exclusive local control.
It reads all four DAC registers once and validates the returned register address,
then moves each channel toward its stored zero code: **CH1 130996; nominal
CH2–4 131072**. Positive outputs
decrease; negative outputs increase toward zero. Channels move concurrently, with
their SPI writes performed sequentially a few microseconds apart.

The requested rate is **0.01 V/s**. Each channel moves by one code (CH1 about 76.284 µV; nominal channels
76.294 µV) no more often than every 7630 µs. The interval increases if a channel
profile requires it to respect the rate ceiling. Scheduling overhead can slow it. There are no catch-up jumps after delays. Typical times:

| Initial magnitude | Approximate time to zero |
|---|---|
| 0.1 V | 10 seconds |
| 2 V | 200 seconds |
| 10 V | 1000 seconds (16 minutes 40 seconds) |

The routine does not call Ethernet, INIT, CLR, RESET, or power-cycle a DAC. It
requires working MCU execution, DAC SPI/readback, and the established offset-binary
configuration/reference voltages. A physically failed Ethernet device that holds
the shared SPI bus electrically cannot be overcome by firmware on that same bus.
Analog zero and the actual slew rate must be verified with a meter/scope.

If any starting readback has an invalid register address (including all-high or
all-low SPI responses), no channel is written. The controls stay latched and the
USB diagnostic reports `SAFE ZERO blocked: invalid DAC readback; latched.`
Resolve the readback problem before rearming/retrying. Register-address validation
detects these malformed frames; it is not a CRC and cannot detect every data-bit
error or establish the actual analog voltage.

SAFE ZERO wins if both buttons are pressed. NETWORK presses during the safe ramp
are ignored, not queued. At zero, the software remains latched and remote commands
remain disabled. **Release both buttons and press NETWORK again after completion**
to rearm remote control. Rearming does not change the zeroed DAC registers.

## Remote RAMP timing versus SAFE ZERO

An idle TCP connection expires after 120 seconds without received bytes; reconnect
after expiry. Active DAC operations refresh that lease so long ramps can finish.
`INIT` verifies a normally configured active channel without writing its output.
Only an already ground-clamped channel is configured and verified at zero before
releasing the clamp; incompatible active configurations return an error unchanged.

The existing remote command is `RAMP channel target_code step_size step_time_us`.
`step_size` is a positive integer number of DAC codes per write; `step_time_us`
is the delay before each write, in microseconds. One code is nominally
20/262144 V, so a step size of 10 is approximately 0.763 mV per write.

For a fixed start and target, the write count is
`ceil(abs(target_code - start_code) / step_size)`. The final step is shortened
to reach the target exactly. An already-reached target causes no DAC writes.
Nominal programmed waiting time is write count times `step_time_us`; actual
elapsed time also includes SPI, register reads, button/network checks, and client
transport. The existing code provides a per-step delay, not a precise wall-clock
slew-rate guarantee. At very short delays, that overhead may dominate.

SAFE ZERO is separate: its rate is fixed at <=0.01 V/s regardless of the last
remote RAMP parameters. B19/B20 verify remote step/delay scaling using the real
firmware with host doubles. Their simulated delays exclude hardware SPI/network
execution costs and Python/meter timing; they are not a physical timing measurement.

### Python wrapper with speed in V/s

The user's original wrapper calculated seconds per step and sent that integer
directly to firmware expecting microseconds. At both 0.001 and 0.01 V/s, truncation
followed by a minimum of 1 sent the same 1 us delay. Convert seconds to microseconds
before rounding. Copy this example into the notebook containing the existing
connected `dac` object; it does not create a connection itself.

```python
from pathlib import Path
import sys
sys.path.insert(0, str(Path("Software/python").resolve()))  # From the repo root.
from dac_client import ramp as ramp_dac

def ramp(channel, voltage, speed):
    return ramp_dac(dac, channel, voltage, speed, timeout=3600)
```

The implementation is in `Software/python/dac_client.py`; its five offline tests
cover units, voltage boundaries, malformed inputs, status polling, aborts, and
timeouts. At 0.001 V/s it sends 762940 us per step; at 0.01 V/s it sends 76294 us.

**Protocol change:** RAMP now replies `RAMP ACCEPTED channel target step delay`
after validating the starting readback and before the first step. This is an
acceptance, not completion. `STATUS` returns `RAMP RUNNING/COMPLETE/ABORTED/FAILED
channel target` (or IDLE before any ramp). There is no unsolicited completion
line. The helper polls STATUS until complete by default; `wait=False` returns
on acceptance. Its overall timeout is adjustable for slow ramps; the connection
must also use a finite per-query timeout. A helper timeout does not automatically
change outputs or disconnect; the ramp may still be running. Deliberately close
the connection to abort. S01 now records measured CH1 slew/zero evidence; CH2–4 need separate meter checks.

## Why the Ethernet library is local

`Software/lib/Ethernet` vendors upstream 2.0.2 plus small recovery changes.
Upstream socket-command, stable-register-read, and transmit waits can otherwise
loop forever. The local versions have 100 ms deadlines and a finite poll fallback,
and sample button cancellation while waiting. Hooks only access GPIO and RAM;
they never perform nested DAC/SPI work. Network failures latch the network offline.
The application uses static IP; DHCP and DNS are not part of the recovery path.

Remote ramp delays are sliced into at most 1000 us waits and checked again before
each DAC write. Callbacks also service up to 64 input bytes, dispatch informational
commands, and reject busy DAC commands immediately. Oversized or NUL-containing
lines are discarded entirely through the next newline. USB diagnostics drop messages when the TX buffer has no room,
instead of blocking local controls behind an unresponsive USB host.

These controls address software/network failures while the MCU and DAC path work.
There is no claimed recovery from a hung CPU, disabled interrupts, or damaged DAC
hardware.

## Verification and bench procedure

Run `python -m unittest discover -s tests -p test_buttons.py -v` for the 38 offline
button and actual-Ethernet-driver tests. `tests/physical_controls.ipynb` gives each
test its own cell. N1-N10 run against both W5100 and W5500 register transports;
N11-N12 cover W5500 PHY reset and failed readback. These are simulations, not
physical controller tests. Build with `pio run -e uno_r4_minima`.

### Readback regression correction (2026-10-14)

The initial button firmware wrote D4 HIGH before `pinMode(D4, OUTPUT)`. Unlike the
assumed GPIO behavior, the installed Uno R4 core configures OUTPUT with a LOW
output latch, leaving the shield's SD select asserted. The correction sets OUTPUT
first, then HIGH, before any SPI clocks. D10 uses the same corrected ordering.
This is a confirmed software defect. The user subsequently reported that zero is
correct and readback no longer automatically returns +10 V. This is user-reported
bench evidence, not a recorded SPI trace establishing the electrical mechanism.

READ and both ramp paths now check the AD5780's returned register-address bits
before decoding a voltage code. Invalid frames produce an error, rather than
turning an all-high response into code 262143. A legitimate full-scale code is
still accepted. B14-B18 reproduce the GPIO/readback failures and validate the fix.
The host GPIO double now models the R4 OUTPUT-latch behavior that earlier tests
missed, and DAC replies include the address bits specified in datasheet Table 9.

After uploading the correction, check `READ 1` against the meter before trying a
ramp. If it reports `ERR! DAC readback invalid; voltage unknown.`, inspect the USB
diagnostic's raw readback value and the SPI wiring/selection; do not substitute a
guessed starting voltage. The separate malab clone's `voltsource.AD5780.get_voltage` raises on `ERR!`
and uses the device calibration; it never turns an error into a numeric voltage.

Sources: installed Arduino Renesas core 1.6.0 `cores/arduino/digital.cpp` and
`r_ioport_api.h`; [AD5780 datasheet, Table 9](https://www.analog.com/media/en/technical-documentation/data-sheets/ad5780.pdf).

Before a firmware upload, arrange the bench so a controller restart is acceptable.
Uploading itself is not the NETWORK-button operation and does not have its
preserve-output guarantee.

1. With test outputs connected to a meter/scope, upload and initialize the channels
   using the normal procedure. Use small test voltages of both signs initially.
2. Record all four outputs. Press NETWORK while a TCP client is open and with a
   partial command queued. Reconnect to the same address; verify unchanged output
   voltages/register codes and that the partial command was discarded.
3. Start a remote ramp with a long step delay, then press NETWORK. Confirm no
   further ramp steps after the button is recognized and no return to the target.
4. Put channels above and below zero, unplug Ethernet, and press SAFE ZERO. Measure
   all four channels moving toward zero at approximately 0.01 V/s without a jump.
5. Press NETWORK during that ramp; it must not interrupt or rearm it. After zero,
   release both buttons, press NETWORK, reconnect, and read all four zero codes.
6. Test simultaneous presses, held buttons, and recovery after a missing Ethernet
   shield (change the stack only with power off). Record scope evidence; a host test
   cannot establish electrical bus behavior or analog output accuracy.

## Optional USB bench actions (2026-10-16)

Build `pio run -e uno_r4_minima_bench` to include action emulation at 115200 baud.
Upload that environment only when restarting the board is acceptable. The normal
`uno_r4_minima` build does not consume USB commands. Pins and physical buttons
remain the same in both builds.

Send one newline-terminated command and wait for its `BENCH` response:

| USB command | Response / meaning |
| --- | --- |
| `BENCH STATUS` | `BENCH STATUS READY`, `OFFLINE`, `ACTIVE`, `LATCHED`, or `FAILED`. |
| `BENCH NETWORK` | `BENCH OK NETWORK`: network reset requested; wait for a fresh TCP connection. |
| `BENCH SAFEZERO` | `BENCH OK SAFEZERO`: all-channel zero ramp requested. |

`BENCH ERR ...` indicates rejection. NETWORK is rejected while SAFE ZERO is
requested, active, or physically held. A new NETWORK action after completion
rearms. Invalid starting readback reports `FAILED`, distinct from completed
`LATCHED`. Completion still needs independent analog evidence; status only
confirms the scheduled register endpoint.

The USB parser only queues the same action flags used by physical buttons. It
performs no SPI and no logging within Ethernet cancellation hooks. Replies wait
for TX space in foreground code; missing USB TX space does not block physical
buttons. Malformed/oversized lines are discarded in full. B22 exercises the
shared reset/zero/preemption/priority paths and parser recovery in the host build.

The separate clone in `../malab` contains the Keysight 34461A driver and
`notebooks/live_acceptance.ipynb`. Commands, measurements and assertions are
written directly in notebook cells. USB access needs `pyserial`; close serial
monitors before opening that port from the notebook. Emulation does not test the
electrical switch contacts, debounce, or a physically failed Ethernet controller.

The main API exposes these actions as `AD5780(..., bench_port="COM5")` methods
`network_reset`, `safe_zero` / `safe_ramp`, and `get_recovery_status`. The firmware
stores this box's measured CH1 calibration in `DeviceCalibration.h`, so the
physical button reaches calibrated zero without requiring a PC/network. Other
channels retain nominal profiles. The measured CH1 safe ramp takes about 5 s from
±0.05 V to zero, at no more than 0.01 V/s.

For controlled fault tests only, the bench firmware also accepts
`BENCH FAULT READBACK`, `BENCH FAULT CLEAR`, and `BENCH FAULT NETWORK`.
READBACK substitutes all-high frames after actual SPI reads, without changing DAC
registers. CLEAR removes that override. NETWORK clears the actual W5500 IP
configuration in foreground code to exercise normal automatic recovery. The main
API method is `bench_inject_fault('readback'/'clear'/'network')`. The normal build
compiles these controls out. E01/E02 verify errors, held outputs and recovery on
the real board; this is not proof against physical chip damage.

Validation: 86 outer regressions and 25 driver groups pass. Both builds compile;
production uses 73,676 bytes flash / 3,820 RAM, bench uses 74,900 / 3,924. Authorized
uploads and live tests are recorded in the fix ledger with binary hashes. The
current live notebook uses the historical `voltsource.AD5780` entry point.
