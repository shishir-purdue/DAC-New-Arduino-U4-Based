# Local bench configuration

Copy `bench.example.json` to **`bench.local.json`** in this directory and enter
the real identities. The local file is ignored by Git. Example addresses and
MACs in tracked files are documentation values, not instrument connection data.
Firmware builds, Python defaults, notebooks and the dashboard read the same file.
Set `DAC_BENCH_CONFIG` to a different absolute local path if needed.

Each item in `units`, in order, owns four global DAC channels. The first owns
1–4, the second 5–8, the third 9–12. Give each entry a unique `name`, IPv4
`address`, MAC, and (when connected) `usb_port` and `usb_serial`. The MAC/IP
identify the Ethernet interface; the USB serial selects the Arduino for upload.
`usb_port` controls the dashboard's button emulation. Use null for unavailable
USB fields. Upload refuses to choose an unspecified board. Connecting never
initializes outputs. Changing list order changes the outer channel numbering.

`calibration` contains four entries: null means nominal conversion. A measured
entry has `offset_nanovolts`, `step_picovolts`, `zero_code` and `measured`.
Keep a measured profile with its physical unit. The build generates all four
profiles together with that unit's identity, so its physical zero button uses
the same stored calibration even without a PC.

`meter.address` selects the Keysight; `meter.channel` is the global channel its
leads are physically attached to. Software does not switch meter wiring.
Optional `instruments` entries hold addresses for legacy driver examples.

## Add another DAC box

1. Append another unit object to your local file; do not edit C++ source.
2. Enter its labeled IP/MAC and verify its USB serial/port physically.
3. Use four null calibration entries until that box is measured.
4. Select its name for build/upload, for example in PowerShell:

```powershell
$env:DAC_UNIT = 'unit3'
pio run -e uno_r4_minima_bench
pio run -e uno_r4_minima_bench -t upload
Remove-Item Env:DAC_UNIT
```

Without `DAC_UNIT`, the base environment selects `unit1`; the Unit 2 aliases
select `unit2`. The build fails when the local file or named unit is missing.
The generated header and binaries stay under ignored `.pio/`. DFU is selected
by the configured USB serial, not `--upload-port`.

5. Restart the idle dashboard with `./malab/tools/start_dashboard.ps1 -Restart`.
   The appended unit appears with the next four channel numbers.

The current safe-zero rate is **at most 0.02 V/s** on each channel, with single
code steps and no catch-up bursts. A nominal 10 V to zero takes about 500 s.
Remote `ramp(..., speed=...)` retains its independently specified speed.
Firmware changes take effect only after each physical unit is uploaded.

## Publication

Never commit the local JSON, generated headers/binaries, or raw run output.
Public evidence uses redacted example network identities without changing the
measurements. Historical commits are a separate concern: ignoring a file does
not remove a value already present in Git history.

## Python-only resistor settings

The API/dashboard saves optional per-channel load resistances in ignored
`resistors.local.json` beside the selected bench configuration. Keys use each
physical unit's address and local channel, so reordering the active unit list
does not move resistor settings. Positive values are in ohms; absent entries
mean voltage mode. Add/edit/remove through the API or each dashboard card.
These settings never enter firmware builds or uploads. A lock file and atomic
temporary files in this directory also use the ignored `.local.json` suffix.
See [current controls and API](../malab/dashboard/README.md#per-channel-resistor-and-current-controls).

## Python-only channel labels

Optional display names live in ignored `labels.local.json` beside the bench
configuration, with the same physical-address/local-channel keys. Use each
card's **Rename** control or `set_label(channel, text)` / `get_label(channel)`
in the Python API. Empty text clears a label. Labels survive restart and unit
reordering and never enter firmware builds. Label and resistor files use the
same atomic storage implementation but retain separate values and files.
