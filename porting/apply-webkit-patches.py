#!/usr/bin/env python3
"""Apply reviewed patches only to exact pinned source files, preserving other edits."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
from paths import external_work_dir

HERE = Path(__file__).resolve().parent

def digest(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path)
    args = parser.parse_args()
    source = external_work_dir(args.source)
    pin = json.loads((HERE / "webkit-source.json").read_text())
    stamp = json.loads((source / ".iewebkit-hydrated.json").read_text())
    manifest = json.loads((HERE / "webkit-patches.json").read_text())
    if stamp != pin or manifest["source_commit"] != pin["commit"]:
        raise SystemExit("Source/patch pin mismatch")
    for item in manifest["patches"]:
        relative = Path(item["path"])
        if relative.is_absolute() or ".." in relative.parts:
            raise SystemExit("Invalid patch target")
        target = source / relative
        if target.resolve() != target or not target.is_file():
            raise SystemExit("Patch target must be a regular non-symlink file")
        before = target.read_bytes()
        current = digest(before)
        if current == item["after_sha256"]:
            print("Already applied:", item["patch"])
            continue
        if current != item["before_sha256"]:
            raise SystemExit(f"Preserving unrecognized local edits: {relative}")
        patch = (HERE / item["patch"]).read_bytes()
        if digest(patch) != item["patch_sha256"]:
            raise SystemExit("Patch checksum mismatch")
        with tempfile.TemporaryDirectory(prefix=".iewebkit-patch-", dir=source.parent) as folder:
            staged = Path(folder) / relative
            staged.parent.mkdir(parents=True, exist_ok=True)
            staged.write_bytes(before)
            subprocess.run(["patch", "--batch", "--fuzz=0", "-p1", "-d", folder], input=patch, check=True, capture_output=True)
            if digest(staged.read_bytes()) != item["after_sha256"]:
                raise SystemExit("Patched source checksum mismatch")
            staged.chmod(target.stat().st_mode)
            os.replace(staged, target)
        print("Applied:", item["patch"])

if __name__ == "__main__":
    main()
