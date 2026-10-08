#!/usr/bin/env python3
"""Package the complete development host matrix; never certify a browser engine.

Only host DLLs, build/PE metadata and explicit license notices are included.
The source snapshot must match an existing Git commit. Windows guest images,
system DLLs, logs, objects and private lab files are never discovered or copied.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import zipfile

import build_host_matrix as matrix
import build_host_variant as builder
import verify_host_pe

ROOT = builder.ROOT
HOST_FILES = ('iewebkit-host.dll', 'build.json', 'pe-verification.json')
NOTICE_FILES = ('LICENSE', 'THIRD_PARTY_NOTICES.md')
DISCLAIMER = ('Development host components only. No WebKit/JavaScriptCore engine is '
              'included. No IE/Windows/security-mode guest compatibility is certified. '
              'Protected Mode and Enhanced Protected Mode targets are build declarations, '
              'not verified integration. Do not install as a working replacement browser.\n')


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(content):
    return hashlib.sha256(content).hexdigest()


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + '\n').encode()


def read_file(path):
    require(path.is_file() and not path.is_symlink(), 'missing or symlinked file: ' + str(path))
    return path.read_bytes()


def git(root, *arguments):
    return subprocess.run(['git', '-C', str(root), *arguments], check=True,
                          capture_output=True, timeout=30).stdout


def source_snapshot(root, revision):
    commit = git(root, 'rev-parse', '--verify', '--end-of-options', revision + '^{commit}').decode().strip()
    require(re.fullmatch(r'[0-9a-f]{40}|[0-9a-f]{64}', commit), 'invalid Git commit')
    names = (*builder.SOURCES, 'variants.json', *NOTICE_FILES)
    sources = {name: git(root, 'show', commit + ':' + name) for name in names}
    return commit, sources


def development_flags(value):
    for key in ('engine_included', 'guest_verified', 'release_eligible'):
        require(value.get(key) is False, 'development metadata must explicitly keep ' + key + '=false')


def collect_payload(matrix_path, source_files):
    report_raw = read_file(matrix_path)
    report = json.loads(report_raw)
    require(report.get('schema_version') == 1 and report.get('artifact_kind') == 'development-host',
            'not a development host matrix')
    development_flags(report)
    require(report.get('passed') is True, 'matrix contains failed builds')
    source_hashes = {name: digest(source_files[name]) for name in builder.SOURCES}
    require(report.get('source_sha256') == source_hashes, 'matrix source hashes do not match Git revision')
    data = json.loads(source_files['variants.json'])
    manifest_hash = digest(json.dumps(data, sort_keys=True).encode())
    require(report.get('manifest_sha256') == manifest_hash, 'target manifest does not match Git revision')
    results = report.get('results', [])
    require(isinstance(results, list) and results, 'missing matrix results')
    # All builds must carry a single numeric file version and the exact declared tuple.
    first = results[0]
    require(isinstance(first, dict) and isinstance(first.get('output'), str), 'invalid matrix result')
    first_output = Path(first['output'])
    require(first_output.resolve().parent == matrix_path.parent.resolve() and not first_output.is_symlink(),
            'matrix output escapes its work directory')
    version = json.loads(read_file(first_output / 'build.json'))['configuration']['version']
    plans = {p['target']['artifact_id']: p for p in matrix.configurations(data, version)}
    identities = [r.get('artifact_id') for r in results if isinstance(r, dict)]
    require(len(identities) == len(results) == len(plans) and set(identities) == set(plans),
            'matrix must contain every exact declared variant once')
    require(report.get('variant_count') == len(plans) and
            report.get('profile_count') == len(data['profiles']), 'matrix counts do not match manifest')
    payload = {name: source_files[name] for name in NOTICE_FILES}
    payload['DEVELOPMENT.txt'] = DISCLAIMER.encode()
    clean_results = []
    for result in sorted(results, key=lambda item: item['artifact_id']):
        plan = plans[result['artifact_id']]
        target = plan['target']
        require(result.get('passed') is True and result.get('engine_included') is False and
                result.get('guest_verified') is False, 'invalid or certified matrix result')
        require(result.get('profile') == 'ie' + target['ie'].replace('.', '') + '-' + target['os'],
                'matrix profile does not match selected target')
        directory = Path(result['output'])
        require(not directory.is_symlink() and directory.resolve().parent == matrix_path.parent.resolve() and
                directory.resolve() ==
                (matrix_path.parent / plan['build_id']).resolve(), 'variant output escapes expected directory')
        files = {name: read_file(directory / name) for name in HOST_FILES}
        metadata = json.loads(files['build.json'])
        require(metadata.get('schema_version') == 1 and metadata.get('product') == 'IEWebkit' and
                metadata.get('artifact_kind') == 'development-host', 'invalid host build metadata')
        development_flags(metadata)
        require(metadata.get('configuration') == plan, 'host configuration does not match declared target')
        require(metadata.get('source_sha256') == source_hashes, 'host source hashes do not match Git revision')
        for name in ('iewebkit-host.dll', 'pe-verification.json'):
            require(metadata.get('artifact_sha256', {}).get(name) == digest(files[name]),
                    'host artifact digest mismatch: ' + name)
        require(result.get('sha256') == digest(files['iewebkit-host.dll']) and
                result.get('verification_sha256') == digest(files['pe-verification.json']),
                'matrix artifact digest mismatch')
        actual = verify_host_pe.verify(files['iewebkit-host.dll'], plan)
        require(json.loads(files['pe-verification.json']) == actual, 'PE verification record does not match DLL bytes')
        for name, content in files.items():
            payload['hosts/' + plan['build_id'] + '/' + name] = content
        clean_results.append({key: value for key, value in result.items() if key != 'output'})
    # Absolute work directories are operational details, not release metadata.
    cleaned = dict(report, results=clean_results)
    payload['host-matrix.json'] = json_bytes(cleaned)
    return payload, report, version, digest(report_raw)


def runtime_payload(paths):
    require(paths, 'explicit compiler/runtime license notices are required; use --runtime-notice')
    payload = {}
    for path in paths:
        name = path.name
        require(re.fullmatch(r'(COPYING[0-9]*(?:\.[A-Za-z0-9_-]+)*|DISCLAIMER(?:\.[A-Za-z0-9_-]+)*|'
                             r'LICENSE(?:\.[A-Za-z0-9_-]+)*|copyright)', name),
                'runtime notice must be a COPYING, DISCLAIMER, LICENSE or copyright file')
        require(re.fullmatch(r'[A-Za-z0-9_+.-]+', path.parent.name), 'invalid runtime notice directory name')
        archive_name = 'runtime-notices/' + path.parent.name + '/' + name
        require(archive_name not in payload, 'duplicate runtime notice name: ' + archive_name)
        payload[archive_name] = read_file(path)
    return payload


def package(matrix_path, output_dir, tag, revision='HEAD', runtime_notices=(), root=ROOT):
    require(re.fullmatch(r'v[0-9][A-Za-z0-9._-]*-dev(?:[._-][A-Za-z0-9._-]+)?', tag),
            'tag must identify a development prerelease, for example v0.1.0-dev.20261008')
    output = output_dir.resolve()
    require(output != root.resolve() and not output.is_relative_to(root.resolve()),
            'package output must be outside the repository')
    require(not output.exists(), 'package output already exists; choose a new output directory')
    matrix_path = matrix_path.absolute()
    require(not matrix_path.resolve().is_relative_to(root.resolve()), 'matrix must be outside the repository')
    commit, source_files = source_snapshot(root, revision)
    payload, report, version, report_hash = collect_payload(matrix_path, source_files)
    payload.update(runtime_payload(runtime_notices))
    metadata = dict(schema_version=1, product='IEWebkit', tag=tag, source_revision=commit,
                    artifact_kind='development-host', engine_included=False, guest_verified=False,
                    release_eligible=False, profile_count=report['profile_count'],
                    variant_count=report['variant_count'], host_file_version=version,
                    original_matrix_sha256=report_hash, source_sha256=report['source_sha256'],
                    file_sha256={name: digest(content) for name, content in sorted(payload.items())})
    metadata_raw = json_bytes(metadata)
    payload['release-metadata.json'] = metadata_raw
    archive_name = 'IEWebkit-' + tag + '-development-hosts.zip'
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.iewebkit-package-', dir=output.parent) as temporary:
        stage = Path(temporary)
        with zipfile.ZipFile(stage / archive_name, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for name, content in sorted(payload.items()):
                entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.create_system = 3
                entry.external_attr = 0o100644 << 16
                archive.writestr(entry, content)
        (stage / 'release-metadata.json').write_bytes(metadata_raw)
        sums = {name: digest((stage / name).read_bytes()) for name in (archive_name, 'release-metadata.json')}
        (stage / 'SHA256SUMS').write_text(''.join(value + '  ' + name + '\n' for name, value in sorted(sums.items())))
        require(not output.exists(), 'package output appeared during packaging; refusing overwrite')
        stage.rename(output)
    return dict(output=str(output), source_revision=commit, profile_count=metadata['profile_count'],
                variant_count=metadata['variant_count'], engine_included=False, guest_verified=False,
                release_eligible=False, asset_sha256=sums)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--matrix', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--tag', required=True)
    parser.add_argument('--revision', default='HEAD')
    parser.add_argument('--runtime-notice', type=Path, action='append', required=True)
    args = parser.parse_args()
    try:
        result = package(args.matrix, args.output_dir, args.tag, args.revision, args.runtime_notice)
        print(json.dumps(result, indent=2))
        return 0
    except (OSError, ValueError, KeyError, TypeError, subprocess.SubprocessError) as error:
        print(str(error), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())
