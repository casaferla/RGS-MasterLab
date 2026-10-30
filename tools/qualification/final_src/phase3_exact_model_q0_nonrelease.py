#!/usr/bin/env python3
"""Exact rational check of Work Phase3's declared FFT/underflow envelopes.

A public synthetic arithmetic check, NOT canonical Q0 approval, proof of the
C++ FFT producer, proof of MPFR/compiler correctness, or SRC admission.
"""
from fractions import Fraction as F


def verify_phase3_error_model_nonrelease():
    u = F(1, 2**53)
    eta = F(1, 2**38)
    gamma2 = 2*u/(1-2*u)
    alpha = (1+3*u)*(1+2*gamma2)
    recurrence = (1+3*u)*alpha**255-1+F(1, 2**1000)
    safe = (1+16*u)**256-1
    assert 0 <= recurrence < safe < eta, 'twiddle upper envelope'

    butterfly_local = eta+2*gamma2*(1+eta)+u*(1+eta+2*gamma2*(1+eta))
    c = 4*eta+64*u
    assert butterfly_local < c, 'local butterfly bound'
    assert 22*c < 1, 'FFT depth denominator'
    q = 22*c/(1-22*c)
    gamma81 = 81*u/(1-81*u)
    total = q*(1+gamma81)+gamma81
    assert 0 < total < F(1, 2**29), 'FFT+input absolute-error bound'
    assert 1/(1-gamma81) < 1+F(1, 2**40), 'normalization stability'

    # Number of real elementary operations for 20 transforms, each radix-2.
    assert 20*10*(2**22//2)*22 < 2**34
    assert 3**22 < 2**35, 'real-component propagation estimate'
    assert F(2**69, 2**1075) < F(1, 2**900), 'gradual underflow bound'
    # Source's sufficient 1/30000 amplitude ripple constraint.
    assert F(20, 29999) < F(1, 1000), 'passband ripple dB envelope'
    return {
        'status': 'PHASE3_EXACT_RATIONAL_ENVELOPES_PASS_NONRELEASE',
        'maximum_fft_log2': 22,
        'taylor_order': 20,
        'twiddle_recurrence_less_than': '2^-38',
        'fft_input_absolute_error_less_than': '2^-29',
        'underflow_absolute_error_less_than': '2^-900',
        'release_qualified': False,
        'canonical_q0_approved': False,
        'product_admission_authorized': False,
    }


if __name__ == '__main__':
    import json
    print(json.dumps(verify_phase3_error_model_nonrelease(), sort_keys=True))
