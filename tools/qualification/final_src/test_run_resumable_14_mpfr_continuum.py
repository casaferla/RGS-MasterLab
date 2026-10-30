#!/usr/bin/env python3
"""Adversarial synthetic orchestration tests; no fake Q0 continuum release."""
from fractions import Fraction
import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from hashlib import sha256
from unittest.mock import patch
from zipfile import ZipFile

from phase11_math_preflight import ContractError, Ratio
from run_resumable_14_mpfr_continuum import (
    walk_ratio, authenticate_original_archive, _fair_ratio_order, _fair_band_order, _fair_attempts, PROVEN)


class Real14ContinuousCheckpoints(unittest.TestCase):
    def setUp(self):
        self.tmp=TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.r=Ratio.make(48000,96000)
        self.taps=tuple(Fraction(x) for x in (0,0,2,0,0))
        self.sha="a"*64
        self.calls=[]

    def _call(self, attempts, prover, **kw):
        return walk_ratio(self.r,self.taps,self.sha,self.root,
                          max_attempts=attempts,boxes=4,bits=256,
                          max_depth=6,engine_sha256="b"*64,
                          prover=prover,**kw)

    def _all_good(self,*args,**kwargs):
        self.calls.append((args[2],args[3],kwargs["kind"]))
        return dict(status=PROVEN,remaining_boxes=0,
                    proven_boxes=1,visited_boxes=1,release_qualified=False)

    def test_real_checkpoint_resume_no_duplicate_proof(self):
        first=self._call(1,self._all_good)
        self.assertEqual(first["attempted"],1)
        self.assertEqual(first["completed_bands"],1)
        self.assertEqual(first["band_evidence"]["passband"]["full_continuum_covered_nonrelease"],True)
        self.assertEqual(first["band_evidence"]["stopband"]["full_continuum_covered_nonrelease"],False)
        self.assertGreater(first["pending_nodes"],0)
        final=self._call(4,self._all_good)
        self.assertEqual(final["attempted"],1)
        self.assertEqual(final["pending_nodes"],0)
        self.assertEqual(final["completed_bands"],2)
        self.assertEqual(len(self.calls),2)
        replay=self._call(0,self._all_good)
        self.assertEqual(replay["attempted"],0)
        self.assertEqual(replay["pending_nodes"],0)
        self.assertEqual(replay["proven_leaves"],2)

    def test_explicit_split_preserves_exact_rational_partition(self):
        def split_first(r,t,lo,hi,**kw):
            self.calls.append((lo,hi,kw["kind"]))
            if kw["kind"]=="passband" and hi-lo>Fraction(12*48000,50):
                return dict(status="UNRESOLVED",remaining_boxes=1,
                            proven_boxes=0,visited_boxes=1,release_qualified=False)
            return dict(status=PROVEN,remaining_boxes=0,
                        proven_boxes=1,visited_boxes=1,release_qualified=False)
        result=self._call(8,split_first)
        self.assertEqual(result["pending_nodes"],0)
        self.assertEqual(result["proven_leaves"],3)
        self.assertEqual(result["attempted"],4)
        saved=list(self.root.glob("*.json"))
        self.assertEqual(len(saved),4)
        self.assertEqual(self._call(0,split_first)["attempted"],0)
        root=json.loads((self.root/"L2_M1_passband_r.json").read_text())
        self.assertEqual(root["status"],"SPLIT_UNRESOLVED")
        self.assertEqual(root["lo_hz"],"0")
        self.assertEqual(root["hi_hz"],str(Fraction(12*48000,25)))

    def test_checkpoint_tampering_fails_closed(self):
        self._call(2,self._all_good)
        p=self.root/"L2_M1_passband_r.json"
        original=json.loads(p.read_text())
        for mutate in (lambda d:d.update(candidate_sha256="c"*64),
                       lambda d:d.update(status="SPLIT_UNRESOLVED"),
                       lambda d:d.update(release_qualified=True),
                       lambda d:d["proof_result"].update(remaining_boxes=1)):
            modified=json.loads(json.dumps(original))
            mutate(modified)
            p.write_text(json.dumps(modified))
            with self.assertRaises(ContractError):
                self._call(0,self._all_good)
        p.write_text(json.dumps(original))
        self.assertEqual(self._call(0,self._all_good)["pending_nodes"],0)

    def test_unresolved_never_claimed_proven(self):
        def unresolved(*a,**k):
            return dict(status="UNRESOLVED",remaining_boxes=2,
                        proven_boxes=0,release_qualified=False)
        result=self._call(1,unresolved)
        self.assertEqual(result["proven_leaves"],0)
        self.assertGreater(result["pending_nodes"],0)
        for wrong in (lambda *a,**k: dict(status=PROVEN,remaining_boxes=1,
                                      proven_boxes=1,release_qualified=False),
                      lambda *a,**k: dict(status=PROVEN,remaining_boxes=0,
                                      proven_boxes=1,release_qualified=True)):
            another=self.root/"other"
            another.mkdir(exist_ok=True)
            with self.assertRaises(ContractError):
                walk_ratio(self.r,self.taps,self.sha,another,
                           max_attempts=2,boxes=4,bits=256,max_depth=4,
                           engine_sha256="b"*64,prover=wrong)

    def test_archive_provenance_missing_repacked_and_folder_tamper(self):
        private=self.root/"candidate"
        private.mkdir()
        (private/"asset.bin").write_bytes(b"ORIGINAL-KERNEL")
        (private/"manifest.json").write_text("ORIGINAL-MANIFEST")
        original=self.root/"original.zip"
        with ZipFile(original,"w") as z:
            for file in private.iterdir():
                z.write(file,file.name)
        with self.assertRaisesRegex(ContractError,"frozen SHA"):
            authenticate_original_archive(private,original)
        with patch("run_resumable_14_mpfr_continuum.CANDIDATE_ZIP_SHA",
                   sha256(original.read_bytes()).hexdigest()):
            with self.assertRaisesRegex(ContractError,"inventory"):
                authenticate_original_archive(private,original)
            # Even after supplying a complete 15-member ZIP the exact
            # member bytes must match, not merely a self-consistent manifest.
            for n in range(13):
                (private/f"extra-{n}.bin").write_bytes(b"original-"+str(n).encode())
            with ZipFile(original,"w") as z:
                for file in private.iterdir(): z.write(file,file.name)
        with patch("run_resumable_14_mpfr_continuum.CANDIDATE_ZIP_SHA",
                   sha256(original.read_bytes()).hexdigest()):
            self.assertEqual(authenticate_original_archive(private,original),
                             sha256(original.read_bytes()).hexdigest())
            (private/"asset.bin").write_bytes(b"SWAPPED-KERNEL")
            with self.assertRaisesRegex(ContractError,"bytes differ"):
                authenticate_original_archive(private,original)

    def test_fair_band_order_reaches_stopband_on_resume(self):
        bands=(("passband",Fraction(0),Fraction(12*48000,25)),
               ("stopband",Fraction(24000),Fraction(48000)))
        self.assertEqual(_fair_band_order(self.r,bands,self.root)[0][0],"passband")
        first=self._call(1,self._all_good)
        self.assertEqual(first["completed_bands"],1)
        self.assertEqual(_fair_band_order(self.r,bands,self.root)[0][0],"stopband")
        resumed=self._call(1,self._all_good)
        self.assertEqual(resumed["attempted"],1)
        self.assertEqual(resumed["completed_bands"],2)
        self.assertEqual(self.calls[-1][2],"stopband")
        self.assertEqual(self._call(0,self._all_good)["completed_bands"],2)

    def test_partial_split_never_claims_whole_band(self):
        def split(*args,**kwargs):
            return dict(status="UNRESOLVED",remaining_boxes=1,
                        proven_boxes=0,visited_boxes=1,release_qualified=False)
        state=self._call(1,split)
        self.assertEqual(state["completed_bands"],0)
        self.assertGreater(state["pending_nodes"],0)
        self.assertFalse(state["band_evidence"]["passband"]["full_continuum_covered_nonrelease"])
        self.assertFalse(state["band_evidence"]["stopband"]["full_continuum_covered_nonrelease"])

    def test_short_budget_fairly_reaches_unvisited_ratios(self):
        pairs={(1,2):(88200,44100),(2,1):(48000,96000),
               (4,1):(44100,176400)}
        (self.root/"L1_M2_passband_r.json").write_text("historical")
        (self.root/"L1_M2_passband_r0.json").write_text("historical")
        order=_fair_ratio_order(pairs,self.root)
        self.assertEqual([x[0] for x in order],[(2,1),(4,1),(1,2)])
        self.assertEqual(_fair_attempts(8,14),1)
        self.assertEqual(_fair_attempts(24,14),2)
        self.assertEqual(_fair_attempts(0,14),0)
        with self.assertRaises(ContractError):
            _fair_attempts(-1,14)

    def test_bad_budget_refused(self):
        for opts in (dict(max_attempts=-1),dict(boxes=0),dict(bits=128),
                     dict(max_depth=25),dict(max_attempts=True)):
            cfg=dict(max_attempts=1,boxes=4,bits=256,max_depth=5)
            cfg.update(opts)
            with self.assertRaises(ContractError):
                walk_ratio(self.r,self.taps,self.sha,self.root,
                           engine_sha256="b"*64,prover=self._all_good,**cfg)
        self.assertFalse(any(self.root.iterdir()))

if __name__=="__main__":
    unittest.main(verbosity=2)