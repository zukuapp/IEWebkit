#!/usr/bin/env python3
"""Test compiler-specific thread-name code extracted from pinned ThreadingWin."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from paths import external_work_dir
HERE=Path(__file__).resolve().parent

def run(command):
    result=subprocess.run(command,text=True,capture_output=True)
    if result.returncode: raise SystemExit(result.stderr)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    source,output=external_work_dir(args.source),external_work_dir(args.output)
    item=next(p for p in json.loads((HERE/'webkit-patches.json').read_text())['patches'] if p['path']=='Source/WTF/wtf/win/ThreadingWin.cpp')
    body=(source/item['path']).read_bytes()
    if hashlib.sha256(body).hexdigest()!=item['after_sha256']:
        raise SystemExit('ThreadingWin must match the reviewed source patch')
    text=body.decode(); start=text.index('#if COMPILER(MSVC)\n// MS_VC_EXCEPTION');end=text.index('\nvoid Thread::initializePlatformThreading()',start)
    output.mkdir(parents=True,exist_ok=True)
    (output/'thread-name-under-test.inc').write_text(text[start:end])
    fixture=HERE/'thread-name-smoke.cpp';host=output/'thread-name-host'
    run(['c++','-std=c++17','-O2','-I',str(output),str(fixture),'-o',str(host)])
    result=run([str(host)]);(output/'host.log').write_text(result.stdout);print(result.stdout,end='')
    run(['i686-w64-mingw32-g++','-std=c++17','-Os','-static','-static-libgcc','-static-libstdc++','-DIEWK_REAL_WIN32','-DWINVER=0x0410','-D_WIN32_WINDOWS=0x0410','-D_WIN32_WINNT=0x0400','-I',str(output),str(fixture),'-o',str(output/'thread-name-me.exe')])
    for arch in ('i686','x86_64'):
        run(['clang++','--target='+arch+'-pc-windows-msvc','-std=c++17','-fms-extensions','-I',str(output),'-c',str(HERE/'thread-name-msvc-syntax.cpp'),'-o',str(output/('msvc-'+arch+'.obj'))])
    print('Real MSVC-target SEH syntax compiles for x86/x64; native ME fixture built, not executed.')
if __name__=='__main__': main()
