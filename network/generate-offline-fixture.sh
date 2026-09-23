#!/usr/bin/env bash
# Generate a disposable public test-only CA and server key outside the checkout.
set -euo pipefail
if [[ $# -ne 1 ]]; then
  echo "usage: $0 /absolute/external-fixture-dir" >&2
  exit 2
fi
fixture_dir=$(realpath -m "$1")
repo_root=$(realpath "$(dirname "$0")/..")
if [[ "$fixture_dir" == "$repo_root" || "$fixture_dir" == "$repo_root/"* ]]; then
  echo "fixture keys must stay outside the IEWebkit checkout" >&2
  exit 2
fi
if [[ -e "$fixture_dir/ca.key" || -e "$fixture_dir/server.key" ]]; then
  echo "refusing to overwrite an existing fixture key" >&2
  exit 2
fi
umask 077
mkdir -p "$fixture_dir"
openssl req -x509 -newkey rsa:2048 -sha256 -days 3650 -nodes \
  -keyout "$fixture_dir/ca.key" -out "$fixture_dir/ca.pem" \
  -subj '/CN=IEWebkit Test CA' \
  -addext 'basicConstraints=critical,CA:TRUE' \
  -addext 'keyUsage=critical,keyCertSign,cRLSign' \
  > "$fixture_dir/ca-gen.log" 2>&1
openssl req -newkey rsa:2048 -sha256 -nodes \
  -keyout "$fixture_dir/server.key" -out "$fixture_dir/server.csr" \
  -subj '/CN=iewebkit.invalid' \
  > "$fixture_dir/server-gen.log" 2>&1
openssl x509 -req -in "$fixture_dir/server.csr" \
  -CA "$fixture_dir/ca.pem" -CAkey "$fixture_dir/ca.key" -CAcreateserial \
  -out "$fixture_dir/server.pem" -days 3650 -sha256 \
  -extfile "$repo_root/network/testdata/server.ext" \
  > "$fixture_dir/sign.log" 2>&1
sha256sum "$fixture_dir/ca.pem" "$fixture_dir/server.pem" \
  "$fixture_dir/server.key" > "$fixture_dir/SHA256SUMS"
echo "Disposable fixture generated under $fixture_dir"
