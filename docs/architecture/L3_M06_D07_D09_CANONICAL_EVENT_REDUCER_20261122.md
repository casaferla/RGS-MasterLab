# L3 M06 D07–D09 descriptor reducer from exact native P04 events

Authority: Phase6 PO P04 accepted frozen (200Hz rational grid,
20ms, frame-center onset, RAW positive spectral flux separate from
robustZ, NMS z>=6), and M06 B03 event support. The caller supplies
ALREADY detected *same canonical event list* and separately derived
per-event D07 contrast, D08 crest, D09 positive rise time. This code
does not claim to implement the detector or assemble synthetic event
contexts. Edge events missing context remain present in denominator
and each descriptor has its own native support valid/total.

M06 reference-grade ≥5s active, at least 8 canonical detector events
and at least 8 valid descriptor events. Raw 10/50/90 Type7 ordered
quantiles for D07/D08; D09 raw positive-ms quantiles followed by
LOG2_POSITIVE reference 1ms. No added transient density mass and no
treating attack rise feature as Transient Shaper knob.
