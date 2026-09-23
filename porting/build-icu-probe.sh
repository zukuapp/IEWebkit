#!/bin/sh
set -eu
if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo 'Usage: build-icu-probe.sh EXTERNAL_ICU_PREFIX EXTERNAL_OUTPUT [korean-guest]' >&2
    exit 2
fi
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
python3 "$script_dir/paths.py" "$1" "$2"
icu_prefix=$(CDPATH= cd -- "$1" && pwd)
mkdir -p "$2"
output=$(CDPATH= cd -- "$2" && pwd)
case ${3:-any-guest} in
    korean-guest) set -- -DIEWK_EXPECT_KOREAN_GUEST=1 ;;
    any-guest) set -- ;;
    *) echo 'Unknown guest expectation.' >&2; exit 2 ;;
esac
i686-w64-mingw32-g++ -std=c++17 -Os -march=pentium3 -static \
    -static-libgcc -static-libstdc++ -DU_STATIC_IMPLEMENTATION \
    -DWINVER=0x0410 -D_WIN32_WINDOWS=0x0410 -D_WIN32_WINNT=0x0400 \
    "$@" -I "$icu_prefix/include" "$script_dir/icu-smoke.cpp" \
    "$icu_prefix/lib/libsicuin.a" "$icu_prefix/lib/libsicuuc.a" \
    "$icu_prefix/lib/libsicudt.a" -ladvapi32 -o "$output/icu-smoke.exe"
echo 'Built ICU probe; execute and inspect ICUDIAG.LOG in the actual target guest.'
