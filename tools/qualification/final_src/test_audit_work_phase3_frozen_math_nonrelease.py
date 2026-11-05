"""Negative guards for Phase3 frozen-domain/model independent Q0 review.

Synthetic data only. Real pinned Work ZIP is tested in the PRIVATE
operator environment, never uploaded as GitHub public CI input.
"""
import unittest
from fractions import Fraction as F
from phase11_math_preflight import Ratio, kernel_shape
from audit_work_phase3_handoff_nonrelease import AuditError
from audit_work_phase3_frozen_math_nonrelease import (
    canonical_ratio_pairs, verify_frozen_band_domains,
    verify_exact_covered_error_model, verify_independent_rounding_receipt_shapes,
    verify_phase3_q1_q2_subset_receipts,
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



def fake_rounding():
    rows, receipts = [], {}
    for (L, M), (source, target) in canonical_ratio_pairs().items():
        size = kernel_shape(Ratio.make(source, target)).N
        for bits in (256, 512):
            sha = "a"*64
            rows.append(dict(L=L, M=M, bits=bits,
                             coefficient_count=size, asset_sha256=sha))
            receipts[(L, M, bits)] = dict(
                L=L, M=M, bits=bits,
                shape_formula_exact_pi_interval_verified=True,
                header_independent_regeneration=True,
                original_header_reused_and_independently_parsed=False,
                all_binary64_rounding_cells_proven=True,
                payload_bitwise_equal=True, release_qualified=False,
                asset_sha256=sha, expected_sha256=sha,
                source_sha256="b"*64, arithmetic_helper_sha256="c"*64,
                transcript=f"DIRECTED_COEFFICIENT_ROUNDING_CELLS_PROVEN "
                           f"{size} BITS {bits} MPFR 4.2.1\n")
    return rows, receipts


def fake_q1_subset():
    kernels = {(L,M): "a"*64 for L,M in canonical_ratio_pairs()}
    rows=[]
    for source in (44100,48000,88200,96000,176400,192000):
        for target in (44100,48000,88200,96000,176400,192000):
            from math import gcd
            ratio=(target//gcd(source,target),source//gcd(source,target))
            for recipe, n in (("short_17",17),("one_frame",1)):
                frames=(n*target+source-1)//source
                rows.append(dict(fin=source, fout=target, case=recipe,
                    frames=frames, expected_sha256="b"*64,
                    kernel_sha256=(None if source==target else kernels[ratio]),
                    mpfr_zero_stuffed_rounding_cells_proven=True,
                    exact_dyadic_independent_bitwise_match=True,
                    strict_scalar_absolute_partition_bitwise_equal=True,
                    strict_scalar_crop_bitwise_equal=True,
                    strict_scalar_max_abs_error_vs_exact="0",
                    canonical_qtp_profile_applied=False,
                    actual_product_qt_executed=False, release_qualified=False,
                    transcript=f"FULL_ZERO_STUFF_CONVOLUTION_ROUNDING_PROVEN "
                               f"{frames} MPFR_BITS 256\n"))
    return dict(cases=72, source_master_pairs=36,
                mpfr_vs_dyadic_exact_cases=72,
                scalar_partition_and_crop_cases=72,
                identity_special_bit_cases=6,
                full_Q1_gate="OPEN",full_Q2_gate="OPEN",
                release_qualified=False,product_admission_authorized=False,
                results=rows),kernels

class FrozenMathReviewTests(unittest.TestCase):
    def test_all_72_q1_q2_receipts_are_exact_subset_only(self):
        doc,kernels=fake_q1_subset()
        self.assertEqual(verify_phase3_q1_q2_subset_receipts(
            doc,kernels),72)

    def test_reject_q1_q2_bogus_frames_and_kernel_sha(self):
        doc,kernels=fake_q1_subset()
        doc["results"][0]["frames"]+=1
        with self.assertRaisesRegex(AuditError,"frame count"):
            verify_phase3_q1_q2_subset_receipts(doc,kernels)
        doc,kernels=fake_q1_subset()
        doc["results"][2]["kernel_sha256"]="f"*64
        with self.assertRaisesRegex(AuditError,"unpinned Q0"):
            verify_phase3_q1_q2_subset_receipts(doc,kernels)

    def test_reject_missing_q1_q2_case_and_false_qt_pass(self):
        doc,kernels=fake_q1_subset()
        doc["results"].pop()
        with self.assertRaisesRegex(AuditError,"72 source"):
            verify_phase3_q1_q2_subset_receipts(doc,kernels)
        doc,kernels=fake_q1_subset()
        doc["results"][3]["actual_product_qt_executed"]=True
        with self.assertRaisesRegex(AuditError,"unexecuted product Qt"):
            verify_phase3_q1_q2_subset_receipts(doc,kernels)

    def test_28_binary64_shape_and_rounding_receipt_claims(self):
        rows, receipts=fake_rounding()
        self.assertEqual(verify_independent_rounding_receipt_shapes(
            rows, receipts),28)

    def test_reject_bogus_shape_and_shared_producer(self):
        rows, receipts=fake_rounding()
        rows[0]["coefficient_count"]-=1
        with self.assertRaisesRegex(AuditError,"coefficient count"):
            verify_independent_rounding_receipt_shapes(rows,receipts)
        rows, receipts=fake_rounding()
        receipts[(1,2,256)]["source_sha256"]="d"*64
        with self.assertRaisesRegex(AuditError,"generator provenance"):
            verify_independent_rounding_receipt_shapes(rows,receipts)

    def test_reject_false_rounding_or_cross_precision_payload(self):
        rows, receipts=fake_rounding()
        receipts[(1,2,256)]["all_binary64_rounding_cells_proven"]=False
        with self.assertRaisesRegex(AuditError,"RN assertions"):
            verify_independent_rounding_receipt_shapes(rows,receipts)
        rows, receipts=fake_rounding()
        rows[0]["asset_sha256"]="e"*64
        with self.assertRaisesRegex(AuditError,"SHA discrepancy"):
            verify_independent_rounding_receipt_shapes(rows,receipts)
        # Prior operator precedence could accept a mismatched 512-bit
        # receipt whenever a 256-bit asset SHA matched the stored ratio.
        rows, receipts=fake_rounding()
        receipts[(1,2,512)]["expected_sha256"]="f"*64
        with self.assertRaisesRegex(AuditError,"SHA discrepancy"):
            verify_independent_rounding_receipt_shapes(rows,receipts)

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
