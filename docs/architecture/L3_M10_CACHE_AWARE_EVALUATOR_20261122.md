# L3 M10 — Exact completed-cache evaluation adapter

Reusable entrypoint for the real Product evaluator. It checks an
authentic exact candidate key and mandatory preflight first. An
existing complete current evaluation returns COMPLETED_CACHE_HIT
without launching rendering or spending one of 800 attempts.
A noncached candidate with no available budget returns
NEEDS_NEW_ATTEMPT, not a fabricated cost. With budget, the callback
executes one actual full render+M05/M06 analysis+hard gates.
Failure consumes one attempted slot and is never cached; only a
current fully complete valid M10 cost is storable.

This does not create a Product worker implementation, M09 canonical
execution key serialization, Qt ownership proof, 27-feature extraction,
actual hard constraint enforcement, persistence or verification.
