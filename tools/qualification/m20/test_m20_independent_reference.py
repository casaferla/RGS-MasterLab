"""M20 independent Python Fraction/Decimal oracle, NOT official Phase-6 Gate G."""
from fractions import Fraction
from decimal import Decimal, getcontext
getcontext().prec=70
def rne(x):
    q,r=divmod(x.numerator,x.denominator)
    return q+(2*r>x.denominator or (2*r==x.denominator and q%2==1))
def grid(j,fs): return rne(Fraction(j*fs,10))
def type7(v,p):
    v=sorted(v)
    h=Fraction(len(v)-1)*p
    k=h.numerator//h.denominator
    return v[k] if k+1==len(v) else v[k]+(h-k)*(v[k+1]-v[k])
for fs,w,fft,hop in [(44100,3763,4096,941),(48000,4096,4096,1024),
                      (88200,7526,8192,1882),(96000,8192,8192,2048),
                      (176400,15053,16384,3763),(192000,16384,16384,4096)]:
    assert rne(Fraction(4096*fs,48000))==w
    assert rne(Fraction(w,4))==hop
    assert fft==(1<<(w-1).bit_length())
    nw=rne(Fraction(2*fs,5))
    for n in (nw-1,nw,nw+1,fs):
        count=sum(1 for j in range(16) if grid(j,fs)+nw<=n)
        if n<nw:assert count==0
        elif n==nw:assert count==1
assert [grid(j,44105) for j in (0,1,3,7)]==[0,4410,13232,30874]
assert type7([-1,1],Fraction(1,10))==Fraction(-4,5)
assert type7([-1,1],Fraction(9,10))==Fraction(4,5)
assert type7([-1,0,0,1],Fraction(1,10))==Fraction(-7,10)
# Exact rational integration of DC and Nyquist half-cells, not Parseval full-bin weights.
psd=list(map(Fraction,[1,2,3,4,5]))
def integral(a,b):
    return sum((max(Fraction(0),
        min(Fraction(4),Fraction(2*k+1,2),b)-
        max(Fraction(0),Fraction(2*k-1,2),a))*p
        for k,p in enumerate(psd)),Fraction(0))
assert integral(Fraction(0),Fraction(4))==12
knots=[Fraction(0),Fraction(1,2),Fraction(5,4),Fraction(11,4),Fraction(4)]
assert sum((integral(a,b) for a,b in zip(knots,knots[1:])),Fraction(0))==12
m=(Decimal(1)+Decimal("0.01"))/2
assert abs(10*m.log10()-Decimal("-2.967086218813"))<Decimal("1e-9")
def width_sq(l,r):
    em=sum((a+b)**2 for a,b in zip(l,r))
    es=sum((a-b)**2 for a,b in zip(l,r))
    return Fraction(es,em) if em else None
assert width_sq([1],[1])==0
assert width_sq([1],[0])==1
assert width_sq([3],[-1])==4
assert width_sq([1],[-1]) is None
assert len(set(range(0,8))|set(range(5,24)))==24
print("M20_PYTHON_INDEPENDENT_ORACLE_PASS_NOT_GATE_G")
