#!/usr/bin/env python3
"""Build all declared development host combinations from one source snapshot."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import tempfile
import time

import build_host_variant as builder


def configurations(data, version, profile=None):
    plans = []
    for item in data['profiles']:
        if profile is not None and item['id'] != profile:
            continue
        for arch in item['architectures']:
            for mode in item['modes']:
                plans.append(builder.plan(data, item['id'], arch, mode, version))
    if not plans:
        raise ValueError('no matching profile; no fallback')
    return plans


def build_matrix(data, work, version='0.1.0.0', profile=None, jobs=2, root=builder.ROOT):
    if not 1 <= jobs <= 4:
        raise ValueError('jobs must be between 1 and 4')
    work = work.resolve()
    plans = configurations(data, version, profile)
    for plan in plans:
        builder.external_destination(work, plan['build_id'], root)
    work.mkdir(parents=True, exist_ok=True)
    report_path = work / ('matrix-' + (profile or 'all') + '.json')
    if report_path.exists():
        raise ValueError('matrix report already exists; use a new work directory')
    started = time.monotonic()
    # Read each source once for the entire matrix, not once per parallel target.
    sources = {name: (root / name).read_bytes() for name in builder.SOURCES}
    results = []
    with tempfile.TemporaryDirectory(prefix='.matrix-source-', dir=work) as directory:
        snapshot = Path(directory)
        for name, content in sources.items():
            path = snapshot / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)

        def compile_one(plan):
            begin = time.monotonic()
            result = {'artifact_id': plan['target']['artifact_id'], 'profile':
                      'ie' + plan['target']['ie'].replace('.', '') + '-' + plan['target']['os']}
            try:
                destination = builder.build(plan, work, root=snapshot)
                metadata = json.loads((destination / 'build.json').read_text())
                result.update(passed=True, sha256=metadata['artifact_sha256']['iewebkit-host.dll'],
                              output=str(destination), engine_included=False, guest_verified=False,
                              verification_sha256=metadata['artifact_sha256']['pe-verification.json'])
            except Exception as error:
                result.update(passed=False, error=str(error))
            result['elapsed_seconds'] = round(time.monotonic() - begin, 3)
            return result

        with ThreadPoolExecutor(max_workers=jobs) as executor:
            pending = [executor.submit(compile_one, plan) for plan in plans]
            for future in as_completed(pending):
                result = future.result()
                results.append(result)
                print(json.dumps(result), flush=True)
    report = dict(schema_version=1, artifact_kind='development-host', engine_included=False,
                  guest_verified=False, release_eligible=False,
                  profile_count=len({r['profile'] for r in results}), variant_count=len(results),
                  jobs=jobs, elapsed_seconds=round(time.monotonic() - started, 3),
                  passed=all(r['passed'] for r in results),
                  manifest_sha256=hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest(),
                  source_sha256={name: hashlib.sha256(content).hexdigest() for name, content in sources.items()},
                  results=sorted(results, key=lambda r: r['artifact_id']))
    report_path.write_text(json.dumps(report, indent=2) + '\n')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', required=True, type=Path)
    parser.add_argument('--profile')
    parser.add_argument('--version', default='0.1.0.0')
    parser.add_argument('--jobs', type=int, default=2)
    parser.add_argument('--list-profiles', action='store_true')
    args = parser.parse_args()
    data = json.loads((builder.ROOT / 'variants.json').read_text())
    if args.list_profiles:
        print(json.dumps({'profile': [p['id'] for p in data['profiles']]}))
        return 0
    report = build_matrix(data, args.work_dir, args.version, args.profile, args.jobs)
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
