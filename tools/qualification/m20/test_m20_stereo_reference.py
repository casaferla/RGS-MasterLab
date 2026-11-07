"""Independent exact Fraction temporal/stereo oracles; M20 private candidate, not Gate G."""
from fractions import Fraction as F
def tie_even(a,b):
    q,r=divmod(a,b)
    return q+(2*r>b or (2*r==b and q%2))
def grid(j,fs): return tie_even(j*fs,10)
def width_squared(samples):
    mids=sum((l+r)**2 for l,r in samples)
    sides=sum((l-r)**2 for l,r in samples)
    return None if mids==0 else F(sides,mids)
def union(intervals,lo,hi):
    # Synthetic authoritatively retained intervals; canonical gate not reconstructed.
    clipped=sorted((max(a,lo),min(b,hi)) for a,b in intervals if min(b,hi)>max(a,lo))
    out=[]
    for a,b in clipped:
        if not out or a>out[-1][1]:out.append([a,b])
        else:out[-1][1]=max(b,out[-1][1])
    return [tuple(x) for x in out]
assert width_squared([(F(3),F(-1))])==4
assert width_squared([(F(1),F(0))])==1
assert width_squared([(F(1),F(1))])==0
assert width_squared([(F(1),F(-1))]) is None
assert width_squared([(F(3,2),F(-1,2))])==4
scope=(12345,13345)
blocks=[(12345,12445),(12385,12545),(12535,12645),(0,12345),(20000,20100)]
selected=union(blocks,*scope)
assert selected==[(12345,12645)]
assert sum(b-a for a,b in selected)==300
pairs=[(F(1),F(1)),(F(1),F(-1)),(F(1),F(3))]
assert width_squared(pairs)==F(2,5)
def quantile_type7(data,p):
    data=sorted(data)
    if len(data)==1:return data[0]
    h=(len(data)-1)*p
    k=h.numerator//h.denominator
    return data[-1] if k+1==len(data) else data[k]+(h-k)*(data[k+1]-data[k])
for rate in (44100,48000,88200,96000,176400,192000):
    window=tie_even(2*rate,5)
    assert window>0
    starts=[grid(j,rate) for j in range(8)]
    for n in [window-1,window,window+grid(1,rate)-1,window+grid(1,rate)]:
        expected=sum(s+window<=n for s in starts)
        assert expected in (0,1,2)
        if n==window:assert expected==1
        if n==window+grid(1,rate)-1:assert expected==1
        if n==window+grid(1,rate):assert expected==2
assert [grid(j,44105) for j in (0,1,3,7)]==[0,4410,13232,30874]
assert [quantile_type7([-1,1],p) for p in (F(1,10),F(1,2),F(9,10))]==[F(-4,5),F(0),F(4,5)]
assert [quantile_type7([-1,-F(1,2),0,F(1,2),1],p)
        for p in (F(1,10),F(1,2),F(9,10))]==[F(-4,5),F(0),F(4,5)]
print("M20_PYTHON_STEREO_EXACT_ORACLE_PASS_NOT_GATE_G")
