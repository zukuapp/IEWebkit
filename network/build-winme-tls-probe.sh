#!/usr/bin/env bash
# Reproduce the x86 Windows ME TLS dependency gate without vendoring OpenSSL.
set -euo pipefail

if [[ $# -ne 2 && $# -ne 4 ]]; then
  echo "usage: $0 /absolute/path/openssl-3.5.8.tar.gz /absolute/external-work-dir [ME_DLL_DIR ME_BASELINE_JSON]" >&2
  exit 2
fi
source_archive=$(realpath "$1")
work_root=$(realpath -m "$2")
repo_root=$(realpath "$(dirname "$0")/..")
qa_log_suffix=${IEWK_QA_LOG_SUFFIX:-}
if [[ ! "$qa_log_suffix" =~ ^[0-9]{0,2}$ ]]; then
  echo "IEWK_QA_LOG_SUFFIX must contain zero to two digits" >&2
  exit 2
fi
qa_log_define="-DIEWK_QA_LOG_SUFFIX=\"$qa_log_suffix\""
if [[ $# -eq 4 ]]; then
  me_dll_dir=$(realpath "$3")
  me_baseline=$(realpath "$4")
  if [[ ! -d "$me_dll_dir" || ! -f "$me_baseline" ]]; then
    echo "ME DLL directory and checksum baseline are required together" >&2
    exit 2
  fi
fi
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
patch -p1 < "$repo_root/network/patches/openssl-3.5.8-win9x-critical-section.patch" \
  > "$work_root/patch.log"
patch -p1 < "$repo_root/network/patches/openssl-3.5.8-win9x-thread-backend.patch" \
  >> "$work_root/patch.log"
patch -p1 < "$repo_root/network/patches/openssl-3.5.8-win9x-cryptoapi-ansi.patch" \
  >> "$work_root/patch.log"
patch -p1 < "$repo_root/network/patches/openssl-3.5.8-win9x-cryptoapi-silent.patch" \
  >> "$work_root/patch.log"
perl Configure mingw \
  --cross-compile-prefix=i686-w64-mingw32- \
  --prefix="$work_root/install" \
  no-shared no-pinshared no-apps no-tests no-docs no-asm no-async no-comp \
  no-capieng \
  -DWINVER=0x0490 -D_WIN32_WINDOWS=0x0490 -D_WIN32_WINNT=0x0400 \
  -DIEWK_WIN9X=1 \
  > "$work_root/configure.log" 2>&1
make -j2 build_libs > "$work_root/build.log" 2>&1
if ! rg -q '^[[:space:]]*#[[:space:]]*define OPENSSL_NO_COMP$' include/openssl/configuration.h; then
  echo "OpenSSL no-comp configuration was not generated" >&2
  exit 1
fi
i686-w64-mingw32-nm -g --defined-only \
  crypto/thread/arch/libcrypto-lib-thread_win.obj > "$work_root/thread-win-symbols.txt"
i686-w64-mingw32-nm -g --defined-only \
  crypto/thread/arch/libcrypto-lib-thread_none.obj > "$work_root/thread-none-symbols.txt"
for symbol in _ossl_crypto_mutex_new _ossl_crypto_condvar_new; do
  if ! rg -q " T ${symbol}$" "$work_root/thread-win-symbols.txt" ||
     rg -q " T ${symbol}$" "$work_root/thread-none-symbols.txt"; then
    echo "WinME thread backend selection failed for $symbol" >&2
    exit 1
  fi
done
make install_sw > "$work_root/install.log" 2>&1
i686-w64-mingw32-gcc -O2 -Wall -Wextra \
  -DWINVER=0x0490 -D_WIN32_WINDOWS=0x0490 -D_WIN32_WINNT=0x0400 \
  -I"$work_root/install/include" \
  -o "$work_root/tls-smoke.exe" "$repo_root/network/tls_smoke.c" \
  -L"$work_root/install/lib" -lssl -lcrypto \
  -lws2_32 -lgdi32 -luser32 -ladvapi32 -lcrypt32
i686-w64-mingw32-gcc -O2 -Wall -Wextra \
  -DWINVER=0x0490 -D_WIN32_WINDOWS=0x0490 -D_WIN32_WINNT=0x0400 \
  "$qa_log_define" \
  -I"$work_root/install/include" \
  -o "$work_root/tls-offline.exe" "$repo_root/network/tls_offline.c" \
  -L"$work_root/install/lib" -lssl -lcrypto \
  -lws2_32 -lgdi32 -luser32 -ladvapi32 -lcrypt32
i686-w64-mingw32-gcc -O2 -Wall -Wextra \
  -DWINVER=0x0490 -D_WIN32_WINDOWS=0x0490 -D_WIN32_WINNT=0x0400 \
  -I"$work_root/install/include" \
  -o "$work_root/tls-init-diag.exe" "$repo_root/network/tls_init_diag.c" \
  -L"$work_root/install/lib" -lssl -lcrypto \
  -lws2_32 -lgdi32 -luser32 -ladvapi32 -lcrypt32
i686-w64-mingw32-gcc -O2 -Wall -Wextra -Werror \
  -DWINVER=0x0490 -D_WIN32_WINDOWS=0x0490 -D_WIN32_WINNT=0x0400 \
  -I"$work_root/install/include" \
  -o "$work_root/tls-libctx-diag.exe" "$repo_root/network/tls_libctx_diag.c" \
  -L"$work_root/install/lib" -lcrypto \
  -lws2_32 -lgdi32 -luser32 -ladvapi32 -lcrypt32
i686-w64-mingw32-gcc -std=c99 -Os -Wall -Wextra -Werror -nostdlib \
  -fno-builtin -mwindows -Wl,--entry,_start@0 \
  -Wl,--subsystem,windows:4.0 \
  "$qa_log_define" \
  "$repo_root/network/tls_guest_runner.c" \
  -o "$work_root/tls-runner.exe" -lkernel32 -luser32
for binary in "$work_root/tls-smoke.exe" "$work_root/tls-offline.exe" \
              "$work_root/tls-init-diag.exe" "$work_root/tls-libctx-diag.exe"; do
  binary_name=$(basename "$binary" .exe)
  i686-w64-mingw32-nm -g --defined-only "$binary" \
    > "$work_root/$binary_name-symbols.txt"
  i686-w64-mingw32-objdump -p "$binary" \
    > "$work_root/$binary_name-pe.txt"
  for symbol in _ossl_crypto_mutex_new _ossl_crypto_condvar_new; do
    if ! rg -q " T ${symbol}$" "$work_root/$binary_name-symbols.txt"; then
      echo "linked PE lacks WinME $symbol: $binary" >&2
      exit 1
    fi
  done
  if rg -q 'InitializeCriticalSectionAndSpinCount|GetModuleHandleExW|CryptAcquireContextW' \
     "$work_root/$binary_name-pe.txt"; then
    echo "linked PE imports an unsupported ME routine: $binary" >&2
    exit 1
  fi
done
if [[ $# -eq 4 ]]; then
  for name in tls-smoke tls-offline tls-init-diag tls-libctx-diag tls-runner; do
    python3 "$repo_root/porting/audit-pe-imports.py" \
      --binary "$work_root/$name.exe" \
      --dll-dir "$me_dll_dir" --baseline "$me_baseline" \
      --output "$work_root/$name-imports.json" \
      > "$work_root/$name-import-audit.log"
  done
fi
printf 'IEWK_QA_LOG_SUFFIX=%s\n' "$qa_log_suffix" > "$work_root/qa-log-suffix.txt"
sha256sum "$source_archive" \
  "$repo_root/network/patches/openssl-3.5.8-win9x-critical-section.patch" \
  "$repo_root/network/patches/openssl-3.5.8-win9x-thread-backend.patch" \
  "$repo_root/network/patches/openssl-3.5.8-win9x-cryptoapi-ansi.patch" \
  "$repo_root/network/patches/openssl-3.5.8-win9x-cryptoapi-silent.patch" \
  "$repo_root/network/tls_offline.c" \
  "$repo_root/network/tls_guest_runner.c" \
  "$work_root/qa-log-suffix.txt" \
  "$work_root/tls-smoke.exe" \
  "$work_root/tls-offline.exe" "$work_root/tls-init-diag.exe" \
  "$work_root/tls-libctx-diag.exe" \
  "$work_root/tls-runner.exe" \
  > "$work_root/SHA256SUMS"
echo "Built: $work_root/tls-smoke.exe, tls-offline.exe, tls-init-diag.exe, tls-libctx-diag.exe and tls-runner.exe"
if [[ $# -eq 4 ]]; then
  echo "Pinned ME PE import audit passed for all five executables."
else
  echo "Next: audit PE imports against checksum-pinned ME DLLs, then run in a real guest."
fi
