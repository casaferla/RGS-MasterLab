#!/usr/bin/env python3
"""Bounded adversarial tests of sound MPFR 3rd-order proof; NONRELEASE."""
import math
import unittest
from fractions import Fraction
from unittest.mock import patch

from dtft_third_order_rigorous import (
    enclose_third_order, exact_third_moment, prove_window)
from phase11_math_preflight import ContractError, Ratio


class ThirdOrderMPFRTests(unittest.TestCase):
    def setUp(self):
        self.r=Ratio.make(48000,96000)

    def test_unity_proves_passband_but_never_fake_stopband(self):
        taps=tuple(Fraction(x) for x in (0,0,2,0,0))
        p=prove_window(self.r,taps,0,23040,kind="passband",max_boxes=1)
        self.assertEqual(p["status"],"CONTINUUM_SUBBAND_PROVEN_NONRELEASE")
        self.assertEqual(p["remaining_boxes"],0)
        self.assertIs(p["release_qualified"],False)
        s=prove_window(self.r,taps,34000,34001,kind="stopband",max_boxes=3)
        self.assertEqual(s["status"],"UNRESOLVED")
        self.assertIs(s["release_qualified"],False)

    def test_public_enclosure_does_not_accept_untrusted_third_moment(self):
        taps=tuple(map(Fraction,(1,2,3,2,1)))
        with patch("dtft_third_order_rigorous.exact_third_moment",
                   wraps=exact_third_moment) as spy:
            low,high=enclose_third_order(self.r,taps,3000,3001)
            self.assertEqual(spy.call_count,1)
            self.assertLess(low,high)

    def test_invalid_inputs_rejected_and_budget_unresolved(self):
        taps=tuple(Fraction(x) for x in (0,0,2,0,0))
        for lo,hi in ((-1,0),(2,1),(0,96000)):
            with self.subTest(lo=lo,hi=hi):
                with self.assertRaises(ContractError):
                    enclose_third_order(self.r,taps,lo,hi)
        with self.assertRaises(ContractError):
            enclose_third_order(self.r,tuple(map(Fraction,(1,2,3))),0,1)
        for wrong in (0,True,-3,1_000_001):
            with self.subTest(budget=wrong):
                with self.assertRaises(ContractError):
                    prove_window(self.r,taps,10,20,kind="stopband",max_boxes=wrong)
        result=prove_window(self.r,taps,34000,34001,kind="stopband",max_boxes=1)
        self.assertEqual(result["status"],"UNRESOLVED")
        self.assertGreater(result["remaining_boxes"],0)

    def test_independent_sampled_values_enclosed(self):
        # Sampling only checks our implementation; the actual proof is the
        # directed MPFR Taylor enclosure over each continuous frequency box.
        filters=(
            tuple(Fraction(x) for x in (0,0,2,0,0)),
            (Fraction(1,64),Fraction(-1,8),Fraction(9,4),
             Fraction(-1,8),Fraction(1,64)),
            tuple(Fraction(x,4) for x in (1,2,4,2,1)),
        )
        for taps in filters:
            for a,b in ((0,100),(3000,3001),(34000,34020),(47990,48000)):
                low,high=enclose_third_order(self.r,taps,a,b)
                d=len(taps)//2
                for i in range(51):
                    f=a+(b-a)*i/50
                    val=(float(taps[d])+2*sum(
                        float(taps[d+u])*math.cos(
                            2*math.pi*f*u/(self.r.fin*self.r.L))
                        for u in range(1,d+1)))/self.r.L
                    self.assertLessEqual(float(low)-1e-12,val)
                    self.assertLessEqual(val,float(high)+1e-12)


if __name__=="__main__":
    unittest.main(verbosity=2)
