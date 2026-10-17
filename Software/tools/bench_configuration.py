"""Validate local unit identities and render private firmware configuration."""
import ipaddress
import json
import re
from pathlib import Path


def read_units(path):
    config = json.loads(Path(path).read_text(encoding="utf-8"))
    units = config.get("units")
    if not isinstance(units, list) or not units:
        raise ValueError("Local bench configuration needs a nonempty units list")
    seen = {key: set() for key in ("name", "address", "mac", "usb_serial", "usb_port")}
    for unit in units:
        if not isinstance(unit, dict):
            raise ValueError("Each unit must be an object")
        name = unit.get("name", "")
        if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9_-]+", name):
            raise ValueError("Each unit needs a simple unique name")
        address = str(ipaddress.IPv4Address(unit["address"]))
        ip = ipaddress.IPv4Address(address)
        if ip.is_multicast or ip.is_unspecified or ip.is_loopback:
            raise ValueError("Unit IP must be a usable unicast address")
        mac = unit.get("mac", "")
        if not isinstance(mac, str) or not re.fullmatch(r"(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}", mac):
            raise ValueError("Each unit needs a six-byte colon-separated MAC")
        mac = mac.upper()
        if int(mac[:2], 16) & 1 or int(mac.replace(":", ""), 16) == 0:
            raise ValueError("Unit MAC must be a nonzero unicast address")
        unit["address"], unit["mac"] = address, mac
        for key in seen:
            value = unit.get(key)
            if key in ("usb_port", "usb_serial") and value is None:
                continue
            if not isinstance(value, str) or not value.strip():
                raise ValueError(f"Invalid unit {key}")
            value = value.upper()
            if value in seen[key]:
                raise ValueError(f"Duplicate unit {key}; refusing ambiguous configuration")
            seen[key].add(value)
        serial = unit.get("usb_serial")
        if serial is not None and not serial.isalnum():
            raise ValueError("USB serial must be alphanumeric")
        profiles = unit.get("calibration", [None] * 4)
        if not isinstance(profiles, list) or len(profiles) != 4:
            raise ValueError("Each unit needs exactly four calibration entries")
        for profile in profiles:
            if profile is None:
                continue
            if not isinstance(profile, dict):
                raise ValueError("Calibration must be null or an object")
            bounds = {"offset_nanovolts": (-(1 << 63), (1 << 63)-1),
                      "step_picovolts": (1, 1_000_000_000), "zero_code": (0, 262143)}
            for key, (low, high) in bounds.items():
                value = profile.get(key)
                if type(value) is not int or not low <= value <= high:
                    raise ValueError(f"Invalid calibration {key}")
            if type(profile.get("measured")) is not bool:
                raise ValueError("Calibration measured must be boolean")
    return config


def select_unit(config, name):
    matches = [u for u in config["units"] if u["name"] == name]
    if len(matches) != 1:
        raise ValueError("Selected unit is missing from local bench configuration")
    return matches[0]


def render_header(unit):
    mac = ",".join("0x" + x for x in unit["mac"].split(":"))
    ip = ",".join(unit["address"].split("."))
    profiles = []
    for p in unit.get("calibration", [None] * 4):
        if p is None:
            profiles.append("nominalCalibration")
        else:
            profiles.append("{%dLL,%du,%du,%s}" % (p["offset_nanovolts"], p["step_picovolts"],
                            p["zero_code"], str(p["measured"]).lower()))
    return ("#pragma once\n// Generated private configuration; never commit this file.\n"
            f"#define DAC_CONFIGURED_MAC {{{mac}}}\n"
            f"#define DAC_CONFIGURED_IP {{{ip}}}\n"
            f'#define DAC_NETWORK_READY_MESSAGE "Network ready: {unit["address"]}:23."\n'
            f'#define DAC_CHANNEL_CALIBRATIONS {{{",".join(profiles)}}}\n')
