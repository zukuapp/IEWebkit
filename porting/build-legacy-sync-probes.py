#!/usr/bin/env python3
"""Exercise exact-source legacy mutex/condition methods with host Win32 event shims."""
import argparse, hashlib, json, subprocess
from pathlib import Path
from paths import external_work_dir
HERE=Path(__file__).resolve().parent

def run(command,timeout=60):
    result=subprocess.run(command,text=True,capture_output=True,timeout=timeout)
    if result.returncode: raise SystemExit(result.stderr or result.stdout)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();source,output=external_work_dir(args.source),external_work_dir(args.output)
    manifest=json.loads((HERE/'webkit-patches.json').read_text())['patches'];texts={}
    for name in ('ThreadingPrimitives.h','ThreadingWin.cpp'):
        item=next(p for p in manifest if p['path'].endswith('/'+name));body=(source/item['path']).read_bytes()
        if hashlib.sha256(body).hexdigest()!=item['after_sha256']: raise SystemExit(name+' source hash mismatch')
        texts[name]=body.decode()
    output.mkdir(parents=True,exist_ok=True)
    text=texts['ThreadingPrimitives.h'];start=text.index('#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600');end=text.index('using ThreadSpecificKey = DWORD;',start)
    (output/'legacy-sync-types.inc').write_text(text[start:end])
    text=texts['ThreadingWin.cpp'];start=text.index('#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0600\nstatic bool legacyMutexTryLock');end=text.index('\nvoid Thread::yield()',start)
    (output/'legacy-sync-methods.inc').write_text(text[start:end])
    fixture=HERE/'legacy-sync-smoke.cpp'
    for name,version in [('me','0x0400'),('nt5','0x0501')]:
        binary=output/('sync-host-'+name)
        run(['c++','-std=c++17','-O2','-pthread','-D_WIN32_WINNT='+version,'-I',str(output),str(fixture),'-o',str(binary)])
        result=run([str(binary)],30);(output/(name+'.log')).write_text(result.stdout);print(name+':\n'+result.stdout,end='')
    run(['i686-w64-mingw32-g++','-std=c++17','-Os','-march=pentium3','-static','-static-libgcc','-static-libstdc++','-DIEWK_REAL_WIN32','-DWINVER=0x0410','-D_WIN32_WINDOWS=0x0410','-D_WIN32_WINNT=0x0400','-I',str(output),str(fixture),'-o',str(output/'legacy-sync-me.exe')])
    print('Native ME fixture built only; host event shim does not certify Win9x APIs.')
if __name__=='__main__': main()
