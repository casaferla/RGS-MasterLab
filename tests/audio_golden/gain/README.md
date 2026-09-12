# Gain-v1 golden vectors

`expected_bits.hpp` contains checked-in IEEE-754 binary64 input, factor, and
output bit patterns for the frozen Gain-v1 law. They were produced independently
from production C++ with `generate_gain_oracle.py`, gmpy2 2.3.1, MPFR 4.2.2,
256-bit precision, and round-to-nearest/ties-to-even conversion to binary64.

Ordinary configure, build, and test consume only the checked-in values. They do
not require Python, gmpy2, MPFR, network access, or dependency installation.
