import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

PORTING = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PORTING))
spec = importlib.util.spec_from_file_location("apply_source_patches", PORTING / "apply-webkit-patches.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class SourcePatchTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        self.bundle = self.root / "bundle"
        self.source = self.root / "source"
        self.bundle.mkdir(); self.source.mkdir()
        self.target = self.source / "sample.c"
        self.target.write_bytes(b"old\n")
        self.pin = {"commit": "pinned"}
        (self.bundle / "webkit-source.json").write_text(json.dumps(self.pin))
        (self.source / ".iewebkit-hydrated.json").write_text(json.dumps(self.pin))
        delta = b"--- a/sample.c\n+++ b/sample.c\n@@ -1 +1 @@\n-old\n+new\n"
        (self.bundle / "sample.patch").write_bytes(delta)
        digest = lambda value: hashlib.sha256(value).hexdigest()
        self.manifest = {"source_commit": "pinned", "patches": [{"path": "sample.c", "patch": "sample.patch", "before_sha256": digest(b"old\n"), "after_sha256": digest(b"new\n"), "patch_sha256": digest(delta)}]}
        (self.bundle / "webkit-patches.json").write_text(json.dumps(self.manifest))

    def apply(self):
        with patch.object(module, "HERE", self.bundle), patch.object(sys, "argv", ["apply", "--source", str(self.source)]):
            module.main()

    def test_exact_patch_and_idempotent_rerun(self):
        self.apply(); self.assertEqual(self.target.read_bytes(), b"new\n")
        self.apply(); self.assertEqual(self.target.read_bytes(), b"new\n")

    def test_local_edits_are_preserved(self):
        self.target.write_bytes(b"user change\n")
        with self.assertRaises(SystemExit): self.apply()
        self.assertEqual(self.target.read_bytes(), b"user change\n")

    def test_corrupt_patch_cannot_modify_source(self):
        (self.bundle / "sample.patch").write_text("corrupt")
        with self.assertRaises(SystemExit): self.apply()
        self.assertEqual(self.target.read_bytes(), b"old\n")

    def test_symlink_target_is_rejected(self):
        outside = self.root / "outside.c"; outside.write_bytes(b"old\n")
        self.target.unlink(); self.target.symlink_to(outside)
        with self.assertRaises(SystemExit): self.apply()
        self.assertEqual(outside.read_bytes(), b"old\n")

if __name__ == "__main__":
    unittest.main()
