#!/usr/bin/env python3
"""Fast resumable REAL-generator pipeline lifecycle tests (synthetic test backend).

NONE of these test fixtures contains a real MPFR Kaiser-Sinc asset. They test
atomic checkpoints, error recovery, actual 14-ratio publication from verified
double-run receipts and fail-closed semantics without claiming release.
"""
from __future__ import annotations

from pathlib import Path
from tempfile import TemporaryDirectory
import json
import unittest

from build_candidate_manifest import canonical_pair_for_ratio
from build_resumable_real_candidates import (
    RECEIPT, stage_one, stage_all, finalize,
)
from phase11_math_preflight import ContractError
from test_build_candidate_manifest import shape_only_fixture
from verify_candidate_family import audit_candidate_family


class ResumableCandidateLifecycle(unittest.TestCase):
    def test_checkpoint_reuses_actual_verified_pair_without_regeneration(self):
        (L,M),(fin,fout)=next(iter(canonical_pair_for_ratio().items()))
        calls=0
        def generator(a,b,budget):
            nonlocal calls
            calls+=1
            return shape_only_fixture(a,b,budget)
        with TemporaryDirectory() as root:
            work=Path(root)/"work"
            data,bits=stage_one(work,fin,fout,generator=generator)
            self.assertEqual(bits,256)
            self.assertEqual(calls,2)
            self.assertEqual(stage_one(work,fin,fout,generator=generator),
                             (data,bits))
            self.assertEqual(calls,2,"verified checkpoint should be reused")
            folder=work/f"ratio_L{L}_M{M}"
            self.assertEqual(len(list(folder.iterdir())),2)
            receipt=json.loads((folder/RECEIPT).read_text())
            self.assertEqual(receipt["backend"],"INJECTED_TEST_ONLY_NOT_MPFR")
            self.assertFalse(receipt["independent_generator_verified"])
            self.assertFalse(receipt["release_qualified"])
            self.assertFalse(receipt["product_admission_authorized"])
            with self.assertRaisesRegex(ContractError,"provenance"):
                stage_one(work,fin,fout)  # no synthetic backend in REAL run
            with self.assertRaisesRegex(ContractError,"all 14 checkpoints"):
                finalize(work,Path(root)/"cannot-publish")
            self.assertFalse((Path(root)/"cannot-publish").exists())

    def test_bounded_process_pool_generates_all_fourteen_checkpoints_and_resumes(self):
        with TemporaryDirectory() as root:
            work=Path(root)/"parallel"
            progress=[]
            completed=stage_all(
                work,workers=2,generator=shape_only_fixture,
                progress=progress.append)
            self.assertEqual(completed["ratios"],14)
            self.assertFalse(completed["product_admission_authorized"])
            self.assertEqual(len(list(work.iterdir())),14)
            self.assertEqual(
                [x["L"] for x in completed["assets"]],
                sorted(x["L"] for x in completed["assets"]))
            self.assertEqual(
                len([x for x in progress if x.startswith(
                    "MPFR_RATIO_CHECKPOINTED")]),14)
            resumed=stage_all(work,workers=2,generator=shape_only_fixture)
            self.assertEqual(completed,resumed)
            self.assertEqual(
                sum(x["bytes"] for x in completed["assets"]),11440952)
            for invalid in (0,5,1.0,True):
                with self.assertRaisesRegex(ContractError,"worker count"):
                    stage_all(Path(root)/"not_created",workers=invalid,
                              generator=shape_only_fixture)
            self.assertFalse((Path(root)/"not_created").exists())

    def test_failure_at_second_reproduction_never_leaks_checkpoint(self):
        (L,M),(fin,fout)=next(iter(canonical_pair_for_ratio().items()))
        count=0
        def bad(a,b,n):
            nonlocal count
            count+=1
            d,bits=shape_only_fixture(a,b,n)
            return (d[:-1]+bytes([d[-1]^1]) if count==2 else d),bits
        with TemporaryDirectory() as root:
            work=Path(root)/"work"
            with self.assertRaisesRegex(ContractError,"DOUBLE_RUN_REPRODUCTION_MISMATCH"):
                stage_one(work,fin,fout,generator=bad)
            self.assertFalse((work/f"ratio_L{L}_M{M}").exists())
            self.assertEqual(list(work.iterdir()),[])
            stage_one(work,fin,fout,generator=shape_only_fixture)
            self.assertTrue((work/f"ratio_L{L}_M{M}").is_dir())

    def test_corruption_tampered_receipt_and_extras_refuse_reuse(self):
        (L,M),(fin,fout)=next(iter(canonical_pair_for_ratio().items()))
        with TemporaryDirectory() as root:
            work=Path(root)/"work"
            stage_one(work,fin,fout,generator=shape_only_fixture)
            folder=work/f"ratio_L{L}_M{M}"
            asset=folder/f"RGSMSR1_L{L}_M{M}.bin"
            original=asset.read_bytes()
            asset.write_bytes(original[:-1]+bytes([original[-1]^1]))
            with self.assertRaises(ContractError):
                stage_one(work,fin,fout,generator=shape_only_fixture)
            asset.write_bytes(original)
            receipt_file=folder/RECEIPT
            original_receipt=receipt_file.read_text()
            fake=json.loads(original_receipt)
            fake["product_admission_authorized"]=True
            receipt_file.write_text(json.dumps(fake))
            with self.assertRaisesRegex(ContractError,"provenance"):
                stage_one(work,fin,fout,generator=shape_only_fixture)
            receipt_file.write_text(original_receipt)
            (folder/"unexpected.txt").write_text("no")
            with self.assertRaisesRegex(ContractError,"unexpected"):
                stage_one(work,fin,fout,generator=shape_only_fixture)

    def test_full_fourteen_synthetic_receipts_publish_only_nonrelease(self):
        with TemporaryDirectory() as root:
            work=Path(root)/"checkpoint"
            output=Path(root)/"final_candidate"
            complete=stage_all(work,generator=shape_only_fixture)
            self.assertEqual(complete["ratios"],14)
            self.assertFalse(complete["product_admission_authorized"])
            self.assertEqual(len(list(work.iterdir())),14)
            # Genuine --finalize NEVER accepts injected test provenance.
            with self.assertRaisesRegex(ContractError,"provenance"):
                finalize(work,output)
            self.assertFalse(output.exists())
            published=finalize(work,output,allow_test_fixture=True)
            self.assertTrue(output.is_dir())
            self.assertEqual(published["observed_ratio_count"],14)
            self.assertTrue(published["complete_ratio_family"])
            self.assertFalse(published["independent_generator_verified"])
            self.assertFalse(published["product_admission_authorized"])
            self.assertFalse(published["mpfr_dtft_oracle_passed"])
            self.assertFalse(published["q1_q6_passed"])
            integrity=audit_candidate_family(output)
            self.assertTrue(integrity["complete"])
            self.assertFalse(integrity["product_admission_authorized"])
            # Screens run ONCE on the actual cached candidate bytes, with
            # zero extra coefficient generator passes. Stubs are TEST ONLY.
            seen=[]
            def screen(data,fin,fout):
                seen.append((fin,fout))
                return dict(independent_precision_stable=True,
                            candidate_bytes_match_both=True,
                            release_qualified=False)
            screened_dir=Path(root)/"screened_nonrelease_candidate"
            screened=finalize(work,screened_dir,allow_test_fixture=True,
                              independent_screen=screen)
            self.assertEqual(len(seen),14)
            self.assertTrue(screened["independent_mpmath_screen_complete"])
            self.assertFalse(screened["independent_generator_verified"])
            self.assertFalse(screened["product_admission_authorized"])
            self.assertFalse(screened["q1_q6_passed"])
            self.assertTrue(audit_candidate_family(screened_dir)["complete"])
            # A failed late screen preserves all expensive checkpoints
            # and never partially publishes a purported 14-kernel result.
            failcount=0
            def fail_late(data,fin,fout):
                nonlocal failcount
                failcount+=1
                report=screen(data,fin,fout)
                report["candidate_bytes_match_both"]=failcount!=14
                return report
            broken=Path(root)/"not_published"
            with self.assertRaisesRegex(ContractError,
                                        "INDEPENDENT_NUMERICAL_SCREEN_FAILED"):
                finalize(work,broken,allow_test_fixture=True,
                         independent_screen=fail_late)
            self.assertEqual(failcount,14)
            self.assertFalse(broken.exists())
            self.assertEqual(len(list(work.iterdir())),14)
            # No overwrite of an existing published family.
            with self.assertRaisesRegex(ContractError,"already exists"):
                finalize(work,output,allow_test_fixture=True)
            checkpoint=next(work.iterdir())
            (checkpoint/"extra").write_text("no")
            with self.assertRaises(ContractError):
                finalize(work,Path(root)/"other",allow_test_fixture=True)
            self.assertFalse((Path(root)/"other").exists())


if __name__=="__main__":
    unittest.main(verbosity=2)
