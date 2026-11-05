"""Synthetic adversarial audit tests. No private Work coefficients in CI."""
import io
import json
import unittest
from zipfile import ZipFile
from audit_work_phase3_handoff_nonrelease import AuditError, sha, parsed, no_release, audit

class WorkPhase3IntakeTests(unittest.TestCase):
    def test_reject_unpinned_fake_zip(self):
        raw=io.BytesIO()
        with ZipFile(raw,"w") as z:
            z.writestr("phase3/PHASE3_PACKAGE_MANIFEST.json","{}")
        with self.assertRaisesRegex(AuditError,"external Phase3 ZIP pin"):
            audit(raw.getvalue())
    def test_duplicate_json_keys(self):
        with self.assertRaisesRegex(AuditError,"duplicate JSON key"):
            parsed(b'{"release_qualified":false,"release_qualified":true}')
    def test_nonfinite_json(self):
        with self.assertRaises(AuditError):
            parsed(b'{"value":NaN}')
    def test_release_flags_must_be_literal_false(self):
        for x in (True,0,None,"false"):
            with self.assertRaises(AuditError):
                no_release({"product_admission_authorized":x})
    def test_missing_member_even_if_outer_pin_replaced(self):
        raw=io.BytesIO()
        with ZipFile(raw,"w") as z:
            z.writestr("phase3/PHASE3_PACKAGE_MANIFEST.json","{}")
        with self.assertRaises(AuditError):
            audit(raw.getvalue(),sha(raw.getvalue()))
    def test_duplicate_zip_member_even_if_outer_pin_replaced(self):
        import warnings
        raw=io.BytesIO()
        with warnings.catch_warnings():
            warnings.simplefilter("ignore",UserWarning)
            with ZipFile(raw,"w") as z:
                z.writestr("phase3/x","a")
                z.writestr("phase3/x","b")
        with self.assertRaisesRegex(AuditError,"duplicate member"):
            audit(raw.getvalue(),sha(raw.getvalue()))

if __name__=="__main__":
    unittest.main()
