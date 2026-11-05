#!/usr/bin/env python3
"""Regression guard for the actual Qt 14-Q0 + 36-Q1 fixture directory contract.

This tests CODE WIRING only, NOT that a genuine native Q1 binary actually ran.
An unskipped native Qt run with frozen private inputs is still mandatory.
"""
from __future__ import annotations

from pathlib import Path
import re
import unittest

TEST_NAME = "phase11External14RealCandidatesAll36OfflineNativeRehearsal"
Q0 = "RGSML_PHASE11_REAL_14_Q0_DIR"
Q1 = "RGSML_PHASE11_REAL_36_Q1_DIR"
LEGACY = "RGSML_PHASE11_REAL_14_Q1_EXPECTED_DIR"


def verify_source_method(source: str) -> None:
    # Limit inspection to ONE exact method, not unrelated Qt tests.
    begin = source.find("void PlaybackSampleRateAdapterTest::\n" + TEST_NAME + "()")
    if begin < 0:
        raise AssertionError("genuine 36-Q1 native Qt test method absent")
    # The next Qt slot MAY be another private genuine-asset test. Limit
    # the audit to the 36-Q1 METHOD, never include a following 15-alias QSKIP.
    next_slot = source.find("\nvoid PlaybackSampleRateAdapterTest::", begin + 10)
    namespace_end = source.find("}  // namespace rgsml::tests", begin)
    if namespace_end < 0:
        raise AssertionError("genuine 36-Q1 native Qt test body truncated")
    end = min(next_slot, namespace_end) if next_slot >= 0 else namespace_end
    method = source[begin:end]
    env = re.findall(r'qgetenv\("([A-Z][A-Z0-9_]+)"\)', method)
    if env != [Q0,Q1]:
        raise AssertionError(f"native fixture must use ONE Q0 and ONE Q1 directory: {env!r}")
    if LEGACY in method or "expectedRoot" in method:
        raise AssertionError("stale duplicate Q1 oracle directory/loop is forbidden")
    required = (
        "Q1_EXPECTED_MANIFEST_NONRELEASE.json",
        "0072fb8cf2fa5930c8200c67cac21d710724b0eb0ee1f2d73e9dbf9798938d37",
        "expectedRows.size(),36",
        "Q1_EXPECTED_",
        "interleaved_f64le_sha256",
        "std::bit_cast<std::uint64_t>(observed),expectedBits",
        "channel_bits(partitioned.value()->pcm,ch)",
        "QCOMPARE(identities,6)",
        "QCOMPARE(conversions,30)",
        "QCOMPARE(static_cast<int>(std::count(seen.begin(),seen.end(),true)),14)",
    )
    for token in required:
        if token not in method:
            raise AssertionError("native Q1 test dropped a critical guard: " + token)
    if method.count("QSKIP(") != 1:
        raise AssertionError("native real-data test must SKIP only for absent Q0 fixture")


def verify_native_15_alias_source_method(source: str) -> None:
    name = "phase11External14RealCandidates15NativeAliasSineRehearsal"
    begin = source.find("void PlaybackSampleRateAdapterTest::\n" + name + "()")
    if begin < 0:
        raise AssertionError("real native 15-alias Qt test missing")
    end = source.find("}  // namespace rgsml::tests", begin)
    if end < 0:
        raise AssertionError("native 15-alias method unterminated")
    method = source[begin:end]
    required = (
        'qgetenv("' + Q0 + '")',
        "const double maxAliasAmplitude",
        "minAttenuationDb=140.0",
        "materialize_phase11_pinned_final_src_candidate(",
        "sourceFrames=fin/10",
        "outputFrames=fout/10",
        "std::sin(phase)",
        "QCOMPARE(digest,std::string(it->sha))",
        "aliasAmplitude<=maxAliasAmplitude",
        "QCOMPARE(tested,15)",
        "ratiosSeen.begin(),ratiosSeen.end(),true)),7",
    )
    if method.count("QSKIP(") != 1:
        raise AssertionError("exactly one SKIP permitted: absent original Q0 folder")
    if "RGSML_PHASE11_REAL_14_Q1_EXPECTED_DIR" in method:
        raise AssertionError("legacy reference folder in native 15-alias test")
    for key in required:
        if key not in method:
            raise AssertionError("native 15-alias test missing guard: "+key)


class RealQ1ContractSourceTests(unittest.TestCase):
    def setUp(self):
        self.path = (Path(__file__).resolve().parents[3] / "tests" /
                     "unit" / "audio" / "test_playback_sample_rate_adapter.cpp")
        self.source = self.path.read_text(encoding="utf-8")

    def test_exact_native_q0_q1_folder_contract(self):
        verify_source_method(self.source)
        verify_native_15_alias_source_method(self.source)

    def test_native_alias_threshold_tamper_must_fail(self):
        with self.assertRaises(AssertionError):
            verify_native_15_alias_source_method(
                self.source.replace("minAttenuationDb=140.0",
                                    "minAttenuationDb=120.0",1))

    def test_native_alias_sha_pin_tamper_must_fail(self):
        with self.assertRaises(AssertionError):
            verify_native_15_alias_source_method(
                self.source.replace("QCOMPARE(digest,std::string(it->sha))",
                                    "QVERIFY(!digest.empty())",1))

    def test_stale_legacy_env_must_fail(self):
        with self.assertRaises(AssertionError):
            verify_source_method(self.source.replace(
                Q1, LEGACY, 1))

    def test_second_duplicate_q1_variable_must_fail(self):
        injected = self.source.replace(
            'const QByteArray referenceRoot=qgetenv("' + Q1 + '");',
            'const QByteArray referenceRoot=qgetenv("' + Q1 + '");\n'
            '    const QByteArray expectedRoot=qgetenv("' + LEGACY + '");',
            1)
        with self.assertRaises(AssertionError):
            verify_source_method(injected)

    def test_missing_bitwise_oracle_comparison_must_fail(self):
        with self.assertRaises(AssertionError):
            verify_source_method(self.source.replace(
                "std::bit_cast<std::uint64_t>(observed),expectedBits",
                "std::bit_cast<std::uint64_t>(observed),0", 1))


if __name__ == "__main__":
    unittest.main(verbosity=2)
