"""PlatformIO pre-script: generate unit data only inside the ignored build dir."""
import importlib.util
import os
from pathlib import Path

Import("env")
root = Path(env.subst("$PROJECT_DIR"))
spec = importlib.util.spec_from_file_location("bench_configuration", root / "Software/tools/bench_configuration.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
path = Path(os.environ.get("DAC_BENCH_CONFIG", root / "config/bench.local.json"))
if not path.is_file():
    raise ValueError("Create ignored config/bench.local.json from the example before building")
name = os.environ.get("DAC_UNIT", env.GetProjectOption("custom_unit", "unit1"))
unit = module.select_unit(module.read_units(path), name)
directory = Path(env.subst("$BUILD_DIR")) / "private_config"
directory.mkdir(parents=True, exist_ok=True)
header = directory / "DeviceConfig.generated.h"
content = module.render_header(unit)
if not header.is_file() or header.read_text(encoding="utf-8") != content:
    header.write_text(content, encoding="utf-8")
env.Prepend(CPPPATH=[str(directory)])
env["DAC_USB_SERIAL"] = unit.get("usb_serial") or ""
print("Using local configuration for " + name)
