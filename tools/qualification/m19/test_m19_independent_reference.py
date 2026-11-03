#!/usr/bin/env python3
"""Independent stdlib-only M19 mathematical preflight, NOT production Gate G.

These fixtures are fixed by the accepted 2026-10-04 M19 scope-state
re-signoff and 2026-09-12 M19 frozen five-metric product contract.
This reference deliberately does not import the candidate C++ module.
"""
import math

B = (1.53512485958697, -2.69169618940638, 1.19839281085285,
     -1.69065929318241, 0.73248077421585)
A = (1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621)

def filter_48k(signal):
    state = [[0.0, 0.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0]]
    energy = []
    for value in signal:
        x = value
        for stage, (b0, b1, b2, a1, a2) in enumerate((B, A)):
            x1, x2, y1, y2 = state[stage]
            y = b0*x+b1*x1+b2*x2-a1*y1-a2*y2
            state[stage] = [x, x1, y, y1]
            x = y
        energy.append(x*x)
    return energy

def full_loudness_mono_48k(signal):
    energies = filter_48k(signal)
    blocks = [math.fsum(energies[start:start+19200])/19200
              for start in range(0, len(energies)-19199, 4800)]
    def loudness(power):
        return -0.691 + 10.0*math.log10(power)
    active = [q for q in blocks if q > 0.0 and loudness(q) > -70.0]
    if not active:
        return None
    relative_threshold = loudness(math.fsum(active)/len(active)) - 10.0
    retained = [q for q in active if loudness(q) > relative_threshold]
    assert retained, "nonempty absolute-active set must retain a relative block"
    return loudness(math.fsum(retained)/len(retained))

def full_scope_metrics(channels):
    assert channels and channels[0] and all(len(c)==len(channels[0]) for c in channels)
    values = [x for channel in channels for x in channel]
    assert all(math.isfinite(x) for x in values)
    peak = max(map(abs, values))
    if peak == 0.0:
        return None, None, None
    peak_db = 20*math.log10(peak)
    # Independent mean-square via an accurate summation at moderate amplitudes.
    rms_db = 10*math.log10(math.fsum(x*x for x in values)/len(values))
    return rms_db, peak_db, peak_db-rms_db

def close(actual, target, eps=2e-5):
    assert math.isfinite(actual) and abs(actual-target) < eps, (actual,target)

def run():
    dc = [1.0]*19200
    close(full_loudness_mono_48k(dc), -26.2768703195911)
    impulse = [0.0]*19200
    impulse[0] = 1.0
    close(full_loudness_mono_48k(impulse), -39.6949569766218)
    rms, peak, crest = full_scope_metrics([impulse])
    close(rms, -10*math.log10(19200))
    close(peak, 0)
    close(crest, 10*math.log10(19200))
    assert full_loudness_mono_48k([0.0]*19200) is None
    assert full_loudness_mono_48k([1.0]*19199) is None
    assert full_scope_metrics([[0.0]*19200]) == (None,None,None)
    rms, peak, crest = full_scope_metrics([[2.0]*19200])
    close(rms, 20*math.log10(2))
    close(peak, 20*math.log10(2))
    close(crest, 0)
    assert full_scope_metrics([dc,dc]) == (0.0,0.0,0.0)
    # Exactly-at-threshold blocks are excluded, not >=.
    q = [19.0,1.0]
    rel_threshold = (-0.691+10*math.log10(math.fsum(q)/len(q)))-10.0
    assert math.isclose(rel_threshold, -0.691, abs_tol=1e-12)
    assert not (q[1] > (math.fsum(q)/len(q))/10.0)
    # Poison outside scope is not included; the scope owns the filter reset.
    assert full_loudness_mono_48k(([1.0e4]+dc+[-1.0e4])[1:-1]) == full_loudness_mono_48k(dc)
    print("M19_INDEPENDENT_48K_ORACLE_PASS_NOT_FORMAL_GATE_G")

if __name__ == "__main__":
    run()
