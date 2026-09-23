#!/bin/sh
# Upstream's Windows 7 x86 ABI bootstrap, NOT a completed ME runtime port.
set -eu
if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo 'Usage: build-icu-x86.sh EXTERNAL_WORK_DIRECTORY [JOBS:1-or-2]' >&2
    exit 2
fi
jobs=${2:-2}
case "$jobs" in 1|2) ;; *) echo 'Use 1 or 2 build jobs.' >&2; exit 2;; esac
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
python3 "$script_dir/paths.py" "$1"
mkdir -p "$1"
work=$(CDPATH= cd -- "$1" && pwd)
python3 "$script_dir/bootstrap-icu.py" --work-dir "$work"
mkdir -p "$work/host-build" "$work/target-build" "$work/prefix-x86"
cd "$work/host-build"
"$work/icu/source/configure" --prefix="$work/prefix-host" \
    --enable-static --disable-shared --disable-tests --disable-samples \
    --disable-extras --with-data-packaging=static > "$work/host-configure.log" 2>&1
make -j"$jobs" > "$work/host-build.log" 2>&1
cd "$work/target-build"
build_triplet=$(sh "$work/icu/source/config.guess")
"$work/icu/source/configure" --host=i686-w64-mingw32 \
    --build="$build_triplet" \
    --with-cross-build="$work/host-build" --prefix="$work/prefix-x86" \
    --enable-static --disable-shared --disable-tests --disable-samples \
    --disable-extras --disable-tools --disable-icuio --with-data-packaging=static \
    CC=i686-w64-mingw32-gcc CXX=i686-w64-mingw32-g++ \
    AR=i686-w64-mingw32-ar RANLIB=i686-w64-mingw32-ranlib \
    CFLAGS=-Os CXXFLAGS=-Os > "$work/target-configure.log" 2>&1
make -j"$jobs" > "$work/target-build.log" 2>&1
# ICU's MinGW defaults move even the static data archive to bin; keep it
# beside uc/i18n and replace the temporary stub-data archive correctly.
make install MINGW_MOVEDLLSTOBINDIR=NO > "$work/target-install.log" 2>&1
cmp "$work/target-build/lib/libsicudt.a" "$work/prefix-x86/lib/libsicudt.a"
printf '%s\n' "$work/prefix-x86"
