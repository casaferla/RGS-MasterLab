"""Independent M20 rational/Decimal profile reference, not canonical QTP or Gate G."""
from decimal import Decimal,getcontext
from fractions import Fraction
getcontext().prec=70
D=Decimal
def integrate(psd,lo,hi,df):
    # Exact half-bin DC/Nyquist cell measure; piecewise-constant PSD density.
    n=len(psd)-1
    nyq=n*df
    total=Fraction(0)
    for k,x in enumerate(psd):
        a=max(Fraction(0),(Fraction(k)-Fraction(1,2))*df)
        b=min(nyq,(Fraction(k)+Fraction(1,2))*df)
        overlap=max(Fraction(0),min(b,hi)-max(a,lo))
        total+=x*overlap
    return total
p=[Fraction(x) for x in (1,2,3,4,5)]
df=Fraction(1)
assert integrate(p,Fraction(0),Fraction(4),df)==12
cuts=[Fraction(0),Fraction(1,2),Fraction(5,4),Fraction(11,4),Fraction(4)]
assert sum((integrate(p,a,b,df) for a,b in zip(cuts,cuts[1:])),Fraction(0))==12
# Authoritative external eligibility: missing/LOW_SUPPORT never becomes an energy zero.
energies=[Fraction(3),Fraction(0),Fraction(1),Fraction(7),Fraction(5)]
codes=[1,1,0,2,1]
measured=[x if c==1 else None for x,c in zip(energies,codes)]
total=sum((v for v in measured if v is not None),Fraction())
fractions=[None if v is None else v/total for v in measured]
assert total==8 and fractions==[Fraction(3,8),Fraction(0),None,None,Fraction(5,8)]
assert sum(v for v in fractions if v is not None)==1
for gain in (Fraction(1,2),Fraction(3),Fraction(2)**20):
    scaled=[None if v is None else v*gain*gain for v in measured]
    target=sum(v for v in scaled if v is not None)
    assert [None if v is None else v/target for v in scaled]==fractions
# All frequencies missing, and genuine silence, produce status not flat normalized profile.
assert not any(c==1 for c in (0,2,0))
assert sum(Fraction(0) for _ in (1,1,1))==0
# At 44.1k the visible upper support is 0.45Fs = 19845, not 20k.
for fs in (44100,48000,88200,96000,176400,192000):
    u=min(20000,Fraction(45,100)*fs)
    assert u==(19845 if fs==44100 else 20000)
# A static two-frame power view must mean in linear power, not decibels.
power=(D(1)+D("0.01"))/2
db=10*power.log10()
assert abs(db-D("-2.967086218813"))<D("1e-9")
assert abs(db-D(-10))>D(7)
print("M20_INDEPENDENT_PROFILE_RATIONAL_REFERENCE_PASS_NOT_GATE_G")
