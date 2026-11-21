#!/usr/bin/env python3
"""Independent Python integer temporal-grid oracle; no C++ imports.

M19 accepted Phase-6 scope grid: 400 ms windows, 100 ms relative starts,
round-to-nearest-even of exact integer ratios. Tests diagnostic 44,105 Hz
only as geometry; does NOT certify an additional K-weighting bank.
"""
def rne(num: int, den: int) -> int:
    q, r = divmod(num, den)
    return q + int(2*r > den or (2*r == den and q % 2 == 1))

def support(fs: int, s0: int, length: int):
    window = rne(2*fs, 5)
    starts = []
    index = 0
    while True:
        begin = rne(index*fs, 10)
        if begin + window > length:
            break
        starts.append(s0+begin)
        index += 1
    return starts

def run():
    rates=(44100,48000,88200,96000,176400,192000)
    durations=(17640,19200,35280,38400,70560,76800)
    hops=(4410,4800,8820,9600,17640,19200)
    for fs,b,h in zip(rates,durations,hops):
        assert rne(2*fs,5)==b
        for origin in (0,12345,96000):
            for scope_length in (0,b-1,b,b+1,b+h-1,b+h,
                                 b+h+1,b+2*h-1,b+2*h,b+3*h+17):
                actual=support(fs,origin,scope_length)
                n=1+((scope_length-b)//h) if scope_length>=b else 0
                assert len(actual)==n
                assert actual==[origin+i*h for i in range(n)]
                assert all(origin<=x and x+b<=origin+scope_length
                           for x in actual)
    nonintegral=(0,4410,8821,13232,17642,22052,
                 26463,30874,35284,39694,44105)
    assert [rne(j*44105,10) for j in range(11)]==list(nonintegral)
    assert rne(2*44105,5)==17642
    # The 7th (=index 6) nominal 100-ms block only becomes complete
    # at 26463+17642; a rounded repeated +4410 would admit it early.
    assert len(support(44105,12345,26462+17642))==6
    assert len(support(44105,12345,26463+17642))==7
    assert [rne(5*n,10) for n in (1,3,5,7,9)]==[0,2,2,4,4]
    print("M19_INDEPENDENT_TEMPORAL_GRID_PASS_NOT_FORMAL_GATE_G")

if __name__=="__main__":
    run()
