import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

PORTING = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PORTING))
import paths
import bootstrap

spec = importlib.util.spec_from_file_location("bootstrap_icu", PORTING / "bootstrap-icu.py")
bootstrap_icu = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bootstrap_icu)

class ExternalPathTests(unittest.TestCase):
    def test_rejects_repository_and_nested_paths(self):
        for destination in (paths.REPOSITORY, paths.REPOSITORY / "never-create" / "source"):
            with self.subTest(destination=destination), self.assertRaises(SystemExit):
                paths.external_work_dir(destination)

    def test_symlink_cannot_reenter_repository(self):
        with tempfile.TemporaryDirectory() as folder:
            alias = Path(folder) / "source-alias"
            alias.symlink_to(paths.REPOSITORY, target_is_directory=True)
            with self.assertRaises(SystemExit):
                paths.external_work_dir(alias / "never-create")

    def test_external_path_is_resolved(self):
        with tempfile.TemporaryDirectory() as folder:
            self.assertEqual(paths.external_work_dir(Path(folder) / "output"), Path(folder).resolve() / "output")

    def test_bootstraps_reject_before_network_or_directory_creation(self):
        destination = paths.REPOSITORY / "never-create-download-test"
        self.assertFalse(destination.exists())
        for module in (bootstrap, bootstrap_icu):
            with self.subTest(module=module.__name__), patch.object(sys, "argv", ["bootstrap", "--work-dir", str(destination)]), patch("urllib.request.urlopen") as network:
                with self.assertRaises(SystemExit):
                    module.main()
                network.assert_not_called()
                self.assertFalse(destination.exists())

if __name__ == "__main__":
    unittest.main()
