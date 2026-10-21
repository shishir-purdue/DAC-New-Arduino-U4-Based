# DAC test/debugging wiring diagram

One representative DAC box and one measured channel. Power rails are +15 V,
0 V and −15 V; no connector pin assignment is implied. Meter HI goes to the
selected output and LO to its return. USB upload uses the existing Arduino
bootloader/DFU path; runtime diagnostic feedback and button emulation use the
USB serial COM interface. Ethernet control/readback and meter SCPI share the
bench LAN; the switch is omitted. The PC compares actual voltage readings with
notebook assertions before the next firmware edit. This illustrates the voltage
acceptance tests; it does not add a load or imply direct current measurement.

Editable source: `dac-test-debugging-loop.svg`. Shareable render:
`dac-test-debugging-loop.png`. Generated as native SVG for exact labels/arrows.
The OpenAI Blossom in the PC is the unmodified inline vector from the official
[OpenAI developer documentation](https://developers.openai.com/codex), identifying
Codex's provider. The logo belongs to OpenAI. No private network identities are
included in the diagram.
