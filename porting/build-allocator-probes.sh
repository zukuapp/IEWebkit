#!/bin/sh
# Build actual-header probes; only the native host pair probe is executed here.
set -eu
if [ "$#" -ne 2 ]; then
    echo 'Usage: build-allocator-probes.sh PATCHED_WEBKIT_SOURCE EXTERNAL_OUTPUT' >&2
    exit 2
fi
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
python3 "$script_dir/paths.py" "$1" "$2"
source_dir=$(CDPATH= cd -- "$1" && pwd)
mkdir -p "$2"
output_dir=$(CDPATH= cd -- "$2" && pwd)
python3 "$script_dir/apply-webkit-patches.py" --source "$source_dir"
pas_include=$source_dir/Source/bmalloc/libpas/src/libpas
mi_include=$source_dir/Source/bmalloc/mimalloc/mimalloc/include
cc -std=gnu11 -O2 -pthread -I "$pas_include" "$script_dir/pas-pair-smoke.c" \
    -latomic -o "$output_dir/pas-pair-host"
"$output_dir/pas-pair-host"
i686-w64-mingw32-gcc -std=gnu11 -O2 -march=pentium3 -static -static-libgcc \
    -DIEWK_EXPECT_X86=1 -DWINVER=0x0410 -D_WIN32_WINNT=0x0400 \
    -I "$pas_include" "$script_dir/pas-pair-smoke.c" -o "$output_dir/pas-pair-smoke.exe"
i686-w64-mingw32-gcc -std=gnu11 -O2 -march=pentium3 -static -static-libgcc \
    -Werror=implicit-function-declaration -DWINVER=0x0410 \
    -D_WIN32_WINDOWS=0x0410 -D_WIN32_WINNT=0x0400 -I "$mi_include" \
    "$script_dir/mimalloc-lock-smoke.c" -o "$output_dir/mimalloc-lock-smoke.exe"
# Regression build: modern declarations must retain upstream SRW selection.
i686-w64-mingw32-gcc -std=gnu11 -O2 -static -static-libgcc \
    -Werror=implicit-function-declaration -DWINVER=0x0601 -D_WIN32_WINNT=0x0601 \
    -I "$mi_include" "$script_dir/mimalloc-lock-smoke.c" -o "$output_dir/mimalloc-lock-modern.exe"
i686-w64-mingw32-objdump -p "$output_dir/mimalloc-lock-modern.exe" \
    > "$output_dir/modern-imports.txt"
for symbol in AcquireSRWLockExclusive TryAcquireSRWLockExclusive ReleaseSRWLockExclusive; do
    grep -q "$symbol" "$output_dir/modern-imports.txt"
done
echo 'Built x86 probes; Windows binaries were not executed.'
