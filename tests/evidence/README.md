# Public evidence and private bench settings

Historical evidence preserves measurement values, timing, and PASS/FAIL results.
Network identities and USB serial identifiers have been replaced with clearly
documented example values. These addresses are not connection settings.
Original unsanitized records remain only in ignored local results storage.

Runtime settings live in ignored `config/bench.local.json`. Do not commit raw
logs, executed notebooks, generated firmware headers or firmware binaries that
contain those settings. The 0.01 V/s results in earlier runs describe the old
firmware; the current requested safe-zero rate is 0.02 V/s.
