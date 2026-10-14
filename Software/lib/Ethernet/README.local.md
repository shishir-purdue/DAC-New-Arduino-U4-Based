# DAC Box Ethernet recovery fork

Base: [arduino-libraries/Ethernet 2.0.2](https://github.com/arduino-libraries/Ethernet/tree/2.0.2),
copied from the PlatformIO-installed package on 2026-10-14. Original source headers,
AUTHORS, and library metadata are preserved. Upstream file licenses apply; this
vendored directory is not relicensed by the repository's top-level MIT license.

Local modifications (2026-10-14):

- `EthernetClass::reset()` clears DHCP/socket/server caches and invalidates cached
  WIZnet detection so `begin()` really resets and reconfigures the controller.
- `utility/DacNetworkGuard.h` provides 100 ms bounded waits and a 100,000-poll
  fallback. The application supplies `dacNetworkCancelled()` and
  `dacNetworkFault()`. Hooks must never call SPI, Ethernet, or blocking output.
- Socket command, RX/TX count-stability, TCP/UDP send, TCP connect, client flush,
  and client stop waits cooperate with cancellation. Init's 560 ms delay is
  sliced into 1 ms pieces. Cancelled register reads zero their destination buffers;
  cancelled writes do not touch SPI. RX count underflow latches a fault.
- Fault/cancellation paths unwind their existing SPI transactions normally.
- W5500 initialization separately resets PHYCFGR and restores all-capable
  auto-negotiation. Failed PHY readback leaves initialization unsuccessful.

This application uses a static address and accepts TCP clients. DHCP/DNS protocol
loops are not hardened or used for local recovery. The serial monitor's logging
guard is in `Software/lib/AD5780/DiagnosticLog.h`, outside this library.

Do not replace this library with unmodified registry Ethernet 2.0.2: builds would
lose the reset API/cancellation behavior needed by the buttons. PlatformIO loads
this directory via the project's `lib_dir = Software/lib`. Relevant regression
tests compile the actual `socket.cpp` and `utility/w5100.cpp` against W5100 and
W5500 register/SPI doubles; electrical validation still needs a bench.
