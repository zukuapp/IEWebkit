#!/usr/bin/env python3
"""Test the exact patched WTF truncate body, retaining a modern-path regression."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from paths import external_work_dir

HERE = Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    source, output = external_work_dir(args.source), external_work_dir(args.output)
    patches = json.loads((HERE / 'webkit-patches.json').read_text())['patches']
    item = next(p for p in patches if p['path'] == 'Source/WTF/wtf/win/FileHandleWin.cpp')
    body = (source / item['path']).read_bytes()
    if hashlib.sha256(body).hexdigest() != item['after_sha256']:
        raise SystemExit('FileHandleWin must match the reviewed source patch')
    text = body.decode()
    start = text.index('bool FileHandle::truncate(')
    end = text.index('\nstd::optional<uint64_t> FileHandle::seek', start)
    output.mkdir(parents=True, exist_ok=True)
    (output / 'file-truncate-under-test.inc').write_text(text[start:end])
    for name, version in [('win9x', '0x0400'), ('nt5', '0x0501'), ('modern', '0x0600')]:
        binary = output / ('file-truncate-' + name)
        subprocess.run(['c++', '-std=c++17', '-O2', '-D_WIN32_WINNT=' + version,
                        '-I', str(output), str(HERE / 'file-truncate-smoke.cpp'), '-o', str(binary)], check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True, check=True)
        (output / (name + '.log')).write_text(result.stdout)
        print(name + ': ' + result.stdout, end='')
    subprocess.run(['i686-w64-mingw32-g++', '-std=c++17', '-Os', '-march=pentium3', '-static',
                    '-static-libgcc', '-static-libstdc++', '-DIEWK_REAL_WIN32=1', '-DWINVER=0x0410',
                    '-D_WIN32_WINDOWS=0x0410', '-D_WIN32_WINNT=0x0400', '-I', str(output),
                    str(HERE / 'file-truncate-smoke.cpp'), '-o', str(output / 'file-truncate-me.exe')], check=True)
    print('Native probe built; no guest execution implied.')

if __name__ == '__main__':
    main()
