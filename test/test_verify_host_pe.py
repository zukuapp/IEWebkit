import copy
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build_host_variant as builder
import build_host_matrix as matrix
import verify_host_pe as verifier


class MatrixSelectionTests(unittest.TestCase):
    def test_all_profiles_expand_to_every_unique_architecture_and_mode(self):
        data = json.loads((ROOT / 'variants.json').read_text())
        plans = matrix.configurations(data, '0.1.0.0')
        ids = {p['build_id'] for p in plans}
        self.assertEqual(len(data['profiles']), 17)
        self.assertEqual(len(plans), 55)
        self.assertEqual(len(ids), 55)
        self.assertEqual(len(matrix.configurations(data, '0.1.0.0', 'ie11-win81')), 6)
        with self.assertRaisesRegex(ValueError, 'no matching'):
            matrix.configurations(data, '0.1.0.0', 'missing')


@unittest.skipUnless(all(shutil.which(prefix + suffix)
                           for prefix in ('i686-w64-mingw32-', 'x86_64-w64-mingw32-')
                           for suffix in ('gcc', 'windres')), 'both MinGW architectures required')
class LinkedPEVerificationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='iewebkit-pe-test-')
        cls.addClassCleanup(cls.directory.cleanup)
        work = Path(cls.directory.name)
        data = json.loads((ROOT / 'variants.json').read_text())
        cls.artifacts = []
        for profile, arch, mode in [('ie55-winme', 'x86', 'classic'),
                                     ('ie11-win81', 'x64', 'enhanced-protected')]:
            config = builder.plan(data, profile, arch, mode, '1.2.3.4')
            target = work / arch
            target.mkdir()
            (target / 'version.rc').write_text(builder.resource_text(config))
            (target / 'minimal.c').write_text('int metadata_fixture(void) { return 0; }\n')
            subprocess.run([config['tools']['rc'], '-i', str(target / 'version.rc'), '-o',
                            str(target / 'version.o'), '-O', 'coff'], check=True, capture_output=True)
            major, minor = config['subsystem']
            subprocess.run([config['tools']['cc'], '-shared', '-Wl,--no-insert-timestamp',
                            f'-Wl,--major-os-version,{major},--minor-os-version,{minor}',
                            f'-Wl,--major-subsystem-version,{major},--minor-subsystem-version,{minor}',
                            str(target / 'minimal.c'), str(target / 'version.o'),
                            '-o', str(target / 'fixture.dll')], check=True, capture_output=True)
            cls.artifacts.append(((target / 'fixture.dll').read_bytes(), config))

    def test_real_x86_x64_linked_metadata(self):
        for binary, config in self.artifacts:
            with self.subTest(arch=config['target']['arch']):
                result = verifier.verify(binary, config)
                self.assertTrue(result['passed'])
                self.assertFalse(result['engine_included'])
                self.assertFalse(result['guest_verified'])
                self.assertFalse(result['release_eligible'])

    def test_corrupt_machine_timestamp_subsystem_and_dll_flag_are_rejected(self):
        binary, config = self.artifacts[0]
        pe = struct.unpack_from('<I', binary, 0x3c)[0]
        for offset, value in [(pe + 4, b'\x64\x86'), (pe + 8, b'\x01\0\0\0'),
                              (pe + 22, b'\x02\0'), (pe + 24 + 48, b'\x06\0')]:
            broken = bytearray(binary)
            broken[offset:offset + len(value)] = value
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                verifier.verify(bytes(broken), config)

    def test_linked_strings_and_fixed_version_must_match_target(self):
        binary, config = self.artifacts[0]
        for key, value in [('ie', '6'), ('mode', 'protected'), ('os', 'winxp-sp3')]:
            wrong = copy.deepcopy(config)
            wrong['target'][key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                verifier.verify(binary, wrong)
        wrong = copy.deepcopy(config)
        wrong['version'] = '1.2.3.5'
        with self.assertRaisesRegex(ValueError, 'FIXEDFILEINFO'):
            verifier.verify(binary, wrong)
        needle = config['target']['artifact_id'].encode('utf-16le')
        changed = binary.replace(needle, b'X\0' + needle[2:])
        with self.assertRaisesRegex(ValueError, 'strings'):
            verifier.verify(changed, config)

    def test_truncation_missing_resources_and_malformed_version_fail(self):
        binary, config = self.artifacts[0]
        pe = struct.unpack_from('<I', binary, 0x3c)[0]
        missing = bytearray(binary)
        struct.pack_into('<II', missing, pe + 24 + 96 + 16, 0, 0)
        for broken in (b'', binary[:64], binary[:512], bytes(missing)):
            with self.subTest(size=len(broken)), self.assertRaises(ValueError):
                verifier.verify(broken, config)
        with self.assertRaises(ValueError):
            verifier.version_block(b'\x08\0\0\0\0\0A\0')


if __name__ == '__main__':
    unittest.main()
