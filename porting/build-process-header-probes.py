#!/usr/bin/env python3
"""Check the actual process header's include isolation, C ABI and visibility."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from paths import external_work_dir
HERE = Path(__file__).resolve().parent

def run(command):
    result = subprocess.run(command,text=True,capture_output=True)
    if result.returncode:
        raise SystemExit(result.stderr)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    source,output=external_work_dir(args.source),external_work_dir(args.output)
    items=json.loads((HERE/'webkit-patches.json').read_text())['patches']
    for name in ('pas_process.h','pas_process.c'):
        item=next(p for p in items if p['path'].endswith('/'+name))
        if hashlib.sha256((source/item['path']).read_bytes()).hexdigest()!=item['after_sha256']:
            raise SystemExit(name+' must match the reviewed patch')
    output.mkdir(parents=True,exist_ok=True)
    include=source/'Source/bmalloc/libpas/src/libpas'
    for target,cc,cxx,flags in [('host','cc','c++',[]),
            ('me','i686-w64-mingw32-gcc','i686-w64-mingw32-g++',['-DWINVER=0x0410','-D_WIN32_WINDOWS=0x0410','-D_WIN32_WINNT=0x0400']),
            ('modern','i686-w64-mingw32-gcc','i686-w64-mingw32-g++',['-D_WIN32_WINNT=0x0a00'])]:
        for hidden in (0,1):
            obj=output/f'{target}-hidden{hidden}.obj'
            common=['-O2','-Wundef','-Werror=undef','-DPAS_BMALLOC=1',f'-DPAS_BMALLOC_HIDDEN={hidden}','-I',str(include),*flags]
            run([cc,'-std=c11',*common,'-c',str(include/'pas_process.c'),'-o',str(obj)])
            if target=='host':
                symbol=next(line for line in run(['readelf','-Ws',str(obj)]).stdout.splitlines() if line.rstrip().endswith(' pas_process_is_shutting_down'))
                assert (' HIDDEN ' if hidden else ' DEFAULT ') in symbol, symbol
            for order,extra in [('first',['-DIEWK_PROCESS_HEADER_FIRST']),('last',[])]:
                binary=output/f'{target}-hidden{hidden}-{order}.exe'
                run([cxx,'-std=c++17',*(['-static'] if target!='host' else []),*common,*extra,str(HERE/'process-header-smoke.cpp'),str(obj),'-o',str(binary)])
                if target=='host': run([str(binary)])
        print(target+': both include orders and visibility settings compile/link; '+('POSIX behavior executed' if target=='host' else 'Windows not executed'))
    print('12 C/C++ links, 4 host executions and 2 ELF visibility checks passed.')
if __name__=='__main__': main()
