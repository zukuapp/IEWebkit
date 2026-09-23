import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build_host_variant as builder


class HostVariantBuildTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((ROOT / 'variants.json').read_text())

    def plan(self, profile='ie55-winme', arch='x86', mode='classic', version='0.1.0.0', **kwargs):
        return builder.plan(self.data, profile, arch, mode, version, **kwargs)

    def test_every_profile_has_unique_os_arch_mode_builds(self):
        identities = set()
        for profile in self.data['profiles']:
            for arch in profile['architectures']:
                for mode in profile['modes']:
                    configuration = self.plan(profile['id'], arch, mode)
                    self.assertNotIn(configuration['build_id'], identities)
                    identities.add(configuration['build_id'])
                    self.assertEqual(configuration['target']['ie'], profile['ie'])
                    self.assertEqual(configuration['target']['os'], profile['os'])
                    self.assertEqual(configuration['target']['mode'], mode)
                    self.assertFalse(configuration['target']['verified'])
        self.assertEqual(len(self.data['profiles']), 17)

    def test_me_xp64_and_ie11_compile_targets(self):
        me = self.plan()
        self.assertEqual(me['definitions']['_WIN32_WINDOWS'], '0x0490')
        self.assertEqual(me['definitions']['_WIN32_WINNT'], '0x0400')
        self.assertEqual(me['definitions']['_WIN32_IE'], '0x0550')
        xp64 = self.plan('ie6-winxp-x64-sp2', 'x64')
        self.assertEqual(xp64['definitions']['NTDDI_VERSION'], '0x05020200')
        self.assertEqual(xp64['tools']['cxx'], 'x86_64-w64-mingw32-g++')
        win7 = self.plan('ie11-win7-sp1', 'x86', 'protected')
        self.assertEqual(win7['definitions']['_WIN32_WINNT'], '0x0601')
        self.assertEqual(win7['definitions']['_WIN32_IE'], '0x0A00')
        self.assertNotIn('_WIN32_WINDOWS', win7['definitions'])
        self.assertEqual(win7['subsystem'], [6, 1])

    def test_unknown_profile_arch_mode_and_injection_fail(self):
        for profile, arch, mode in [('ie6-win7-sp1', 'x86', 'classic'),
                                    ('ie55-winme', 'x64', 'classic'),
                                    ('ie55-winme', 'x86', 'protected'),
                                    ('ie55-winme; touch injected', 'x86', 'classic'),
                                    ('../../escape', 'x86', 'classic')]:
            with self.subTest(profile=profile, arch=arch, mode=mode), self.assertRaises(ValueError):
                self.plan(profile, arch, mode)
        data = copy.deepcopy(self.data)
        data['profiles'][0]['id'] = 'ie55-unknown-os'
        data['profiles'][0]['os'] = 'unknown-os'
        with self.assertRaisesRegex(ValueError, 'reviewed compiler target'):
            builder.plan(data, 'ie55-unknown-os', 'x86', 'classic', '0.1.0.0')

    def test_numeric_version_rejects_rc_and_path_injection(self):
        for version in ['1.2.3', '1.2.3.65536', '01.2.3.4', '../1.2.3.4',
                        '1.2.3.4"\nBEGIN', '1.2.3.4; touch injected']:
            with self.subTest(version=version), self.assertRaises(ValueError):
                self.plan(version=version)
        self.assertEqual(builder.parse_version('1.2.3.65535'), (1, 2, 3, 65535))
        self.assertNotEqual(self.plan()['build_id'], self.plan(version='0.1.0.1')['build_id'])

    def test_resource_binds_variant_and_marks_development(self):
        classic = builder.resource_text(self.plan())
        protected = builder.resource_text(self.plan('ie11-win7-sp1', 'x64', 'protected'))
        self.assertIn('FILEVERSION 0,1,0,0', classic)
        self.assertIn('ie55-winme-x86-classic', classic)
        self.assertIn('ie11-win7-sp1-x64-protected', protected)
        self.assertIn('Unverified development component', protected)
        self.assertIn('FILEFLAGS 0x2L', protected)
        self.assertNotEqual(classic, protected)

    def test_no_unverified_release_output(self):
        with self.assertRaisesRegex(ValueError, 'no guest certification'):
            self.plan(release=True)

    def test_external_directory_resolves_symlink_and_refuses_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            root = work / 'repo'
            root.mkdir()
            alias = work / 'alias'
            alias.symlink_to(root, target_is_directory=True)
            identity = self.plan()['build_id']
            for path in [root, root / 'build', alias / 'build']:
                with self.subTest(path=path), self.assertRaisesRegex(ValueError, 'outside'):
                    builder.external_destination(path, identity, root)
            destination = builder.external_destination(work / 'external ; spaces', identity, root)
            destination.mkdir(parents=True)
            (destination / 'preserved').write_text('prior artifact')
            with self.assertRaisesRegex(ValueError, 'already exists'):
                builder.external_destination(destination.parent, identity, root)
            self.assertEqual((destination / 'preserved').read_text(), 'prior artifact')

    def test_parallel_same_variant_is_refused_and_lock_is_released(self):
        with tempfile.TemporaryDirectory() as directory:
            destination = Path(directory) / self.plan()['build_id']
            with builder.output_lock(destination):
                with self.assertRaisesRegex(ValueError, 'already being built'):
                    with builder.output_lock(destination):
                        self.fail('duplicate build cannot obtain output ownership')
            with builder.output_lock(destination):
                pass
            self.assertFalse(list(Path(directory).iterdir()))

    def test_missing_x64_compiler_never_uses_x86_or_creates_output(self):
        configuration = self.plan('ie11-win7-sp1', 'x64', 'protected')
        with tempfile.TemporaryDirectory() as directory, patch.object(builder.shutil, 'which', return_value=None):
            work = Path(directory) / 'not-created'
            with self.assertRaisesRegex(ValueError, r'x86_64-w64-mingw32-g\+\+'):
                builder.build(configuration, work)
            self.assertFalse(work.exists())


if __name__ == '__main__':
    unittest.main()
