"""M16 C2 test-only fake sink and independently consumed frame oracle.

Does not use the production playback backend, QAudioSink, M16 FIR, EBU WAV,
or quality-gate statuses. Tests a bounded event ring and a proposed sparse
scheduled-run / processed-watermark observation contract only.
"""
from __future__ import annotations

import collections
import dataclasses
import unittest


@dataclasses.dataclass(frozen=True)
class Sample:
    owner: str | None
    traversal: int
    source_frame: int
    peak: float = 0.0  # Synthetic test peak: never an M16 FIR measurement.
    phase: str = "NEW"
    mapping: str = "EXACT"


@dataclasses.dataclass(frozen=True)
class Run:
    begin: int
    frames: tuple[Sample, ...]

    @property
    def end(self):
        return self.begin + len(self.frames)


@dataclasses.dataclass(frozen=True)
class Event:
    generation: int
    sequence: int
    begin: int
    end: int
    kind: str
    owner: str | None
    traversal: int | None
    peak: float | None
    reason: str | None


class FakeSink:
    """Fake device tracks consumed frames independently of candidate schedule."""

    def __init__(self, capacity=1_000_000, maximum_write=1_000_000):
        self.capacity = capacity
        self.maximum_write = maximum_write
        self.generated = collections.deque()
        self.queue = collections.deque()
        self.consumed = []
        self.paused = False

    def generate(self, frames):
        self.generated.extend(frames)

    def enqueue(self, requested):
        n = min(requested, self.maximum_write,
                self.capacity - len(self.queue), len(self.generated))
        for _ in range(n):
            self.queue.append(self.generated.popleft())
        return n

    def process(self, requested):
        if self.paused:
            return 0
        n = min(requested, len(self.queue))
        for _ in range(n):
            self.consumed.append(self.queue.popleft())
        return n

    @property
    def processed_frames(self):
        return len(self.consumed)

    def reset(self):
        self.generated.clear()
        self.queue.clear()
        self.consumed.clear()  # Device processed clock can restart at zero.
        self.paused = False


class SparseCandidate:
    """Candidate model, not production code; only delivered EVENT ring is bounded.

    Schedule and source frames are deliberately retained in this tiny test
    fixture for independent oracle comparisons. Their retention is NOT proof
    of a bounded production owner/PCM resource model.
    """

    def __init__(self, retained_events=128):
        assert retained_events > 0
        self.capacity = retained_events
        self.generation = 1
        self.runs = []
        self.next_generated = 0
        self.released = 0
        self.next_sequence = 1
        self.events = collections.deque(maxlen=retained_events)
        self.ever_had_gap = False
        self.overflowed = False

    def schedule(self, frames):
        frames = tuple(frames)
        if frames:
            self.runs.append(Run(self.next_generated, frames))
            self.next_generated += len(frames)

    def reset(self):
        self.generation += 1
        self.runs.clear()
        self.next_generated = 0
        self.released = 0
        self.next_sequence = 1
        self.events.clear()
        self.ever_had_gap = False
        self.overflowed = False

    @staticmethod
    def identity(sample):
        if sample.owner is None:
            return ("GAP", None, None, "MISSING_OWNER")
        if sample.phase == "TRANSITION":
            return ("GAP", None, None, "MIXED_CROSSFADE")
        if sample.mapping != "EXACT":
            return ("GAP", None, None, "UNPROVEN_SRC_MAPPING")
        return ("VALID", sample.owner, sample.traversal, None)

    def _append(self, begin, end, identity, peaks):
        kind, owner, traversal, reason = identity
        if kind == "GAP":
            self.ever_had_gap = True
        if len(self.events) == self.capacity:
            self.overflowed = True
        self.events.append(Event(
            self.generation, self.next_sequence, begin, end,
            kind, owner, traversal,
            max(peaks) if kind == "VALID" else None, reason))
        self.next_sequence += 1

    def observe(self, processed_watermark):
        if processed_watermark < self.released:
            raise ValueError("CLOCK_REGRESSION: new generation required")
        if processed_watermark > self.next_generated:
            raise ValueError("UNBACKED_PROCESSED_WATERMARK")
        current = self.released
        for run in self.runs:
            lo = max(current, run.begin)
            hi = min(processed_watermark, run.end)
            if lo >= hi:
                continue
            sub = run.frames[lo - run.begin:hi - run.begin]
            segment_begin = lo
            segment_key = self.identity(sub[0])
            peaks = []
            for offset, sample in enumerate(sub):
                key = self.identity(sample)
                if key != segment_key:
                    self._append(segment_begin, lo + offset, segment_key, peaks)
                    segment_begin = lo + offset
                    segment_key = key
                    peaks = []
                peaks.append(sample.peak)
            self._append(segment_begin, hi, segment_key, peaks)
            current = hi
        if current != processed_watermark:
            self._append(current, processed_watermark,
                         ("GAP", None, None, "MISSING_SCHEDULED_INTERVAL"), [])
        self.released = processed_watermark

    def query(self, generation, sequence=0, limit=128):
        assert limit > 0
        if generation != self.generation:
            return {"status": "STALE_GENERATION", "complete": False, "events": []}
        first = self.events[0].sequence if self.events else self.next_sequence
        if sequence < first - 1:
            return {"status": "EXPIRED_TOKEN_GAP", "complete": False, "events": []}
        pending = [e for e in self.events if e.sequence > sequence]
        selected = pending[:limit]
        return {"status": "OK", "events": selected,
                "complete": not (self.ever_had_gap or self.overflowed or len(pending) > limit),
                "has_more": len(pending) > limit,
                "next_sequence": selected[-1].sequence if selected else sequence}


