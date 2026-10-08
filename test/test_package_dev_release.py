import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build_host_matrix as matrix
import build_host_variant as builder
import package_dev_release as packager


class DevelopmentPackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix='iewebkit-package-test-')
        self.addCleanup(self.temporary.cleanup)
        self.work = Path(self.temporary.name)
        self.root = self.work / 'repo'
        self.root.mkdir()
        self.data = json.loads((ROOT / 'variants.json').read_bytes())
        for name in (*builder.SOURCES, 'variants.json', *packager.NOTICE_FILES):
            destination = self.root / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes((ROOT / name).read_bytes())
        for args in [('init', '-q'), ('add', '.'), ('-c', 'user.name=Package Test',
                     '-c', 'user.email=package@example.invalid', 'commit', '-q', '-m', 'source fixture')]:
            subprocess.run(['git', '-C', str(self.root), *args], check=True, capture_output=True)
        self.commit, self.sources = packager.source_snapshot(self.root, 'HEAD')
        self.matrix_dir = self.work / 'matrix'
        self.matrix_dir.mkdir()
        self.path = self.matrix_dir / 'matrix-all.json'
        self.source_hashes = {name: packager.digest(self.sources[name]) for name in builder.SOURCES}
        plans = matrix.configurations(self.data, '0.1.0.0')
        results = []
        self.metadata = {}
        for plan in plans:
            directory = self.matrix_dir / plan['build_id']
            directory.mkdir()
            binary = ('fixture for ' + plan['build_id']).encode()
            verification = self.verify(binary, plan)
            files = {'iewebkit-host.dll': binary, 'pe-verification.json': packager.json_bytes(verification)}
            metadata = dict(schema_version=1, product='IEWebkit', artifact_kind='development-host',
                            engine_included=False, guest_verified=False, release_eligible=False,
                            configuration=plan, source_sha256=self.source_hashes,
                            artifact_sha256={name: packager.digest(content) for name, content in files.items()})
            files['build.json'] = packager.json_bytes(metadata)
            self.metadata[directory] = metadata
            for name, content in files.items():
                (directory / name).write_bytes(content)
            # These files may exist in a build or lab directory but must never be copied.
            (directory / 'private.img').write_bytes(b'private Windows guest image')
            (directory / 'build.log').write_bytes(b'private diagnostic path')
            target = plan['target']
            results.append(dict(artifact_id=target['artifact_id'],
                                profile='ie' + target['ie'].replace('.', '') + '-' + target['os'],
                                passed=True, engine_included=False, guest_verified=False,
                                output=str(directory), sha256=packager.digest(binary),
                                verification_sha256=packager.digest(files['pe-verification.json'])))
        self.report = dict(schema_version=1, artifact_kind='development-host', passed=True,
                           engine_included=False, guest_verified=False, release_eligible=False,
                           profile_count=len(self.data['profiles']), variant_count=len(plans),
                           manifest_sha256=packager.digest(json.dumps(self.data, sort_keys=True).encode()),
                           source_sha256=self.source_hashes, results=results)
        self.write_report()
        self.notice = self.work / 'gcc-runtime' / 'COPYING.RUNTIME'
        self.notice.parent.mkdir()
        self.notice.write_text('Runtime license fixture\n')
        self.verifier = patch.object(packager.verify_host_pe, 'verify', side_effect=self.verify)
        self.verifier.start()
        self.addCleanup(self.verifier.stop)

    @staticmethod
    def verify(binary, configuration):
        return dict(schema_version=1, passed=True, engine_included=False,
                    guest_verified=False, release_eligible=False,
                    artifact_sha256=packager.digest(binary))

    def write_report(self):
        self.path.write_bytes(packager.json_bytes(self.report))

    def package(self, output='release', **kwargs):
        return packager.package(self.path, self.work / output, 'v0.1.0-dev.20261008',
                                runtime_notices=[self.notice], root=self.root, **kwargs)

    def test_complete_matrix_and_revision_bound_payload_exclude_private_build_files(self):
        result = self.package()
        output = Path(result['output'])
        metadata_raw = (output / 'release-metadata.json').read_bytes()
        metadata = json.loads(metadata_raw)
        self.assertEqual(metadata['source_revision'], self.commit)
        self.assertEqual(metadata['variant_count'], 55)
        self.assertEqual(metadata['profile_count'], 17)
        for flag in ('engine_included', 'guest_verified', 'release_eligible'):
            self.assertIs(metadata[flag], False)
        archive = next(output.glob('*.zip'))
        with zipfile.ZipFile(archive) as packaged:
            names = packaged.namelist()
            self.assertEqual(sum(name.endswith('/iewebkit-host.dll') for name in names), 55)
            self.assertFalse(any(name.endswith(('.img', '.log', '.o')) for name in names))
            self.assertEqual(packaged.read('release-metadata.json'), metadata_raw)
            self.assertIn('runtime-notices/gcc-runtime/COPYING.RUNTIME', names)
            for name, checksum in metadata['file_sha256'].items():
                self.assertEqual(packager.digest(packaged.read(name)), checksum)
            self.assertNotIn(str(self.matrix_dir), packaged.read('host-matrix.json').decode())
        for line in (output / 'SHA256SUMS').read_text().splitlines():
            checksum, name = line.split('  ')
            self.assertEqual(packager.digest((output / name).read_bytes()), checksum)

    def test_packaging_same_snapshot_is_deterministic_and_never_overwrites(self):
        first = self.package('one')
        second = self.package('two')
        self.assertEqual(first['asset_sha256'], second['asset_sha256'])
        with self.assertRaisesRegex(ValueError, 'already exists'):
            self.package('one')

    def test_source_and_manifest_mismatches_fail_before_creating_output(self):
        for key in ('source_sha256', 'manifest_sha256'):
            original = copy.deepcopy(self.report[key])
            self.report[key] = {} if key == 'source_sha256' else '0' * 64
            self.write_report()
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, 'Git revision'):
                self.package()
            self.assertFalse((self.work / 'release').exists())
            self.report[key] = original

    def test_incomplete_duplicate_failed_and_certified_matrices_are_rejected(self):
        original = copy.deepcopy(self.report)
        for mutation in ('missing', 'duplicate', 'failed', 'certified'):
            self.report = copy.deepcopy(original)
            if mutation == 'missing':
                self.report['results'].pop()
            elif mutation == 'duplicate':
                self.report['results'][1] = self.report['results'][0]
            elif mutation == 'failed':
                self.report['passed'] = False
            else:
                self.report['guest_verified'] = True
            self.write_report()
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                self.package()
            self.assertFalse((self.work / 'release').exists())

    def test_tampered_host_and_verification_metadata_are_rejected(self):
        directory = Path(self.report['results'][0]['output'])
        for name in ('iewebkit-host.dll', 'pe-verification.json'):
            path = directory / name
            original = path.read_bytes()
            path.write_bytes(original + b'\ncorrupted')
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'digest mismatch'):
                self.package()
            path.write_bytes(original)

    def test_forged_pe_report_is_reverified_against_binary(self):
        directory = Path(self.report['results'][0]['output'])
        forged = self.verify((directory / 'iewebkit-host.dll').read_bytes(), {})
        forged['artifact_sha256'] = '0' * 64
        content = packager.json_bytes(forged)
        (directory / 'pe-verification.json').write_bytes(content)
        self.metadata[directory]['artifact_sha256']['pe-verification.json'] = packager.digest(content)
        (directory / 'build.json').write_bytes(packager.json_bytes(self.metadata[directory]))
        self.report['results'][0]['verification_sha256'] = packager.digest(content)
        self.write_report()
        with self.assertRaisesRegex(ValueError, 'DLL bytes'):
            self.package()

    def test_build_output_escape_and_symlinked_payload_are_refused(self):
        original = self.report['results'][0]['output']
        self.report['results'][0]['output'] = str(self.work / 'unrelated')
        self.write_report()
        with self.assertRaisesRegex(ValueError, 'escapes'):
            self.package()
        self.report['results'][0]['output'] = original
        self.write_report()
        dll = Path(original) / 'iewebkit-host.dll'
        other = self.work / 'outside.dll'
        dll.rename(other)
        dll.symlink_to(other)
        with self.assertRaisesRegex(ValueError, 'symlinked'):
            self.package()

    def test_output_and_runtime_notices_are_explicitly_scoped(self):
        with self.assertRaisesRegex(ValueError, 'outside the repository'):
            packager.package(self.path, self.root / 'release', 'v0.1.0-dev.20261008', root=self.root)
        with self.assertRaisesRegex(ValueError, 'prerelease'):
            packager.package(self.path, self.work / 'release', 'v1.0.0', root=self.root)
        with self.assertRaisesRegex(ValueError, 'required'):
            packager.runtime_payload([])
        copying = self.notice.parent / 'COPYING3.LIB'
        copying.write_text('GPL library license fixture\n')
        self.assertIn('runtime-notices/gcc-runtime/COPYING3.LIB', packager.runtime_payload([copying]))
        private = self.work / 'private.img'
        private.write_bytes(b'private')
        with self.assertRaisesRegex(ValueError, 'runtime notice'):
            packager.runtime_payload([private])


if __name__ == '__main__':
    unittest.main()
