"""Notebook/CLI entry points. Failures are real assertions, never expected-pass xfails."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
HERE = Path(__file__).resolve().parent
PRELIMINARY = ["Mem-1", "Mem-2", "Mem-3", "Robust-1", "Robust-2"]
DESCRIPTIONS = {
    "Mem-1": "Repeated real command dispatch performs no C++ heap allocations",
    "Mem-2": "Silent second clients release sockets; owner survives repeated commands",
    "Mem-3": "Disconnect stops socket and clears partial input before reconnect",
    "Robust-1": "Reject second clients both while idle and during a ramp",
    "Robust-2": "Busy handlers do no SPI work; connection lifecycle cannot release operation lock",
    "C1": "Invalid channels perform no SPI; valid channels 1 through 4 work",
    "C2": "SPI begins before DAC transactions",
    "C3": "Second-client rejection leaves owner and ramp operational (overlaps Robust-1)",
    "C4": "Link/configuration loss discards stale clients and retries recovery without changing DAC outputs",
    "C5": "Cooperative ramp services connections/input and aborts on disconnect (with M4/A4)",
    "H1": "No local SPI settings variable shadows global settings in setup",
    "H2": "Reject negative ramp step before output changes",
    "H3": "Reject negative ramp delay before output changes or unsigned wait",
    "H4": "Long step waits use bounded/cooperative timing (policy check; timing needs bench)",
    "H5": "EnableOutput public declaration matches definition",
    "H6": "Bare plus/minus signs are not integers",
    "M1": "Recover from a silent owner within proposed 120-second timeout policy",
    "M2": "Client receives explicit warning/error when target is clamped",
    "M3": "Oversized command cannot execute a truncated valid prefix",
    "M4": "Command injected during ramp is serviced before ramp returns",
    "M5": "Local include capitalization matches actual filename",
    "M6": "No unused frame-prefix assignments",
    "M7": "GPIO aliases resolve to defined symbols",
    "M8": "Balanced SPI transactions; interrupt contention requires a separate bench review",
    "M9": "INIT preserves output under proposed no-change policy (verify intended startup policy)",
    "M10": "Reject integer overflow before DAC access",
    "M11": "Acknowledge ramp before first blocking delay",
    "A1": "Active client is not reclassified as a second client",
    "A2": "INIT detects bad register readback instead of reporting success",
    "A3": "Repository contains a reproducible target/dependency build configuration",
    "A4": "No additional DAC write after disconnect during step delay",
}
MODES = {key: "host behavior" for key in DESCRIPTIONS}
MODES.update({key: "source audit" for key in ["H1", "H5", "M5", "M6", "M7", "A3"]})
MODES.update({"C4": "bench review", "M8": "host + bench review"})


class ReviewRequired(unittest.SkipTest):
    """No automatic pass is possible without physical evidence or an agreed policy."""


def code(path: str) -> str:
    text = (ROOT / path).read_text(encoding="utf-8")
    return re.sub(r"//[^\n]*|/\*.*?\*/", "", text, flags=re.S)


def compiler_command() -> tuple[list[str], dict[str, str], bool]:
    """Use an installed compiler; do not download dependencies or alter PATH globally."""
    env = os.environ.copy()
    if os.name == "nt":
        if shutil.which("cl"):
            return ["cl"], env, True
        vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        if vswhere.exists():
            found = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires",
                                    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
                                   capture_output=True, text=True, check=True).stdout.strip()
            if found:
                vcvars = Path(found) / "VC/Auxiliary/Build/vcvars64.bat"
                # Trusted local installer path; env setup only. Compilation uses argv below.
                result = subprocess.run(f'call "{vcvars}" >nul && set', shell=True,
                                        capture_output=True, text=True, check=True)
                seen = set()
                for line in result.stdout.splitlines():
                    key, sep, value = line.partition("=")
                    if sep and key and not key.startswith("=") and key.lower() not in seen:
                        seen.add(key.lower())
                        # Windows keys are case-insensitive; avoid duplicate PATH variants.
                        for old in list(env):
                            if old.lower() == key.lower():
                                del env[old]
                        env[key] = value
                path = next(v for k, v in env.items() if k.lower() == "path")
                cl = shutil.which("cl", path=path)
                if cl:
                    return [cl], env, True
    for name in ["g++", "clang++"]:
        if shutil.which(name):
            return [name], env, False
    raise RuntimeError("No C++17 compiler found. Install MSVC C++ Build Tools (Windows), g++, or clang++; source audits can still run separately.")


def build_host(harness="firmware_tests") -> Path:
    assert harness in {"firmware_tests", "network_tests"}
    build = HERE / ".build"
    build.mkdir(exist_ok=True)
    binary = build / (harness + (".exe" if os.name == "nt" else ""))
    sources = sorted(list((ROOT / "Software").rglob("*.cpp")) + list((ROOT / "Software").rglob("*.h")) + list((HERE / "host").glob("*")) + list((HERE / "network_host").glob("*")))
    digest = hashlib.sha256()
    for path in sources + [Path(__file__)]:
        digest.update(str(path).encode()); digest.update(path.read_bytes())
    stamp = build / (harness + "-source.sha256")
    if binary.exists() and stamp.exists() and stamp.read_text() == digest.hexdigest():
        return binary
    compiler, env, msvc = compiler_command()
    source = HERE / ("host/firmware_tests.cpp" if harness == "firmware_tests" else "network_host/network_tests.cpp")
    includes = [HERE / ("host" if harness == "firmware_tests" else "network_host"), ROOT / "Software/lib/AD5780"]
    if harness == "network_tests":
        includes.append(ROOT / "Software/lib/Ethernet/src")
    if msvc:
        args = compiler + ["/nologo", "/std:c++17", "/EHsc", "/W4", "/wd4100", "/wd4996", "/utf-8"]
        args += [f"/I{p}" for p in includes] + [str(source), f"/Fe:{binary}", f"/Fo:{build / (harness + '.obj')}"]
    else:
        args = compiler + ["-std=c++17", "-Wall", "-Wextra", "-Wno-unused-parameter"]
        args += [f"-I{p}" for p in includes] + [str(source), "-o", str(binary)]
    result = subprocess.run(args, env=env, cwd=build, capture_output=True, text=True, timeout=90)
    (build / (harness + "-compile.log")).write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise RuntimeError("Host compilation failed:\n" + result.stdout + result.stderr)
    stamp.write_text(digest.hexdigest())
    return binary


def run_host(case_id: str) -> None:
    result = subprocess.run([str(build_host("network_tests" if case_id.startswith("N") else "firmware_tests")), case_id], capture_output=True, text=True, timeout=10)
    assert result.returncode == 0, result.stdout + result.stderr or f"Host process exited with code {result.returncode} (no diagnostic output)"


def run_case(case_id: str) -> None:
    """Run one item. AssertionError shows the failure in its notebook cell."""
    if case_id not in DESCRIPTIONS:
        raise KeyError(f"Unknown case {case_id!r}; choose from {list(DESCRIPTIONS)}")
    main = code("Software/src/main.cpp")
    driver = code("Software/lib/AD5780/AD5780.cpp")
    header = code("Software/lib/AD5780/AD5780.h")
    comm = code("Software/lib/AD5780/Communication.cpp")
    if case_id == "Mem-1":
        assert not re.search(r"\b(?:String|new|malloc|calloc|realloc|strdup)\b", main), "Mem-1: dynamic allocation API reintroduced into the application source"
        run_host(case_id)
    elif case_id == "H1":
        assert len(re.findall(r"\bSPISettings\s+AD5780_spi_settings\s*;", main)) == 1, "H1: setup declares a local AD5780_spi_settings; global fallback is not updated"
    elif case_id == "H5":
        pattern = r"void\s+AD5780_EnableOutput\s*\(([^)]*)\)"
        declared = re.search(pattern, header).group(1)
        defined = re.search(pattern, driver).group(1)
        assert re.sub(r"\s+", "", declared) == re.sub(r"\s+", "", defined), f"H5: declaration ({declared}) != definition ({defined})"
        run_host(case_id)
    elif case_id == "M5":
        names = {p.name for p in (ROOT / "Software/lib/AD5780").iterdir()}
        for name in re.findall(r'#include\s+"([^"]+)"', main):
            assert name in names or (ROOT / "Software/src" / name).is_file(), f"M5: include {name!r} does not exactly match library filenames {sorted(names)}"
    elif case_id == "M6":
        unused = re.findall(r"(?:writeCommand|registerWord)\[0\]\s*=\s*0x01", driver)
        assert not unused, f"M6: {len(unused)} unused prefix assignments; transport starts at data[1]"
    elif case_id == "M7":
        symbols = set(re.findall(r"\bGPIO\d_PIN_\w+", header))
        all_code = main + driver + header + comm + code("Software/lib/AD5780/Communication.h")
        undefined = [s for s in symbols if not re.search(r"#\s*define\s+" + s + r"\b", all_code)]
        assert not undefined, f"M7: template aliases refer to undefined GPIO symbols: {sorted(undefined)} (currently unused)"
    elif case_id == "A3":
        configs = list(ROOT.rglob("platformio.ini")) + list(ROOT.rglob("sketch.yaml"))
        assert configs, "A3: no PlatformIO project or Arduino CLI sketch profile checked in; target/dependencies are manual"
        raise ReviewRequired("A3: configuration exists; run a clean target build and review dependency pinning before closing")
    elif case_id == "C4":
        run_host(case_id)
    elif case_id == "C5":
        for related in ("C5", "M4", "A4", "Robust-1"):
            run_host(related)
    elif case_id == "M8":
        run_host(case_id)
        application = main + driver + comm + code("Software/src/PhysicalControls.h")
        assert not re.search(r"\b(?:attachInterrupt|ISR|IRQHandler)\s*\(", application), "M8: new ISR requires a shared-SPI ownership review"
        run_host("B6")  # Cancellation hooks only sample GPIO/RAM, no nested SPI.
    else:
        run_host(case_id)
    print(f"PASS {case_id}: {DESCRIPTIONS[case_id]}")


def run_all(ids=None, *, fail_on_error=False):
    """Collect all failures so one open bug does not hide the remaining results."""
    results = []
    for case_id in ids or DESCRIPTIONS:
        try:
            run_case(case_id)
            result = {"id": case_id, "status": "PASS", "detail": DESCRIPTIONS[case_id]}
        except ReviewRequired as exc:
            result = {"id": case_id, "status": "REVIEW", "detail": str(exc)}
        except (AssertionError, RuntimeError, subprocess.SubprocessError) as exc:
            result = {"id": case_id, "status": "FAIL", "detail": str(exc)}
        results.append(result)
        if result["status"] != "PASS":
            print(f"{result['status']} {case_id}: {result['detail']}")
    counts = {status: sum(r["status"] == status for r in results) for status in ["PASS", "FAIL", "REVIEW"]}
    print("\nSummary:", counts)
    if fail_on_error:
        assert not counts["FAIL"], f"Checklist failures: {[r['id'] for r in results if r['status'] == 'FAIL']}"
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--case", choices=list(DESCRIPTIONS))
    parser.add_argument("--preliminary", action="store_true")
    parser.add_argument("--report", type=Path, help="Write JSON observations (optional)")
    args = parser.parse_args()
    rows = run_all([args.case] if args.case else PRELIMINARY if args.preliminary else None)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(rows, indent=2), encoding="utf-8")
    sys.exit(1 if any(r["status"] == "FAIL" for r in rows) else 0)
