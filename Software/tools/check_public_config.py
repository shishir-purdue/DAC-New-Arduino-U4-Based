"""Check publishable text for local identities; raw results/builds stay ignored.

Run from the workspace before committing/pushing. The optional local JSON adds
checks for MACs and USB serials without embedding those identities in this tool.
"""
import ipaddress
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SUFFIXES = {".py", ".md", ".json", ".ipynb", ".cpp", ".h", ".ini", ".txt",
            ".html", ".js", ".ps1", ".log", ".csv", ".toml", ".yml", ".yaml"}


def audit():
    identities = []
    local = ROOT / "config/bench.local.json"
    if local.is_file():
        config = json.loads(local.read_text(encoding="utf-8"))
        for unit in config["units"]:
            identities += [unit.get(k) for k in ("address", "mac", "usb_serial")]
        identities += [config["meter"]["address"]]
        identities += list(config.get("instruments", {}).values())
    private = [ipaddress.ip_network(n) for n in ("10.0.0.0/8", "172.16.0.0/12", "192.168.0.0/16")]
    violations = set()
    for repo in [ROOT, ROOT / "malab"]:
        if not (repo / ".git").exists():
            continue
        args = ["git", "-c", "safe.directory=" + repo.as_posix(), "-C", str(repo)]
        names = subprocess.check_output(args + ["ls-files", "-co", "--exclude-standard", "-z"]).decode().split("\0")
        for name in set(names):
            path = repo / name
            if not path.is_file() or path.suffix.lower() not in SUFFIXES:
                continue
            text = path.read_bytes().decode("utf-8", errors="replace")
            leak = any(v and v.lower() in text.lower() for v in identities)
            for token in re.findall(r"\b\d{1,3}(?:\.\d{1,3}){3}\b", text):
                try:
                    ip = ipaddress.ip_address(token)
                except ValueError:
                    continue
                # Network definitions in this checker are not bench identities.
                if path != Path(__file__) and any(ip in net for net in private):
                    leak = True
            if re.search(r"\b192\s*,\s*168\s*,", text):
                leak = True
            if leak:
                violations.add(str(path.relative_to(ROOT)))
    if violations:
        raise SystemExit("Private network identities in publishable files:\n" + "\n".join(sorted(violations)))
    print("PASS: no local identities or private IPv4 literals in publishable text")


if __name__ == "__main__":
    audit()
