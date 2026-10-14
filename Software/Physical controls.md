# Physical recovery controls

Implementation: 2026-10-14. Target: Arduino Uno R4 Minima with W5500 Ethernet shield and
the repository's AD5780 extension board. Firmware is prepared and host-tested;
physical button/analog tests are still required. The user has tested the initial
button firmware and subsequently confirmed correct zero/readback after the correction.
Codex has not performed a firmware upload for these changes.

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
then moves every nonzero channel toward
**code 131072** (nominal 0 V for offset-binary +/-10 V operation). Positive outputs
decrease; negative outputs increase toward zero. Channels move concurrently, with
their SPI writes performed sequentially a few microseconds apart.

The requested rate is **0.01 V/s**. Each channel moves by one 76.294 uV code no
more often than every 7630 us (about 0.0099992 V/s at best; scheduling overhead can
make it slightly slower). There are no catch-up jumps after delays. Typical times:

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
import math
import time

V_PER_BIT = 20.0 / 2**18

def ramp(channel, voltage, speed):
    """Ramp to voltage at speed in V/s."""
    if channel not in (1, 2, 3, 4):
        raise ValueError("channel must be 1-4")
    if not math.isfinite(voltage) or not -10.0 <= voltage <= 10.0:
        raise ValueError("voltage must be between -10 and +10 V")
    if not math.isfinite(speed) or speed <= 0:
        raise ValueError("speed must be finite and greater than zero")

    bitcode = min(262143, int((voltage + 10.0) / V_PER_BIT))
    step_size = 10  # DAC codes per step, approximately 0.763 mV
    step_time = math.ceil(step_size * V_PER_BIT * 1_000_000 / speed)
    if step_time > 2_147_483_647:
        raise ValueError("speed is too slow for the firmware's delay range")

    command = f"RAMP {channel:d} {bitcode:d} {step_size:d} {step_time:d}"
    print(command)
    time.sleep(dac.query_sleep)
    response = dac.query(command)
    time.sleep(dac.query_sleep)
    return response
```

At 0.001 V/s this sends 762940 us per step; at 0.01 V/s it sends 76294 us.
The query timeout must exceed the full ramp duration plus overhead (approximately
1000 s for 0 to 1 V at 0.001 V/s). The response is sent after completion. The
wrapper correction is identified from the supplied code; physical slew-rate
verification is still pending.

## Why the Ethernet library is local

`Software/lib/Ethernet` vendors upstream 2.0.2 plus small recovery changes.
Upstream socket-command, stable-register-read, and transmit waits can otherwise
loop forever. The local versions have 100 ms deadlines and a finite poll fallback,
and sample button cancellation while waiting. Hooks only access GPIO and RAM;
they never perform nested DAC/SPI work. Network failures latch the network offline.
The application uses static IP; DHCP and DNS are not part of the recovery path.

Remote ramp delays are sliced into at most 1000 us waits and checked again before
each DAC write. USB diagnostics drop messages when the TX buffer has no room,
instead of blocking local controls behind an unresponsive USB host.

These controls address software/network failures while the MCU and DAC path work.
There is no claimed recovery from a hung CPU, disabled interrupts, or damaged DAC
hardware.

## Verification and bench procedure

Run `python -m unittest discover -s tests -p test_buttons.py -v` for the 32 offline
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
guessed starting voltage. The `get_voltage` client is not present in this repository;
it must treat an `ERR!` response as an error, never as a numeric voltage.

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
