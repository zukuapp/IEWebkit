#!/usr/bin/env bash
# Package only a pinned, disposable TLS guest fixture on read-only media.
set -euo pipefail
umask 077

if [[ $# -ne 5 ]]; then
  echo "usage: $0 BUILD_DIR FIXTURE_DIR QA_HELPER_EXE QA_HELPER_IMPORT_JSON OUTPUT_ISO" >&2
  exit 2
fi
build_dir=$(realpath "$1")
fixture_dir=$(realpath "$2")
helper_exe=$(realpath "$3")
helper_imports=$(realpath "$4")
output_iso=$(realpath -m "$5")
repo_root=$(realpath "$(dirname "$0")/..")
if [[ "$output_iso" == "$repo_root" || "$output_iso" == "$repo_root/"* ]]; then
  echo "ISO must stay outside the IEWebkit checkout" >&2
  exit 2
fi
if [[ -e "$output_iso" || -e "$build_dir/offline-media" ]]; then
  echo "refusing to overwrite existing ISO or media tree" >&2
  exit 2
fi
suffix=$(sed -n 's/^IEWK_QA_LOG_SUFFIX=//p' "$build_dir/qa-log-suffix.txt")
if [[ ! "$suffix" =~ ^[0-9]{1,2}$ ]]; then
  echo "a unique one- or two-digit QA log suffix is required" >&2
  exit 2
fi
python3 - "$build_dir" "$helper_exe" "$helper_imports" <<'PY'
import hashlib
import json
import sys
from pathlib import Path

build, helper, helper_audit = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
for binary, report in (
    (build / "tls-offline.exe", build / "tls-offline-imports.json"),
    (build / "tls-runner.exe", build / "tls-runner-imports.json"),
    (helper, helper_audit),
):
    audit = json.loads(report.read_text())
    digest = hashlib.sha256(binary.read_bytes()).hexdigest()
    if not audit.get("import_compatible") or audit.get("binary_sha256") != digest:
        raise SystemExit(f"missing or mismatched pinned PE import audit: {binary}")
PY

media="$build_dir/offline-media"
mkdir -m 700 "$media"
install -m 600 "$build_dir/tls-offline.exe" "$media/TLSOFF.EXE"
install -m 600 "$build_dir/tls-offline.exe" "$media/ZUKUDIAG.EXE"
install -m 600 "$build_dir/tls-runner.exe" "$media/TLSRUN.EXE"
install -m 600 "$helper_exe" "$media/ZUKUQA.EXE"
install -m 600 "$fixture_dir/ca.pem" "$media/CA.PEM"
install -m 600 "$fixture_dir/otherca.pem" "$media/OTHERCA.PEM"
install -m 600 "$fixture_dir/server.pem" "$media/SERVER.PEM"
install -m 600 "$fixture_dir/server.key" "$media/SERVER.KEY"
xorriso -as mkisofs -iso-level 1 -V "IEWKR${suffix}QA" -o "$output_iso" "$media"
chmod 600 "$output_iso"

verify_dir=$(mktemp -d "$build_dir/.offline-iso-verify.XXXXXX")
trap 'rm -rf "$verify_dir"' EXIT
xorriso -osirrox on -indev "$output_iso" -extract / "$verify_dir"
for file in TLSOFF.EXE ZUKUDIAG.EXE TLSRUN.EXE ZUKUQA.EXE CA.PEM \
            OTHERCA.PEM SERVER.PEM SERVER.KEY; do
  cmp "$media/$file" "$verify_dir/$file"
done
sha256sum "$output_iso" "$media"/* > "$build_dir/offline-iso-SHA256SUMS"
echo "Verified read-only guest ISO: $output_iso"