def oracle_expansion(samples):
    """Per-frame oracle from the sink's actual independent consumed history."""
    result = []
    for i, sample in enumerate(samples):
        if sample.owner is None:
            result.append((i, "GAP", None, None, "MISSING_OWNER"))
        elif sample.phase == "TRANSITION":
            result.append((i, "GAP", None, None, "MIXED_CROSSFADE"))
        elif sample.mapping != "EXACT":
            result.append((i, "GAP", None, None, "UNPROVEN_SRC_MAPPING"))
        else:
            result.append((i, "VALID", sample.owner, sample.traversal, None))
    return result


def expand_events(events):
    return [(i, e.kind, e.owner, e.traversal, e.reason)
            for e in events for i in range(e.begin, e.end)]


class C2TestOnlyContract(unittest.TestCase):
    def setUp(self):
        self.sink = FakeSink()
        self.candidate = SparseCandidate()

    def stage(self, frames):
        self.sink.generate(frames)
        self.candidate.schedule(frames)

    def drain(self):
        while self.sink.generated:
            self.assertGreater(self.sink.enqueue(1_000_000), 0)
        self.sink.process(1_000_000)
        self.candidate.observe(self.sink.processed_frames)

    def verify_oracle(self):
        self.assertEqual(expand_events(self.candidate.events),
                         oracle_expansion(self.sink.consumed))

    def test_T04_B02_T05_T17_short_spike_and_no_future(self):
        frames = [Sample("A", 1, i, 1.25 if i == 90 else 0.10)
                  for i in range(160)]
        self.stage(frames)
        self.sink.enqueue(160)
        self.sink.process(40)
        self.candidate.observe(self.sink.processed_frames)
        self.assertLess(max(e.peak for e in self.candidate.events), 1)
        self.sink.process(120)  # No UI poll over the peak.
        self.candidate.observe(self.sink.processed_frames)
        self.assertGreater(max(e.peak for e in self.candidate.events), 1)
        self.verify_oracle()

    def test_B01_B03_T08_B25_multiple_wraps(self):
        for traversal in range(1, 4):
            self.stage([Sample("A", traversal, i) for i in range(5)])
        self.drain()
        self.verify_oracle()
        self.assertEqual({e.traversal for e in self.candidate.events}, {1, 2, 3})
        self.assertEqual(self.sink.consumed[0].source_frame,
                         self.sink.consumed[5].source_frame)

    def test_B04_B05_no_generated_or_queued_future(self):
        self.stage([Sample("A", 1, i) for i in range(8)])
        self.candidate.observe(0)
        self.assertFalse(self.candidate.events)
        self.sink.enqueue(8)
        self.candidate.observe(0)
        self.assertFalse(self.candidate.events)
        self.sink.process(4)
        self.candidate.observe(4)
        self.assertEqual(self.candidate.released, 4)
        self.verify_oracle()

    def test_B07_B08_B09_T11_T12_handoff_and_mixed(self):
        self.stage([Sample("OLD", 1, i, phase="OLD") for i in range(4)])
        self.stage([Sample("OLD", 1, i, phase="TRANSITION") for i in range(4, 7)])
        self.stage([Sample("NEW", 1, i) for i in range(7, 11)])
        self.sink.enqueue(11)
        self.sink.process(4)
        self.candidate.observe(4)
        self.assertEqual({e.owner for e in self.candidate.events}, {"OLD"})
        self.sink.process(7)
        self.candidate.observe(11)
        self.verify_oracle()
        self.assertIn("MIXED_CROSSFADE", {e.reason for e in self.candidate.events})
        self.assertFalse(self.candidate.query(1)["complete"])

    def test_B10_T15_partial_writes(self):
        self.sink = FakeSink(capacity=3, maximum_write=2)
        self.stage([Sample("A", 1, i) for i in range(9)])
        self.assertEqual(self.sink.enqueue(9), 2)
        self.assertEqual(self.sink.enqueue(9), 1)
        self.candidate.observe(0)
        self.assertFalse(self.candidate.events)
        self.assertEqual(self.sink.process(2), 2)
        self.candidate.observe(2)
        self.verify_oracle()

    def test_B11_T09_pause_does_not_decay(self):
        self.stage([Sample("A", 1, i) for i in range(4)])
        self.sink.enqueue(4)
        self.sink.process(2)
        self.candidate.observe(2)
        self.sink.paused = True
        self.assertEqual(self.sink.process(2), 0)
        self.candidate.observe(self.sink.processed_frames)
        self.assertEqual(self.candidate.released, 2)
        self.sink.paused = False
        self.sink.process(2)
        self.candidate.observe(4)
        self.verify_oracle()

    def test_B12_T07_seek_resets_generation(self):
        self.stage([Sample("A", 1, i) for i in range(3)])
        self.drain()
        token = self.candidate.query(1)["next_sequence"]
        self.sink.reset()
        self.candidate.reset()
        self.stage([Sample("A", 2, 80 + i) for i in range(3)])
        self.drain()
        self.assertEqual(self.candidate.query(1, token)["status"], "STALE_GENERATION")
        self.verify_oracle()
        self.assertEqual(self.candidate.generation, 2)

    def test_B16_B17_T14_unproven_src_is_gap(self):
        self.stage([Sample("A", 1, i, mapping="SRC_CURSOR_ONLY") for i in range(13)])
        self.drain()
        self.verify_oracle()
        self.assertFalse(self.candidate.query(1)["complete"])
        self.assertEqual({e.reason for e in self.candidate.events},
                         {"UNPROVEN_SRC_MAPPING"})

    def test_B19_T16_overflow_expires_token(self):
        self.candidate = SparseCandidate(retained_events=2)
        for i in range(8):
            self.stage([Sample("A", i, i)])
        self.drain()
        self.assertLessEqual(len(self.candidate.events), 2)
        self.assertEqual(self.candidate.query(1, 0)["status"], "EXPIRED_TOKEN_GAP")

    def test_B21_B15_T18_missing_owner_is_gap(self):
        self.stage([Sample(None, 1, i) for i in range(3)])
        self.drain()
        self.verify_oracle()
        self.assertEqual(self.candidate.events[0].reason, "MISSING_OWNER")
        self.assertFalse(self.candidate.query(1)["complete"])

    def test_B22_T21_observer_on_off_payload_parity(self):
        samples = [Sample("A", 1, i, 0.5) for i in range(20)]
        a, b = FakeSink(), FakeSink()
        for sink in (a, b):
            sink.generate(samples)
            sink.enqueue(20)
        ledger = SparseCandidate()
        ledger.schedule(samples)
        for _ in range(4):
            a.process(5)
            ledger.observe(a.processed_frames)
            b.process(5)
        self.assertEqual(a.consumed, b.consumed)
        self.assertEqual(list(a.queue), list(b.queue))
        self.assertEqual(a.processed_frames, b.processed_frames)

    def test_T19_linear_over_zero_threshold(self):
        below, above = 1.0 - 2.0 ** -52, 1.0 + 2.0 ** -52
        self.stage([Sample("A", 1, 0, below), Sample("A", 1, 1, 1.0),
                    Sample("A", 1, 2, above)])
        self.drain()
        self.assertEqual(max(e.peak for e in self.candidate.events), above)
        self.assertTrue(any(e.peak > 1 for e in self.candidate.events))

    def test_T24_B25_independent_oracle_and_pagination(self):
        self.stage([Sample("A", 1, 0), Sample("A", 1, 1),
                    Sample("B", 1, 2), Sample(None, 1, 3)])
        self.drain()
        self.verify_oracle()
        page = self.candidate.query(1, limit=1)
        self.assertTrue(page["has_more"])
        self.assertFalse(page["complete"])

    def test_fail_closed_on_clock_regression_and_future(self):
        self.stage([Sample("A", 1, i) for i in range(4)])
        self.sink.enqueue(4)
        self.sink.process(3)
        self.candidate.observe(3)
        with self.assertRaisesRegex(ValueError, "CLOCK_REGRESSION"):
            self.candidate.observe(2)
        with self.assertRaisesRegex(ValueError, "UNBACKED"):
            self.candidate.observe(5)


if __name__ == "__main__":
    unittest.main(verbosity=2)
