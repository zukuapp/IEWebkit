#!/usr/bin/env python3
"""Fetch pinned ICU source for cross-build preparation, never install globally."""
import argparse
import hashlib
import json
from pathlib import Path
import tarfile
from bootstrap import checked_fetch
from paths import external_work_dir

HERE = Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--work-dir", required=True, type=Path)
    args = parser.parse_args()
    pin = json.loads((HERE / "icu-source.json").read_text())
    work = external_work_dir(args.work_dir)
    work.mkdir(parents=True, exist_ok=True)
    archive = work / "icu4c-78.3-sources.tgz"
    sums = work / "SHASUM512.txt"
    checked_fetch(pin["archive_url"], archive, pin["archive_sha256"])
    checked_fetch(pin["published_sums_url"], sums, pin["published_sums_sha256"])
    digest = hashlib.sha512(archive.read_bytes()).hexdigest()
    if digest != pin["archive_sha512"]:
        raise SystemExit("ICU SHA512 mismatch")
    matching = [line.split()[0] for line in sums.read_text().splitlines() if line.endswith(archive.name)]
    if matching != [digest]:
        raise SystemExit("Published ICU checksum mismatch")
    source = work / pin["archive_root"]
    stamp = source / ".iewebkit-icu-source.json"
    if source.exists():
        if not stamp.exists() or json.loads(stamp.read_text()) != pin:
            raise SystemExit("Existing source tree has no matching pin stamp; preserve it and select a fresh work directory.")
    else:
        with tarfile.open(archive) as bundle:
            for member in bundle.getmembers():
                parts = Path(member.name).parts
                if not parts or parts[0] != pin["archive_root"] or ".." in parts:
                    raise ValueError("Unexpected ICU archive path")
            bundle.extractall(work, filter="data")
        stamp.write_text(json.dumps(pin, indent=2) + "\n")
    print(source)

if __name__ == "__main__":
    main()
