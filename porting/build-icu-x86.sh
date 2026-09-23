#!/bin/sh
# Targeted x86 ICU bootstrap; a Win9x profile requires separate guest validation.
set -eu
if [ "$#" -lt 1 ] || [ "$#" -gt 3 ]; then
    echo 'Usage: build-icu-x86.sh EXTERNAL_WORK_DIRECTORY [JOBS:1-or-2] [win7|win9x]' >&2
    exit 2
fi
jobs=${2:-2}
case "$jobs" in 1|2) ;; *) echo 'Use 1 or 2 build jobs.' >&2; exit 2;; esac
profile=${3:-win7}
case "$profile" in win7|win9x) ;; *) echo 'Select win7 or win9x profile.' >&2; exit 2;; esac
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
python3 "$script_dir/paths.py" "$1"
mkdir -p "$1"
work=$(CDPATH= cd -- "$1" && pwd)
python3 "$script_dir/bootstrap-icu.py" --work-dir "$work"
target_dir=$work/target-build
target_prefix=$work/prefix-x86
target_flags=-Os
if [ "$profile" = win9x ]; then
    python3 "$script_dir/apply-webkit-patches.py" --dependency icu --source "$work/icu"
    target_dir=$work/target-build-win9x
    target_prefix=$work/prefix-x86-win9x
    target_flags='-Os -DU_IEWEBKIT_WIN9X=1 -DUCONFIG_USE_WINDOWS_LCID_MAPPING_API=0'
fi
mkdir -p "$work/host-build" "$target_dir" "$target_prefix"
cd "$work/host-build"
"$work/icu/source/configure" --prefix="$work/prefix-host" \
    --enable-static --disable-shared --disable-tests --disable-samples \
    --disable-extras --with-data-packaging=static > "$work/host-configure.log" 2>&1
make -j"$jobs" > "$work/host-build.log" 2>&1
cd "$target_dir"
build_triplet=$(sh "$work/icu/source/config.guess")
"$work/icu/source/configure" --host=i686-w64-mingw32 \
    --build="$build_triplet" \
    --with-cross-build="$work/host-build" --prefix="$target_prefix" \
    --enable-static --disable-shared --disable-tests --disable-samples \
    --disable-extras --disable-tools --disable-icuio --with-data-packaging=static \
    CC=i686-w64-mingw32-gcc CXX=i686-w64-mingw32-g++ \
    AR=i686-w64-mingw32-ar RANLIB=i686-w64-mingw32-ranlib \
    CFLAGS="$target_flags" CXXFLAGS="$target_flags" > "$work/target-$profile-configure.log" 2>&1
make -j"$jobs" > "$work/target-$profile-build.log" 2>&1
# ICU's MinGW defaults move even the static data archive to bin; keep it
# beside uc/i18n and replace the temporary stub-data archive correctly.
make install MINGW_MOVEDLLSTOBINDIR=NO > "$work/target-$profile-install.log" 2>&1
cmp "$target_dir/lib/libsicudt.a" "$target_prefix/lib/libsicudt.a"
printf '%s\n' "$target_prefix"
