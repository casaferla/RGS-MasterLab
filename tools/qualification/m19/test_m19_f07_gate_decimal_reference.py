#!/usr/bin/env python3
"""M19 F07/F04 independent 100-digit Decimal sufficient-power oracle.

The fixture inputs are exact binary64 dyadics. The comparison *laws* come
from the accepted BS.1770-5/2026-10-04 preaudit, not M19 C++. This is NOT
an official raw-PCM fixture, canonical QTP or Gate G release approval.
"""
from decimal import Decimal, localcontext
from math import isfinite, nextafter
from sys import float_info

def d(value: float) -> Decimal:
    assert isfinite(value) and value >= 0
    return Decimal.from_float(value)

def gate(q):
    weights=[d(x) for x in q]
    strict_abs = Decimal(10) ** ((Decimal("-70") + Decimal("0.691")) / Decimal(10))
    a=[x for x in weights if x>strict_abs]
    if not a:
        return (0,0,None)
    relative=sum(a)/(Decimal(10)*len(a))
    r=[x for x in a if x>relative]
    assert r, "nonempty absolute-active set retains its maximum"
    lufs=Decimal("-0.691")+Decimal(10)*(sum(r)/len(r)).log10()
    return (len(a),len(r),lufs)

def run():
    with localcontext() as ctx:
        ctx.prec=100
        mx=float_info.max
        large_cases=[
          ([1e308,1e308],2,2),
          ([mx,mx],2,2),
          ([mx,mx/2],2,2),
          ([1e308]*1000,1000,1000),
          ([1e308,1e-6],2,1),
          ([1e308,1e306],2,1),
          ([1e308,1e307],2,2),
          ([mx/2]*10000,10000,10000),
        ]
        for powers,abs_count,retained_count in large_cases:
            n,k,lufs=gate(powers)
            assert (n,k)==(abs_count,retained_count)
            assert lufs is not None and lufs.is_finite()
            nr,kr,lr=gate(list(reversed(powers)))
            assert (nr,kr)==(n,k) and lr==lufs

        edge=nextafter(1.0,2.0)
        expected=[
            ([19.0,1.0],2,1),
            ([19.0,edge],2,2),
            ([901.0]+[1.0]*99,100,1),
            ([901.0]+[edge]*99,100,100),
        ]
        results=[]
        for values,na,nr in expected:
            n,k,l=gate(values)
            assert (n,k)==(na,nr)
            assert l is not None and l.is_finite()
            results.append(l)
        assert results[0]>results[1]+Decimal("2.7")
        assert results[2]>results[3]+Decimal("19.54")

        # As Decimal is exact over dyadic source samples, all q are finite
        # although *binary64 sequential sums* of some vectors overflow.
        assert d(mx)*2 > d(mx)
        assert gate([0.0,-0.0])==(0,0,None)
        print("M19_F07_POWER_GATE_DECIMAL_100_DIGIT_PASS_NOT_QTP")

if __name__=="__main__":
    run()
