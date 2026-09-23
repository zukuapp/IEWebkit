#!/usr/bin/env python3
"""Select exact development targets; release selection requires guest evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
VERSIONS = {"5.5", "6", "7", "8", "9", "10", "11"}
CHECKS = {"remote_document", "independent_sites", "dom_javascript", "tls_validation",
          "origin_isolation", "korean_input", "history", "forms_upload",
          "unload", "architecture", "security_mode"}


def validate(data):
    if data.get("schema_version") != 1 or data.get("product") != "IEWebkit":
        raise ValueError("unsupported manifest")
    profiles = data.get("profiles")
    if not isinstance(profiles, list) or not profiles:
        raise ValueError("missing profiles")
    ids, combinations, versions = set(), set(), set()
    for p in profiles:
        if set(p) != {"id", "ie", "os", "architectures", "modes", "engine_profile", "certifications"}:
            raise ValueError("unexpected profile fields")
        if p["ie"] not in VERSIONS or not re.fullmatch(r"ie[0-9]+-[a-z0-9-]+", p["id"]):
            raise ValueError("invalid IE/profile identifier")
        if not re.fullmatch(r"[a-z0-9-]+", p["os"]) or not re.fullmatch(r"[a-z0-9-]+", p["engine_profile"]):
            raise ValueError("invalid OS/engine identifier")
        if p["id"] != "ie" + p["ie"].replace(".", "") + "-" + p["os"] or p["id"] in ids:
            raise ValueError("duplicate or mismatched profile identifier")
        ids.add(p["id"])
        versions.add(p["ie"])
        for field, allowed in [("architectures", {"x86", "x64"}),
                               ("modes", {"classic", "protected", "enhanced-protected"})]:
            values = p[field]
            if not isinstance(values, list) or not values or len(set(values)) != len(values) or not set(values) <= allowed:
                raise ValueError("invalid " + field)
        if p["os"] == "winme" and (p["ie"] not in {"5.5", "6"} or p["architectures"] != ["x86"] or p["modes"] != ["classic"]):
            raise ValueError("invalid ME target")
        if "enhanced-protected" in p["modes"] and p["ie"] not in {"10", "11"}:
            raise ValueError("EPM requires its own IE10/11 integration")
        for arch in p["architectures"]:
            for mode in p["modes"]:
                key = (p["ie"], p["os"], arch, mode)
                if key in combinations:
                    raise ValueError("ambiguous target")
                combinations.add(key)
        certs = p["certifications"]
        if not isinstance(certs, list):
            raise ValueError("certifications must be a list")
        seen = set()
        for c in certs:
            if set(c) != {"arch", "mode", "evidence", "sha256"}:
                raise ValueError("invalid certification fields")
            if c["arch"] not in p["architectures"] or c["mode"] not in p["modes"]:
                raise ValueError("certification target mismatch")
            key = (c["arch"], c["mode"])
            if key in seen:
                raise ValueError("duplicate certification")
            seen.add(key)
            if not re.fullmatch(r"[0-9a-f]{64}", c["sha256"]):
                raise ValueError("invalid evidence digest")
            path = Path(c["evidence"])
            if path.is_absolute() or ".." in path.parts or path.parts[:1] != ("evidence",):
                raise ValueError("evidence must be repository-relative under evidence/")
    if versions != VERSIONS:
        raise ValueError("every IE version needs a separate profile")
    return data


def select(data, ie, os_name, arch, mode, release=False, root=ROOT):
    validate(data)
    matches = [p for p in data["profiles"] if p["ie"] == ie and p["os"] == os_name
               and arch in p["architectures"] and mode in p["modes"]]
    if len(matches) != 1:
        raise ValueError("no exact variant; no version/OS/bitness/security fallback")
    p = matches[0]
    artifact = p["id"] + "-" + arch + "-" + mode
    result = {"artifact_id": artifact, "ie": ie, "os": os_name, "arch": arch,
              "mode": mode, "engine_profile": p["engine_profile"], "verified": False}
    if release:
        cert = next((c for c in p["certifications"] if c["arch"] == arch and c["mode"] == mode), None)
        if not cert:
            raise ValueError("variant has no guest certification; release unavailable")
        path = (root / cert["evidence"]).resolve()
        if not path.is_relative_to(root.resolve()):
            raise ValueError("evidence escapes repository")
        raw = path.read_bytes()
        if hashlib.sha256(raw).hexdigest() != cert["sha256"]:
            raise ValueError("evidence digest mismatch")
        ev = json.loads(raw)
        if ev.get("artifact_id") != artifact or ev.get("passed") is not True:
            raise ValueError("guest evidence failed or mismatched")
        if not re.fullmatch(r"[0-9a-f]{64}", ev.get("artifact_sha256", "")):
            raise ValueError("missing tested artifact digest")
        if not CHECKS <= ev.get("checks", {}).keys() or any(ev["checks"][key] is not True for key in CHECKS):
            raise ValueError("incomplete guest verification")
        result.update(verified=True, artifact_sha256=ev["artifact_sha256"])
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=ROOT / "variants.json")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("validate")
    command = commands.add_parser("select")
    for arg in ["ie", "os", "arch", "mode"]:
        command.add_argument("--" + arg, required=True)
    command.add_argument("--release", action="store_true")
    args = parser.parse_args()
    try:
        data = validate(json.loads(args.manifest.read_text()))
        result = {"valid": True, "profiles": len(data["profiles"])} if args.command == "validate" else select(
            data, args.ie, args.os, args.arch, args.mode, args.release, args.manifest.parent)
        print(json.dumps(result, indent=2))
    except (ValueError, OSError, KeyError, TypeError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
