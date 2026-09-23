#!/bin/sh
# Interpreter bootstrap only: this is not the complete IEWebkit feature profile.
set -eu
if [ "$#" -ne 2 ]; then
    echo 'Usage: configure-jsc-me.sh SOURCE_DIRECTORY BUILD_DIRECTORY' >&2
    exit 2
fi
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
python3 "$script_dir/paths.py" "$1" "$2"
exec cmake -S "$1" -B "$2" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$script_dir/mingw-me-toolchain.cmake" \
    -DPORT=JSCOnly -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DENABLE_JIT=OFF -DENABLE_C_LOOP=ON -DENABLE_WEBASSEMBLY=OFF \
    -DENABLE_REMOTE_INSPECTOR=OFF -DENABLE_STATIC_JSC=ON \
    -DENABLE_API_TESTS=OFF -DUSE_SYSTEM_MALLOC=ON
