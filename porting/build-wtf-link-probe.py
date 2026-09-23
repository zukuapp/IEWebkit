#!/usr/bin/env python3
"""Link an actual WTF RandomDevice caller using the pinned Windows CMake lists.

This is a cross-build/link check, not an RNG, guest or full-engine runtime test.
Build the WTF target first in the supplied external JSCOnly build directory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
from paths import external_work_dir

HERE = Path(__file__).resolve().parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'build', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    source, build, output = [external_work_dir(getattr(args, name)) for name in ('source', 'build', 'output')]
    manifest = json.loads((HERE / 'webkit-patches.json').read_text())['patches']
    paths = ['Source/WTF/wtf/PlatformJSCOnly.cmake', 'Source/WTF/wtf/PlatformWin.cmake']
    for path in paths:
        entry = next(item for item in manifest if item['path'] == path)
        if hashlib.sha256((source / path).read_bytes()).hexdigest() != entry['after_sha256']:
            raise SystemExit('Source must match reviewed platform patch: ' + path)
    archive = build / 'lib/libWTF.a'
    if not archive.is_file():
        raise SystemExit('Build WTF first; this probe does not start an engine build')
    output.mkdir(parents=True, exist_ok=True)
    fixture = output / 'wtf-random-smoke.cpp'
    fixture.write_bytes((HERE / 'wtf-link-smoke.cpp').read_bytes())
    target = 'Source/WTF/wtf/CMakeFiles/WTF.dir/RandomDevice.cpp.obj'
    commands = subprocess.check_output(['ninja', '-C', str(build), '-t', 'commands', target], text=True)
    compile_args = shlex.split(commands.splitlines()[-1])
    object_path = output / 'wtf-random-smoke.o'
    for flag, value in [('-o', object_path), ('-MF', output / 'wtf-random-smoke.d'), ('-c', fixture)]:
        compile_args[compile_args.index(flag) + 1] = str(value)
    subprocess.run(compile_args, cwd=build, check=True)
    report = {'archive_sha256': hashlib.sha256(archive.read_bytes()).hexdigest(),
              'compile': compile_args, 'profiles': {}, 'guest_tested': False}
    for path, legacy in [(path, legacy) for path in paths for legacy in (True, False)]:
        profile = Path(path).stem + ("-win9x" if legacy else "-modern-libraries")
        # Bracket arguments avoid interpolation of path characters by CMake.
        if any(']==]' in str(value) for value in (source, output)):
            raise SystemExit('Unsupported CMake path delimiter')
        libraries_file = output / (profile + '-libraries.txt')
        script = output / (profile + '.cmake')
        script.write_text('set(WIN32 TRUE)\nset(CMAKE_SYSTEM_NAME Windows)\n'
                          f'set(IEWEBKIT_WIN9X {"TRUE" if legacy else "FALSE"})\n'
                          'set(LOWERCASE_EVENT_LOOP_TYPE generic)\n'
                          f'include([==[{source / path}]==])\n'
                          'list(JOIN WTF_LIBRARIES "\\n" required)\n'
                          f'file(WRITE [==[{libraries_file}]==] "${{required}}\\n")\n')
        subprocess.run(['cmake', '-P', str(script)], check=True)
        libraries = libraries_file.read_text().splitlines()
        if ('synchronization' in libraries) == legacy:
            raise SystemExit('Incorrect legacy/modern synchronization dependency')
        # Threads::Threads is an imported CMake target, not a literal library.
        # The actual compiler driver selects its own configured thread runtime.
        libraries = [value for value in libraries if value != 'Threads::Threads']
        if any(not value or '::' in value or value.startswith('-') for value in libraries):
            raise SystemExit('Unrecognized platform dependency')
        command = [compile_args[0], '-static', '-static-libgcc', '-static-libstdc++',
                   '-Wl,--gc-sections', str(object_path), str(archive)]
        command += ['-l' + value for value in libraries]
        command += [str(build / 'lib/libbmalloc.a'), '-latomic', '-lws2_32', '-ladvapi32',
                    '-o', str(output / (profile + '-wtf-random.exe'))]
        result = subprocess.run(command, capture_output=True, text=True)
        (output / (profile + '-link.log')).write_text(result.stdout + result.stderr)
        report['profiles'][profile] = {'libraries': libraries, 'link': command, 'exit_code': result.returncode}
        (output / 'provenance.json').write_text(json.dumps(report, indent=2) + '\n')
        if result.returncode:
            raise SystemExit(result.stderr)
        print(profile + ': actual WTF caller linked; runtime untested')


if __name__ == '__main__':
    main()
