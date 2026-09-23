#!/usr/bin/env python3
"""Build an exact development host variant outside the repository; no installer."""
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

import variants

ROOT = Path(__file__).resolve().parents[1]
# ME uses the conservative Windows 4.10 common API surface plus the explicit
# Win9x/ME declaration. _WIN32_WINNT is a header baseline, not an NT runtime claim.
OS_TARGETS = {
    'winme': ('0x0410', '0x0400', '0x04000000', '0x0490', (4, 0)),
    'winxp-sp3': ('0x0501', '0x0501', '0x05010300', None, (5, 1)),
    'winxp-x64-sp2': ('0x0502', '0x0502', '0x05020200', None, (5, 2)),
    'winvista-sp2': ('0x0600', '0x0600', '0x06000200', None, (6, 0)),
    'win7-sp1': ('0x0601', '0x0601', '0x06010100', None, (6, 1)),
    'win8': ('0x0602', '0x0602', '0x06020000', None, (6, 2)),
    'win81': ('0x0603', '0x0603', '0x06030000', None, (6, 3)),
}
# The Windows SDK defines IE110 as 0x0A00, the same header surface as IE100.
IE_TARGETS = {'5.5': '0x0550', '6': '0x0600', '7': '0x0700', '8': '0x0800',
              '9': '0x0900', '10': '0x0A00', '11': '0x0A00'}
SOURCES = ('include/engine.h', 'host/engine_loader.h', 'host/engine_contract.c',
           'host/engine_loader.cpp', 'host/docobject.cpp')


def parse_version(value):
    if not re.fullmatch(r'(0|[1-9][0-9]{0,4})(\.(0|[1-9][0-9]{0,4})){3}', value):
        raise ValueError('version must contain four decimal integers')
    result = tuple(int(part) for part in value.split('.'))
    if any(part > 65535 for part in result):
        raise ValueError('version component exceeds VERSIONINFO range')
    return result


def plan(data, profile_id, arch, mode, version, release=False, root=ROOT):
    variants.validate(data)
    profiles = [p for p in data['profiles'] if p['id'] == profile_id]
    if len(profiles) != 1:
        raise ValueError('unknown profile ID; no target fallback')
    p = profiles[0]
    target = variants.select(data, p['ie'], p['os'], arch, mode, release, root)
    if release:
        raise ValueError('this tool builds development host components, not certified engine releases')
    parse_version(version)
    if p['os'] not in OS_TARGETS:
        raise ValueError('OS has no reviewed compiler target')
    winver, winnt, ntddi, win9x, subsystem = OS_TARGETS[p['os']]
    definitions = {'WINVER': winver, '_WIN32_WINNT': winnt, 'NTDDI_VERSION': ntddi,
                   '_WIN32_IE': IE_TARGETS[p['ie']],
                   'IEWK_TARGET_IE': str(int(float(p['ie']) * 10)),
                   'IEWK_TARGET_SECURITY_MODE': str({'classic': 0, 'protected': 1, 'enhanced-protected': 2}[mode]),
                   'IEWK_HOST_DEVELOPMENT': '1'}
    if win9x:
        definitions['_WIN32_WINDOWS'] = win9x
    prefix = {'x86': 'i686-w64-mingw32-', 'x64': 'x86_64-w64-mingw32-'}[arch]
    return dict(target=target, version=version,
                build_id=target['artifact_id'] + '-host-v' + version,
                definitions=definitions, subsystem=list(subsystem),
                tools={name: prefix + suffix for name, suffix in
                       [('cc', 'gcc'), ('cxx', 'g++'), ('rc', 'windres'), ('objdump', 'objdump')]})


def resource_text(configuration):
    target = configuration['target']
    # All strings originate in the validated manifest or numeric version.
    version = configuration['version']
    numbers = ','.join(str(i) for i in parse_version(version))
    file_os = '0x00000004L' if target['os'] == 'winme' else '0x00040004L'
    values = {'FileDescription': 'IEWebkit development document host',
              'FileVersion': version, 'ProductName': 'IEWebkit Host Development',
              'ProductVersion': version, 'OriginalFilename': 'iewebkit-host.dll',
              'Variant': target['artifact_id'], 'InternetExplorer': target['ie'],
              'OperatingSystem': target['os'], 'ContentArchitecture': target['arch'],
              'SecurityMode': target['mode'], 'Certification': 'Unverified development component'}
    strings = '\n'.join(f'      VALUE "{key}", "{value}\\0"' for key, value in values.items())
    return f'''#include <winver.h>
1 VERSIONINFO
 FILEVERSION {numbers}
 PRODUCTVERSION {numbers}
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x2L
 FILEOS {file_os}
 FILETYPE 0x2L
 FILESUBTYPE 0x0L
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
{strings}
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
'''


def external_destination(work_dir, build_id, root=ROOT):
    work = work_dir.resolve()
    if work == root.resolve() or work.is_relative_to(root.resolve()):
        raise ValueError('build directory must be outside the repository')
    # build_id only comes from plan; protect this helper when used independently.
    if not re.fullmatch(r'ie[0-9]+-[a-z0-9-]+-host-v[0-9.]+', build_id):
        raise ValueError('invalid build ID')
    destination = work / build_id
    if destination.exists() or destination.is_symlink():
        raise ValueError('variant output already exists; choose another work directory or version')
    return destination


