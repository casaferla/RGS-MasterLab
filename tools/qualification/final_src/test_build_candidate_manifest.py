#!/usr/bin/env python3
"""Fast Q0 lifecycle tests for non-release atomic Phase-11 candidate builder."""
from pathlib import Path
import struct
from tempfile import TemporaryDirectory
import unittest

from build_candidate_manifest import assemble_candidate, canonical_pair_for_ratio
from phase11_math_preflight import (
    HEADER, MAGIC, Ratio, ContractError, kernel_shape,
)


def shape_only_fixture(fin, fout, precision):
    """Structurally valid synthetic CENTER DELTA, NEVER a real Kaiser kernel."""
    ratio = Ratio.make(fin, fout)
    shape = kernel_shape(ratio)
    payload = bytearray(8 * shape.N)
    struct.pack_into("<d", payload, 8 * shape.D, 1.0)
    data = HEADER.pack(MAGIC, 1, 1, shape.L, shape.M, shape.T, 0,
                       shape.N, shape.N, len(payload)) + payload
    return data, 256


class CandidateManifestLifecycle(unittest.TestCase):
    def test_all_fourteen_ratio_metadata_and_subset_is_not_release(self):
        allowed = canonical_pair_for_ratio()
        self.assertEqual(len(allowed), 14)
        sample = dict(list(allowed.items())[:2])
        with TemporaryDirectory() as d:
            target = Path(d) / "candidate"
            result = assemble_candidate(
                target, generator=shape_only_fixture, pairs=sample)
            self.assertTrue(target.is_dir())
            self.assertFalse(result["complete_ratio_family"])
            self.assertFalse(result["independent_generator_verified"])
            self.assertFalse(result["q1_q6_passed"])
            self.assertFalse(result["product_admission_authorized"])
            self.assertEqual(result["observed_ratio_count"], 2)
            self.assertEqual(len(list(target.glob("RGSMSR1_*.bin"))), 2)
            self.assertTrue((target / "CANDIDATE_MANIFEST_NOT_FOR_RELEASE.json").is_file())
            with self.assertRaises(ContractError):
                assemble_candidate(target, generator=shape_only_fixture, pairs=sample)

    def test_mismatched_repeat_never_publishes_partial_assets(self):
        allowed = canonical_pair_for_ratio()
        sample = dict(list(allowed.items())[:1])
        counter = 0

        def divergent(fin, fout, precision):
            nonlocal counter
            counter += 1
            data, bits = shape_only_fixture(fin, fout, precision)
            if counter == 2:
                data = bytearray(data)
                data[-1] ^= 1
                data = bytes(data)
            return data, bits

        with TemporaryDirectory() as d:
            target = Path(d) / "cannot_publish"
            with self.assertRaisesRegex(ContractError, "REPRODUCTION_MISMATCH"):
                assemble_candidate(target, generator=divergent, pairs=sample)
            self.assertFalse(target.exists())
            self.assertFalse(any(Path(d).iterdir()),
                             "no partial temp assets should remain after failure")

    def test_reject_noncanonical_pair_and_precision_before_output(self):
        with TemporaryDirectory() as d:
            target = Path(d) / "invalid"
            with self.assertRaises(ContractError):
                assemble_candidate(target, generator=shape_only_fixture,
                                   max_precision=128)
            with self.assertRaises(ContractError):
                assemble_candidate(target, generator=shape_only_fixture,
                                   pairs={(2, 1): (44100, 192000)})
            self.assertFalse(target.exists())


if __name__ == "__main__":
    unittest.main(verbosity=2)
