# L3-M11 P03 — Protected component no-regression hard gate

Authority: M11 PO closure P03 ACCEPTED_FROZEN 7 Oct 2026.
For EVERY protected required canonical K component:
- abs(G-P)<=d means abs(C-G)<=d;
- otherwise s=sign(G-P), s*C>=min(s*F,s*G)-d AND
  abs(C-G)<=abs(F-G)+d.
No aggregate compensation, reduced K, new 1e-9 slack, or soft J
penalty is admissible. Invalid/missing feature values, Fixed snapshot,
endpoint identity or independent safety eligibility fail closed.
The guard is read-only; it is not M13 verification and supplies no
persisted plan, audio, changed Input Gain or publication rights.
