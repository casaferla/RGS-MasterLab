# L3-M09 P03 — Stereo/Mono-Bass frozen discrete and low seed (nonrelease)

Sources: M09 P03 and M09 PO closure (7 October 2026) ACCEPTED_FROZEN.
The seven abstract alternatives include OFF and LR12/LR24 at
80/120/200Hz. Active LR modes at lowWidth100 are distinct from OFF,
even with an identical amplitude percentage. The discrete records
do not assert actual ModuleRegistry factories, legality, selection or
availability. Selecting one cutoff does not delete other S05/S06
nested cumulative targets or no-low-widening/correlation constraints.

The pure analytical ideal low-lane seed implements:
- standalone `100*2^(targetLow-baselineLow)`;
- combined `100*2^(targetLow-baselineLow-(sideGainDb-midGainDb)/
  (20log10(2)))`, with separately attested bounded broadband gains;
- explicit [0,100] seed limitation retaining raw data, without
  altering DesiredTarget or asserting S05 attainable;
- fail-closed absent exact insertion baseline/M11 evidence, mono input,
  zero-Side cannot widen, and separately distinct exact Side-mute
  endpoint; positive underflow is not interpreted as exact mute.

It does not account for actual crossover phase, SIDE EQ upstream
effects, stereo no-regression or fresh audio measurement. Later
M09 candidate generation, M10 real render, M11/M12 ownership and
M13 verifier still own those requirements.
