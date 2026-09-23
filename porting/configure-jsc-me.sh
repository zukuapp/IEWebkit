#!/bin/sh
# Interpreter bootstrap only: this is not the complete IEWebkit feature profile.
set -eu
if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo 'Usage: configure-jsc-me.sh SOURCE_DIRECTORY BUILD_DIRECTORY [X86_STATIC_ICU_PREFIX]' >&2
    exit 2
fi
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
python3 "$script_dir/paths.py" "$1" "$2"
source_dir=$1
build_dir=$2
python3 "$script_dir/apply-webkit-patches.py" --source "$source_dir"
if [ "$#" -eq 3 ]; then
    python3 "$script_dir/paths.py" "$3"
    icu_prefix=$(CDPATH= cd -- "$3" && pwd)
    for library in libsicuuc.a libsicuin.a libsicudt.a; do
        test -f "$icu_prefix/lib/$library" || { echo "Missing target ICU library: $library" >&2; exit 2; }
    done
    # Explicit target paths prevent accidental linkage against host Linux ICU.
    set -- -DICU_INCLUDE_DIR="$icu_prefix/include" \
        -DICU_UC_LIBRARY_RELEASE="$icu_prefix/lib/libsicuuc.a" \
        -DICU_I18N_LIBRARY_RELEASE="$icu_prefix/lib/libsicuin.a" \
        -DICU_DATA_LIBRARY_RELEASE="$icu_prefix/lib/libsicudt.a" \
        '-DCMAKE_CXX_FLAGS=-DWINVER=0x0410 -D_WIN32_WINDOWS=0x0410 -D_WIN32_WINNT=0x0400 -DU_STATIC_IMPLEMENTATION'
else
    set --
fi
exec cmake -S "$source_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$script_dir/mingw-me-toolchain.cmake" \
    -DPORT=JSCOnly -DIEWEBKIT_WIN9X=ON -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DENABLE_JIT=OFF -DENABLE_C_LOOP=ON -DENABLE_WEBASSEMBLY=OFF \
    -DENABLE_REMOTE_INSPECTOR=OFF -DENABLE_STATIC_JSC=ON \
    -DENABLE_API_TESTS=OFF -DUSE_SYSTEM_MALLOC=OFF -DUSE_MIMALLOC=ON "$@"
