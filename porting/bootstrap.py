#!/usr/bin/env python3
"""Fetch checksum-pinned upstream sources; no installation or production writes."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import tarfile
import urllib.request
from paths import external_work_dir

HERE = Path(__file__).resolve().parent

def checked_fetch(url, target, expected):
    if target.is_file() and hashlib.sha256(target.read_bytes()).hexdigest() == expected:
        return
    with urllib.request.urlopen(url, timeout=60) as response:
        data = response.read()
    if hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f"Source checksum mismatch: {url}")
    target.parent.mkdir(parents=True, exist_ok=True)
    temporary = target.with_name(target.name + ".download")
    temporary.write_bytes(data)
    temporary.replace(target)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    pin = json.loads((HERE / "webkit-source.json").read_text())
    work = external_work_dir(args.work_dir)
    work.mkdir(parents=True, exist_ok=True)
    archive = work / (pin["archive_root"] + ".tar.xz")
    checked_fetch(pin["archive_url"], archive, pin["archive_sha256"])
    source = work / pin["archive_root"]
    stamp = source / ".iewebkit-hydrated.json"
    if source.exists():
        if not stamp.exists() or json.loads(stamp.read_text()) != pin:
            raise SystemExit("Source directory already exists without the matching pin stamp; use a fresh work directory to preserve edits.")
    else:
        with tarfile.open(archive) as bundle:
            for member in bundle.getmembers():
                parts = Path(member.name).parts
                if not parts or parts[0] != pin["archive_root"] or ".." in parts:
                    raise ValueError("Unexpected source archive path")
            bundle.extractall(work, filter="data")
        # Official GTK release archives exclude Windows platform sources.
        # Hydrate those exact files from the corresponding upstream Git tag.
        def hydrate(item):
            path, digest = item
            checked_fetch(pin["supplement_base"] + path, source / path, digest)
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            list(pool.map(hydrate, pin["supplement_files"].items()))
        stamp.write_text(json.dumps(pin, indent=2) + "\n")
    print(source)

if __name__ == "__main__":
    main()
