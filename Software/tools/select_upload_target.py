"""Keep DFU uploads on the selected physical board when both units are on USB."""
from SCons.Script import COMMAND_LINE_TARGETS
Import("env")

# renesas-ra 1.9.0's DFU uploader selects by VID/PID and ignores --upload-port.
# A post script prepends the serial selector without replacing its download flags.
serial = env.get("DAC_USB_SERIAL", "").strip()
if "upload" in COMMAND_LINE_TARGETS and (not serial or not serial.isalnum()):
    raise ValueError("Set usb_serial for the selected unit in ignored local configuration before uploading")
if env.subst("$UPLOAD_PROTOCOL") == "dfu":
    # Even a build without USB metadata must never fall back to any attached board.
    env.Prepend(UPLOADERFLAGS=["-S", serial or "NO_CONFIGURED_USB_TARGET"])
