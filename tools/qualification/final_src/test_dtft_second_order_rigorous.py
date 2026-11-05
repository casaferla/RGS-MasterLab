#!/usr/bin/env python3
"""Fast adversarial second-order MPFR proof checks; no false release."""
import unittest
from fractions import Fraction
from unittest.mock import patch

from phase11_math_preflight import ContractError, Ratio
from dtft_second_order_rigorous import (
    enclose_second_order, prove_window)


class SecondOrderProofTests(unittest.TestCase):
    def setUp(self):
        self.r = Ratio.make(48000,96000)

    def test_exact_delta_passes_one_whole_band_but_cannot_pass_stopband(self):
        # Synthetic h=[0,0,2,0,0] is unity for every real frequency.
        taps=tuple(map(Fraction,(0,0,2,0,0)))
        a=prove_window(self.r,taps,0,48000,kind="passband",max_boxes=1)
        self.assertEqual(a["status"],"CONTINUUM_SUBBAND_PROVEN_NONRELEASE")
        self.assertEqual(a["remaining_boxes"],0)
        self.assertFalse(a["release_qualified"])
        stop=prove_window(self.r,taps,24000,48000,
                          kind="stopband",max_boxes=3)
        self.assertEqual(stop["status"],"UNRESOLVED")
        self.assertFalse(stop["release_qualified"])

    def test_public_api_always_calculates_real_second_moment(self):
        # A public caller cannot supply an artificially tiny Hessian bound.
        taps=tuple(map(Fraction,(0,Fraction(1,2),1,Fraction(1,2),0)))
        lo,hi=enclose_second_order(self.r,taps,1000,1100,bits=256)
        self.assertLessEqual(lo,hi)
        import dtft_second_order_rigorous as module
        original=module.second_moment
        with patch.object(module,"second_moment",wraps=original) as spy:
            prove_window(self.r,taps,1000,1010,kind="passband",max_boxes=4)
            self.assertEqual(spy.call_count,1)
        # This filter has wrong normalization and must NOT certify passband.
        self.assertEqual(prove_window(self.r,taps,1000,1010,
                         kind="passband",max_boxes=4)["status"],"UNRESOLVED")

    def test_invalid_box_and_assets_fail_closed(self):
        taps=tuple(map(Fraction,(0,0,2,0,0)))
        for args in ((-1,0),(0,96000),(2,1)):
            with self.subTest(args=args):
                with self.assertRaises(ContractError):
                    enclose_second_order(self.r,taps,*args)
        with self.assertRaises(ContractError):
            enclose_second_order(self.r,(Fraction(1),Fraction(2),
                                         Fraction(3)),0,1)
        with self.assertRaises(ContractError):
            prove_window(self.r,taps,1,2,kind="passband",max_boxes=0)
        with self.assertRaises(ContractError):
            prove_window(self.r,taps,1,2,kind="unknown",max_boxes=8)
        with self.assertRaises(ContractError):
            prove_window(self.r,taps,1,2,kind="passband",max_boxes=True)

    def test_second_order_bounds_cover_actual_sampled_trig(self):
        # Not a standalone proof: the analytic curvature enclosure IS the
        # proof, sampled checks here catch sign/scaling implementation bugs.
        import math
        taps=(Fraction(1,64),Fraction(-1,8),
              Fraction(9,4),Fraction(-1,8),Fraction(1,64))
        a,b=11000,11100
        lo,hi=enclose_second_order(self.r,taps,a,b,bits=256)
        for i in range(41):
            f=a+(b-a)*i/40
            actual=(float(taps[2])+
                    2*sum(float(taps[2+u])*math.cos(
                        2*math.pi*f*u/(self.r.fin*self.r.L))
                        for u in range(1,3)))/self.r.L
            self.assertGreaterEqual(actual,float(lo))
            self.assertLessEqual(actual,float(hi))


if __name__=="__main__":
    unittest.main(verbosity=2)
