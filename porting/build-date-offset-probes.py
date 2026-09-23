#!/usr/bin/env python3
"""Compile/run isolated regression probes from the actual patched DateMath body."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from paths import external_work_dir

HERE = Path(__file__).resolve().parent

def extract_function(text):
    start = text.index("static int32_t calculateUTCOffset()")
    opening = text.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        if text[end] == "{":
            depth += 1
        elif text[end] == "}":
            depth -= 1
        end += 1
    return text[start:end] + "\n"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = external_work_dir(args.source)
    output = external_work_dir(args.output)
    manifest = json.loads((HERE / "webkit-patches.json").read_text())
    patch = next(p for p in manifest["patches"] if p["path"] == "Source/WTF/wtf/DateMath.cpp")
    source_bytes = (source / patch["path"]).read_bytes()
    if hashlib.sha256(source_bytes).hexdigest() != patch["after_sha256"]:
        raise SystemExit("DateMath must match the reviewed source patch")
    output.mkdir(parents=True, exist_ok=True)
    (output / "date-offset-under-test.inc").write_text(extract_function(source_bytes.decode()))
    fixture = HERE / "date-offset-smoke.cpp"
    for variant, flags in [("win9x", ["-D_WIN32_WINDOWS=0x0410"]), ("modern", [])]:
        binary = output / ("date-offset-" + variant)
        subprocess.run(["c++", "-std=c++17", "-O2", *flags, "-I", str(output), str(fixture), "-o", str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    subprocess.run(["i686-w64-mingw32-g++", "-std=c++17", "-Os", "-static", "-static-libgcc", "-static-libstdc++",
                    "-DIEWK_REAL_WIN32=1", "-DWINVER=0x0410", "-D_WIN32_WINDOWS=0x0410", "-D_WIN32_WINNT=0x0400",
                    "-I", str(output), str(fixture), "-o", str(output / "date-offset-me.exe")], check=True)
    print("Host regression probes passed; Windows executable built but not executed.")

if __name__ == "__main__":
    main()
