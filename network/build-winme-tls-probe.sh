#!/usr/bin/env bash
# Reproduce the x86 Windows ME TLS dependency gate without vendoring OpenSSL.
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 /absolute/path/openssl-3.5.8.tar.gz /absolute/external-work-dir" >&2
  exit 2
fi
source_archive=$(realpath "$1")
work_root=$(realpath -m "$2")
repo_root=$(realpath "$(dirname "$0")/..")
if [[ "$work_root" == "$repo_root" || "$work_root" == "$repo_root/"* ]]; then
  echo "build outputs must stay outside the IEWebkit checkout" >&2
  exit 2
fi
if [[ -e "$work_root/openssl-3.5.8" ]]; then
  echo "refusing to reuse an existing source/build tree: $work_root/openssl-3.5.8" >&2
  exit 2
fi
printf '%s  %s\n' \
  'a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2' \
  "$source_archive" | sha256sum --check --status
mkdir -p "$work_root"
tar -xzf "$source_archive" -C "$work_root"
cd "$work_root/openssl-3.5.8"
perl Configure mingw \
  --cross-compile-prefix=i686-w64-mingw32- \
  --prefix="$work_root/install" \
  no-shared no-pinshared no-apps no-tests no-docs no-asm no-async \
  -DWINVER=0x0490 -D_WIN32_WINDOWS=0x0490 -D_WIN32_WINNT=0x0400 \
  > "$work_root/configure.log" 2>&1
make -j2 build_libs > "$work_root/build.log" 2>&1
make install_sw > "$work_root/install.log" 2>&1
i686-w64-mingw32-gcc -O2 -Wall -Wextra \
  -DWINVER=0x0490 -D_WIN32_WINDOWS=0x0490 -D_WIN32_WINNT=0x0400 \
  -I"$work_root/install/include" \
  -o "$work_root/tls-smoke.exe" "$repo_root/network/tls_smoke.c" \
  -L"$work_root/install/lib" -lssl -lcrypto \
  -lws2_32 -lgdi32 -luser32 -ladvapi32 -lcrypt32
i686-w64-mingw32-gcc -O2 -Wall -Wextra \
  -DWINVER=0x0490 -D_WIN32_WINDOWS=0x0490 -D_WIN32_WINNT=0x0400 \
  -I"$work_root/install/include" \
  -o "$work_root/tls-offline.exe" "$repo_root/network/tls_offline.c" \
  -L"$work_root/install/lib" -lssl -lcrypto \
  -lws2_32 -lgdi32 -luser32 -ladvapi32 -lcrypt32
sha256sum "$source_archive" "$work_root/tls-smoke.exe" \
  "$work_root/tls-offline.exe" > "$work_root/SHA256SUMS"
echo "Built: $work_root/tls-smoke.exe and $work_root/tls-offline.exe"
echo "Next: audit PE imports against checksum-pinned ME DLLs, then run in a real guest."
