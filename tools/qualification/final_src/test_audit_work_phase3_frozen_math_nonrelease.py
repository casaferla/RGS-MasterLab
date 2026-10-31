"""Negative guards for Phase3 frozen-domain/model independent Q0 review.

Synthetic data only. Real pinned Work ZIP is tested in the PRIVATE
operator environment, never uploaded as GitHub public CI input.
"""
import unittest
from fractions import Fraction as F
from audit_work_phase3_handoff_nonrelease import AuditError
from audit_work_phase3_frozen_math_nonrelease import (
    canonical_ratio_pairs, verify_frozen_band_domains,
    verify_exact_covered_error_model,
)


def authentic_geometry():
    rows = []
    for (L, M), (source, master) in canonical_ratio_pairs().items():
        smallest = min(source, master)
        for kind, lo, hi in (
            ("passband", 0, smallest*12//25),
            ("stopband", smallest//2, source*L//2),
        ):
            rows.append(dict(L=L, M=M, kind=kind,
                             lo_hz=str(lo), hi_hz=str(hi),
                             status="VERIFIED_NONRELEASE"))
    return rows


def sufficient_model():
    u, eta = F(1,2**53), F(1,2**38)
    gamma2 = 2*u/(1-2*u)
    c = 4*eta+64*u
    g81=81*u/(1-81*u)
    q=22*c/(1-22*c)
    return {
        "status":"MATHEMATICAL_ERROR_MODEL_REDERIVED_PASS_UNDER_CHECKED_IEEE_ASSUMPTIONS",
        "runtime_guard_gap_found":True,
        "release_qualified":False,
        "twiddle_upper":str((1+16*u)**256-1),
        "twiddle_limit":str(eta),
        "butterfly_local_upper":str(
            eta+2*gamma2*(1+eta)+u*(1+eta+2*gamma2*(1+eta))),
        "used_c":str(c),
        "accumulated_fft_and_input_upper":str(q*(1+g81)+g81),
        "used_envelope":str(F(1,2**29)),
        "underflow_real_operation_count_bound":2**34,
        "underflow_propagation_bound":2**35,
        "underflow_bound":str(F(2**69,2**1075)),
        "used_underflow":str(F(1,2**900)),
    }


class FrozenMathReviewTests(unittest.TestCase):
    def test_exact_all_28_domains(self):
        self.assertEqual(verify_frozen_band_domains(authentic_geometry()), 28)
        self.assertEqual(len(canonical_ratio_pairs()),14)

    def test_wrong_high_end_at_one_ratio(self):
        bands=authentic_geometry()
        bands[0]["hi_hz"]=str(int(bands[0]["hi_hz"])-1)
        with self.assertRaisesRegex(AuditError,"endpoint"):
            verify_frozen_band_domains(bands)

    def test_wrong_band_expansion(self):
        bands=authentic_geometry()
        bands[1]["hi_hz"]=str(int(bands[1]["hi_hz"])+1)
        with self.assertRaisesRegex(AuditError,"endpoint"):
            verify_frozen_band_domains(bands)

    def test_missing_and_duplicated_ratio(self):
        with self.assertRaises(AuditError):
            verify_frozen_band_domains(authentic_geometry()[:-1])
        bands=authentic_geometry()
        bands[-1]=bands[0].copy()
        with self.assertRaises(AuditError):
            verify_frozen_band_domains(bands)

    def test_bool_ratio_and_noncanonical_number(self):
        bands=authentic_geometry()
        bands[0]["L"]=True
        with self.assertRaisesRegex(AuditError,"invalid ratio"):
            verify_frozen_band_domains(bands)
        bands=authentic_geometry()
        bands[0]["lo_hz"]="0/1"
        with self.assertRaisesRegex(AuditError,"integer"):
            verify_frozen_band_domains(bands)

    def test_nonrelease_declared_error_model(self):
        self.assertTrue(verify_exact_covered_error_model(sufficient_model()))

    def test_forged_looser_fft_bound(self):
        model=sufficient_model()
        model["used_envelope"]="1/1024"
        with self.assertRaisesRegex(AuditError,"used_envelope"):
            verify_exact_covered_error_model(model)

    def test_tampered_underflow_or_guard(self):
        model=sufficient_model()
        model["underflow_bound"]="1/2"
        with self.assertRaisesRegex(AuditError,"underflow_bound"):
            verify_exact_covered_error_model(model)
        model=sufficient_model()
        model["runtime_guard_gap_found"]=False
        with self.assertRaisesRegex(AuditError,"status mismatch"):
            verify_exact_covered_error_model(model)

    def test_reject_missing_rational_or_false_release(self):
        model=sufficient_model()
        model["twiddle_upper"]="NaN"
        with self.assertRaises(AuditError):
            verify_exact_covered_error_model(model)
        model=sufficient_model()
        model["release_qualified"]=True
        with self.assertRaises(AuditError):
            verify_exact_covered_error_model(model)


if __name__=="__main__":
    unittest.main()
