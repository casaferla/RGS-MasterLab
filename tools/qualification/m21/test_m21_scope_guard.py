#!/usr/bin/env python3
"""L1-M21 fail-closed activation/authority guard; NOT Gate S or M."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[3]
src=(ROOT/"app/src/internal/m21_metrics_comparison_candidate.hpp").read_text()
test=(ROOT/"tools/qualification/m21/m21_comparison_selftest.cpp").read_text()
contract=(ROOT/"docs/architecture/M21_PARALLEL_PREPRODUCTION_BRANCH_CONTRACT_20261103.md").read_text()
assert 'Gold - Current' in contract and 'PREPARED | GOLD | PROCESSED' in contract
assert 'NO MERGE' in contract and 'M17' in contract and 'M18' in contract
assert 'M19' in contract and 'M20' in contract
assert 'const double result=*g.unrounded-*a.unrounded' in src
assert 'const double d=*g->normalized_shape_db.unrounded -' in src
assert 'PrivatePublicationGuard' in src and 'monotonic_token<=expected_[i]' in src
assert 'Canonical M21 Gate S: BLOCKED.' in contract and 'Real Windows Manual Gate M: BLOCKED.' in contract
assert 'M21' not in (ROOT/"ui/qml/Main.qml").read_text()
assert not re.search(r'm21_metrics_comparison_candidate', (ROOT/"app/CMakeLists.txt").read_text(),re.I)
assert not re.search(r'M21', (ROOT/"ui/CMakeLists.txt").read_text(),re.I)
for bad in ('Match Amount','target_proximity','reference_match_plan','level_match_gain'):
    assert bad not in src, bad
assert 'tonal_delta(&p,&qg,100)' in test
print('M21_PRIVATE_SCOPE_AND_NONACTIVATION_GUARD_PASS_NOT_S_OR_M')
