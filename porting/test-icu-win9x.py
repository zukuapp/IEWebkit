#!/usr/bin/env python3
"""Run host ICU/CLDR and exact-source Win9x registry-boundary regression probes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from paths import external_work_dir

HERE = Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", type=Path, required=True)
    args = parser.parse_args()
    work = external_work_dir(args.work_dir)
    source = work / "icu/source"
    output = work / "win9x-probes"
    item = next(p for p in json.loads((HERE / "icu-patches.json").read_text())["patches"] if p["path"] == "source/common/wintz.cpp")
    body = (work / "icu" / item["path"]).read_bytes()
    if hashlib.sha256(body).hexdigest() != item["after_sha256"]:
        raise SystemExit("ICU detector must match the reviewed patch")
    text = body.decode()
    start = text.index("U_CAPI const char* U_EXPORT2\nuprv_detectWindowsTimeZone()")
    end = text.index("\nU_NAMESPACE_END", start)
    output.mkdir(parents=True, exist_ok=True)
    (output / "icu-timezone-under-test.inc").write_text(text[start:end])
    for fixture, libraries in [("icu-smoke", ["libicui18n.a", "libicuuc.a", "libicudata.a"]),
                               ("icu-timezone-probe", ["libicuuc.a", "libicudata.a"])]:
        executable = output / (fixture + "-host")
        subprocess.run(["c++", "-std=c++17", "-O2", "-DU_STATIC_IMPLEMENTATION", "-I", str(source / "common"),
                        "-I", str(source / "i18n"), "-I", str(output), str(HERE / (fixture + ".cpp")),
                        *(str(work / "host-build/lib" / lib) for lib in libraries), "-lpthread", "-ldl",
                        "-o", str(executable)], check=True)
        result = subprocess.run([str(executable)], capture_output=True, text=True, check=True)
        (output / (fixture + "-host.log")).write_text(result.stdout)
        print(result.stdout, end="")
    print("Host checks passed; actual Win9x APIs were mocked only in the registry-boundary probe.")

if __name__ == "__main__":
    main()
