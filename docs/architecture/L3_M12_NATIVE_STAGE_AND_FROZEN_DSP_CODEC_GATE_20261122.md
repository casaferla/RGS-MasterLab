# L3 M12 native Product REF_LINKED + codec focused Windows/Linux

One C01 focused gate targeting the existing L1 DSP runtime and the
Frozen M12 native implementation seam. The original MANUAL/USER/
UNLINKED default remains untouched. REF_LINKED gets true
ModuleInstance semantics and controller-only structure, without
allowing ordinary manual insertion/reorder/removal.

Exact typed frozen Dynamic EQ and Transient Shaper parameters use
the SAME native DSP JSON codec, with canonical schema IDs
rgsml.dsp.dynamic-eq.parameters/1.0.0 and
rgsml.dsp.transient-shaper.parameters/1.0.0. Strict source key
count protects against duplicate-key collapse, and filter shape
q/shelfSlope is union-discriminated. Legacy L1 schemas retain their
existing serializers. The staged accepted plan currently creates
real ProcessingChain REF_LINKED nodes with exact candidate parameters
and actual available registry factories, protecting fixed Manual.
This is NOT project serialization/transaction, receipt, production
activation or M13 verification yet.

Gate: new M12 native stage tests, native DSP processing chain and
structural integration tests plus existing module parameter codec
tests, on both Linux and Windows. M12 must remain nonrelease until
persistent Product owner, atomic transaction and terminal composition.
