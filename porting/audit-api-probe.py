#!/usr/bin/env python3
"""Compile the pinned port's API fixture and compare with a known ME KERNEL32."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from paths import external_work_dir

HERE = Path(__file__).resolve().parent

def pe(path):
    return subprocess.check_output(["i686-w64-mingw32-objdump", "-p", str(path)], text=True)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kernel32", required=True, type=Path)
    parser.add_argument("--kernel32-sha256", required=True)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    args.output_dir = external_work_dir(args.output_dir)
    actual = hashlib.sha256(args.kernel32.read_bytes()).hexdigest()
    if actual != args.kernel32_sha256:
        raise SystemExit("ME baseline checksum mismatch")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    binary = args.output_dir / "required-api-probe.exe"
    subprocess.run(["i686-w64-mingw32-gcc", "-std=c99", "-Os", "-Wall", "-Wextra", "-Werror", "-D_WIN32_WINNT=0x0A00", str(HERE / "required-api-probe.c"), "-o", str(binary)], check=True)
    exported = set(re.findall(r"^\s*\[\s*\d+\] \+base\[\s*\d+\]\s+[0-9a-fA-F]+\s+(\S+)\s*$", pe(args.kernel32), re.M))
    table = pe(binary).split("The Import Tables", 1)[1]
    kernel = table.split("DLL Name: KERNEL32.dll", 1)[1].split("DLL Name:", 1)[0]
    imported = set(re.findall(r"<none>\s+[0-9a-fA-F]+\s+(\S+)", kernel))
    if not exported or not imported:
        raise SystemExit("Unable to parse PE tables")
    result = {"fixture_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(), "baseline_sha256": actual, "missing_kernel32_exports": sorted(imported - exported), "guest_tested": False}
    (args.output_dir / "required-api-audit.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 1 if result["missing_kernel32_exports"] else 0

if __name__ == "__main__":
    raise SystemExit(main())