@contextmanager
def output_lock(destination):
    lock = destination.parent / ('.' + destination.name + '.build-lock')
    try:
        descriptor = os.open(lock, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    except FileExistsError as error:
        raise ValueError('this exact variant is already being built; inspect the existing build') from error
    try:
        os.close(descriptor)
        yield
    finally:
        lock.unlink()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build(configuration, work_dir, root=ROOT):
    destination = external_destination(work_dir, configuration['build_id'], root)
    resolved = {key: shutil.which(value) for key, value in configuration['tools'].items()}
    if not all(resolved.values()):
        missing = [configuration['tools'][key] for key, value in resolved.items() if not value]
        raise ValueError('missing target toolchain: ' + ', '.join(missing))
    # Pin the selected sources in a private external stage; concurrent agent
    # edits cannot mix translation units during compilation.
    source_bytes = {name: (root / name).read_bytes() for name in SOURCES}
    destination.parent.mkdir(parents=True, exist_ok=True)
    with output_lock(destination), tempfile.TemporaryDirectory(prefix='.iewebkit-host-', dir=destination.parent) as temporary:
        stage = Path(temporary)
        for name, content in source_bytes.items():
            copied = stage / 'source' / name
            copied.parent.mkdir(parents=True, exist_ok=True)
            copied.write_bytes(content)
        artifact = stage / 'artifact'
        artifact.mkdir()
        objects = artifact / 'objects'
        objects.mkdir()
        definitions = ['-D' + key + '=' + value for key, value in configuration['definitions'].items()]
        common = ['-Wall', '-Wextra', '-Werror', '-Os', '-I', str(stage / 'source/include'), *definitions]
        commands = []

        def run(argv):
            commands.append(argv)
            failed = False
            try:
                with (artifact / 'build.log').open('a') as log:
                    result = subprocess.run(argv, cwd=stage, stdout=log, stderr=subprocess.STDOUT, timeout=120)
                failed = result.returncode != 0
            except subprocess.TimeoutExpired:
                failed = True
            if failed:
                # Preserve diagnostics outside the repo without advertising a completed artifact.
                failure = destination.parent / (configuration['build_id'] + '.failed.log')
                shutil.copyfile(artifact / 'build.log', failure)
                raise ValueError('host compile failed or timed out; see ' + str(failure))

        for source, compiler, standard in [('engine_contract.c', 'cc', 'c99'),
                                            ('engine_loader.cpp', 'cxx', 'c++11'),
                                            ('docobject.cpp', 'cxx', 'c++11')]:
            run([resolved[compiler], '-std=' + standard, *common, '-c',
                 str(stage / 'source/host' / source), '-o', str(objects / (Path(source).stem + '.o'))])
        rc = artifact / 'variant.rc'
        rc.write_text(resource_text(configuration))
        run([resolved['rc'], *definitions, '-i', str(rc), '-o', str(objects / 'variant.o'), '-O', 'coff'])
        major, minor = configuration['subsystem']
        linker = (f'-Wl,--kill-at,--no-insert-timestamp,--major-os-version,{major},'
                  f'--minor-os-version,{minor},--major-subsystem-version,{major},--minor-subsystem-version,{minor}')
        run([resolved['cxx'], '-shared', '-static', '-static-libgcc', '-static-libstdc++', linker,
             '-o', str(artifact / 'iewebkit-host.dll'),
             *[str(objects / name) for name in ('docobject.o', 'engine_loader.o', 'engine_contract.o', 'variant.o')],
             '-ladvapi32', '-luser32', '-lole32', '-loleaut32', '-luuid', '-lurlmon'])
        pe = subprocess.run([resolved['objdump'], '-f', str(artifact / 'iewebkit-host.dll')],
                            capture_output=True, text=True, check=True, timeout=15).stdout
        expected = {'x86': 'pei-i386', 'x64': 'pei-x86-64'}[configuration['target']['arch']]
        if 'file format ' + expected not in pe:
            raise ValueError('linked host architecture does not match selected variant')
        (artifact / 'pe-architecture.txt').write_text(pe)
        metadata = dict(schema_version=1, product='IEWebkit', artifact_kind='development-host',
                        engine_included=False, guest_verified=False, release_eligible=False,
                        configuration=configuration,
                        source_sha256={name: hashlib.sha256(content).hexdigest() for name, content in source_bytes.items()},
                        artifact_sha256={str(p.relative_to(artifact)): digest(p) for p in artifact.rglob('*') if p.is_file()},
                        compiler_versions={key: subprocess.run([path, '--version'], capture_output=True,
                            text=True, check=True, timeout=15).stdout.splitlines()[0] for key, path in resolved.items()})
        # Replace temporary paths with a stable prefix in recorded argv.
        metadata['commands'] = [[arg.replace(str(stage), '$BUILD_STAGE') for arg in argv] for argv in commands]
        (artifact / 'build.json').write_text(json.dumps(metadata, indent=2) + '\n')
        # A failed or racing build must not overwrite a previous variant.
        if destination.exists() or destination.is_symlink():
            raise ValueError('variant output appeared during build; refusing overwrite')
        artifact.rename(destination)
    return destination


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--arch', required=True, choices=['x86', 'x64'])
    parser.add_argument('--mode', required=True, choices=['classic', 'protected', 'enhanced-protected'])
    parser.add_argument('--version', default='0.1.0.0')
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--release', action='store_true')
    args = parser.parse_args()
    try:
        configuration = plan(json.loads((ROOT / 'variants.json').read_text()), args.profile,
                             args.arch, args.mode, args.version, args.release)
        result = build(configuration, args.work_dir)
        print(json.dumps({'output': str(result), 'artifact_kind': 'development-host',
                          'guest_verified': False, 'engine_included': False}, indent=2))
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
