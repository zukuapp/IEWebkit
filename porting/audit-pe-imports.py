#!/usr/bin/env python3
"""Compare a linked PE's imports with checksum-pinned OS export baselines.

A passing import audit is not proof of target runtime behavior.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from paths import external_work_dir


def pe(path):
    result = subprocess.check_output(["i686-w64-mingw32-objdump", "-p", str(path)], text=True)
    if "file format pei-i386" not in result:
        raise ValueError(f"Expected x86 PE: {path}")
    return result


def imports(text):
    result = {}
    current = None
    table = text.split("The Import Tables", 1)[1].split("The Export Tables", 1)[0]
    for line in table.splitlines():
        dll = re.search(r"DLL Name:\s*(\S+)", line)
        if dll:
            current = dll[1].lower()
            result[current] = set()
        elif current:
            if "<ordinal>" in line:
                raise ValueError(f"Ordinal import requires separate resolution: {current}")
            symbol = re.search(r"<none>\s+[0-9a-fA-F]+\s+(\S+)", line)
            if symbol:
                result[current].add(symbol[1])
    if not result or any(not values for values in result.values()):
        raise ValueError("No complete named imports found")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--dll-dir", required=True, type=Path)
    parser.add_argument("--baseline", required=True, type=Path, help='JSON: {"dlls": {"kernel32.dll": "sha256", ...}}')
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = external_work_dir(args.output)
    baseline = json.loads(args.baseline.read_text())["dlls"]
    dlls = {p.name.lower(): p for p in args.dll_dir.iterdir() if p.is_file()}
    gaps = {}
    for dll, required in imports(pe(args.binary)).items():
        path = dlls.get(dll)
        if not path or dll not in baseline:
            gaps[dll] = {"reason": "missing pinned baseline", "imports": sorted(required)}
            continue
        if hashlib.sha256(path.read_bytes()).hexdigest() != baseline[dll]:
            raise ValueError(f"Baseline hash mismatch: {dll}")
        exported = set(re.findall(r"^\s*\[\s*\d+\] \+base\[\s*\d+\]\s+[0-9a-fA-F]+\s+(\S+)\s*$", pe(path), re.M))
        if not exported:
            raise ValueError(f"Missing export table: {dll}")
        absent = sorted(required - exported)
        if absent:
            gaps[dll] = {"reason": "missing exports", "imports": absent}
    result = {"binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(), "baseline_sha256": hashlib.sha256(args.baseline.read_bytes()).hexdigest(), "import_compatible": not gaps, "gaps": gaps, "guest_tested": False}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 1 if gaps else 0

if __name__ == "__main__":
    raise SystemExit(main())
