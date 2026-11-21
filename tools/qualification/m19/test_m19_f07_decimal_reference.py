#!/usr/bin/env python3
"""Independent high-precision M19 F07 numerical range oracle.

Separate Python Decimal arithmetic on *exact represented binary64* inputs.
This oracle intentionally does not import M19 C++ or invent an accepted QTP,
numeric floor, error/status mapping, K bank, or product qualification.
"""
import math
import sys
from decimal import Decimal, localcontext

def run():
    with localcontext() as ctx:
        ctx.prec = 100
        two = Decimal(2)
        n = Decimal(4097)
        # Frozen 2026-10-04 adversary 1+4096*(2^-30)^2 = 1+2^-48.
        square_sum = Decimal(1) + two**-48
        power = square_sum / n
        exact_db = Decimal(10)*power.log10()
        baseline_db = Decimal(10)*(Decimal(1)/n).log10()
        assert exact_db > baseline_db
        assert Decimal("1.0e-14") < exact_db-baseline_db < Decimal("2.0e-14")
        assert abs(float(exact_db)-10*math.log10(float(power))) < 3e-14

        # Exact, positive mean-square without any binary64 squaring overflow.
        for exponent in (-600,-510,0,510,512):
            amplitude = two**exponent
            exact_level = Decimal(20)*amplitude.log10()
            approx_level = 20*math.log10(math.ldexp(1.0,exponent))
            assert abs(Decimal.from_float(approx_level)-exact_level) < Decimal("2e-12")

        min_subnormal = math.nextafter(0.0,1.0)
        max_finite = sys.float_info.max
        for sample in (min_subnormal,max_finite):
            dyadic = Decimal.from_float(sample)
            exact_db = Decimal(20)*dyadic.log10()
            assert math.isfinite(float(exact_db))
            approx = 20*math.log10(sample)
            assert abs(Decimal.from_float(approx)-exact_db) < Decimal("3e-12")

        # A peak of unity and three exact zeros yields 6.020599913... dB
        # Crest (Sample Peak minus RMS, not True Peak minus RMS).
        impulse_power=Decimal(1)/Decimal(4)
        expected_rms=Decimal(10)*impulse_power.log10()
        expected_crest=-expected_rms
        assert abs(expected_crest-Decimal("6.020599913279623"))<Decimal("1e-14")

        # Negative zero and +zero are *identical silence* as a power metric;
        # no fake 0 dBFS, -Inf or NaN payload.
        assert Decimal.from_float(-0.0)**2 == Decimal(0)
        assert Decimal.from_float(+0.0)**2 == Decimal(0)

    print("M19_F07_DECIMAL_100_DIGIT_ORACLE_PASS_NOT_CANONICAL_QTP")

if __name__=="__main__":
    run()
