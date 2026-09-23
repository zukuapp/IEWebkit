import copy
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("variants", ROOT / "tools/variants.py")
v = importlib.util.module_from_spec(spec)
spec.loader.exec_module(v)


class Variants(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((ROOT / "variants.json").read_text())

    def test_each_version_has_unique_artifacts(self):
        ids = set()
        for p in self.data["profiles"]:
            for arch in p["architectures"]:
                for mode in p["modes"]:
                    result = v.select(self.data, p["ie"], p["os"], arch, mode)
                    self.assertNotIn(result["artifact_id"], ids)
                    ids.add(result["artifact_id"])
                    self.assertFalse(result["verified"])

    def test_no_fallback(self):
        for args in [("11", "winme", "x86", "classic"),
                     ("5.5", "winme", "x64", "classic"),
                     ("6", "winme", "x86", "protected"),
                     ("12", "win81", "x64", "classic")]:
            with self.subTest(args=args), self.assertRaises(ValueError):
                v.select(self.data, *args)

    def test_unverified_release_rejected(self):
        with self.assertRaisesRegex(ValueError, "no guest certification"):
            v.select(self.data, "5.5", "winme", "x86", "classic", release=True)

    def test_duplicate_rejected(self):
        self.data["profiles"].append(copy.deepcopy(self.data["profiles"][0]))
        with self.assertRaises(ValueError):
            v.validate(self.data)

    def test_evidence_bound_to_exact_target_and_digest(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "evidence").mkdir()
            evidence = {"artifact_id": "ie55-winme-x86-classic", "artifact_sha256": "a" * 64,
                        "passed": True, "checks": dict.fromkeys(v.CHECKS, True)}
            path = root / "evidence/run.json"
            cert = {"arch": "x86", "mode": "classic", "evidence": "evidence/run.json", "sha256": ""}
            self.data["profiles"][0]["certifications"].append(cert)
            def write():
                path.write_text(json.dumps(evidence))
                cert["sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
            write()
            self.assertTrue(v.select(self.data, "5.5", "winme", "x86", "classic", True, root)["verified"])
            evidence["checks"]["tls_validation"] = False
            write()
            with self.assertRaisesRegex(ValueError, "incomplete"):
                v.select(self.data, "5.5", "winme", "x86", "classic", True, root)
            evidence["checks"]["tls_validation"] = True
            evidence["artifact_id"] = "ie6-winme-x86-classic"
            write()
            with self.assertRaisesRegex(ValueError, "mismatched"):
                v.select(self.data, "5.5", "winme", "x86", "classic", True, root)
            path.write_text('{}')
            with self.assertRaisesRegex(ValueError, "digest"):
                v.select(self.data, "5.5", "winme", "x86", "classic", True, root)


if __name__ == "__main__":
    unittest.main()
