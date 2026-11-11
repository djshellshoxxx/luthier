"""Independent100-digit oracle; output columns x0 x1 x2 second divided difference."""
import mpmath as mp
import random
import sys
mp.mp.dps = 100

def f1(x):
    return mp.log(mp.cosh(x))

def f2(x):
    a = abs(x)
    return mp.sign(x) * (a*a/2-a*mp.log(2)+mp.polylog(2,-mp.exp(-2*a))/2+mp.pi**2/24)

def reference(values):
    lo, mid, hi = sorted(mp.mpf(float(x)) for x in values)
    if hi == lo:
        return mp.tanh(hi)
    upper = f1(hi) if hi == mid else (f2(hi)-f2(mid))/(hi-mid)
    lower = f1(lo) if lo == mid else (f2(mid)-f2(lo))/(mid-lo)
    return 2*(upper-lower)/(hi-lo)

cases=[]
for centre in [-1e6,-100,-20,-18,-10,-1.01,-1,-.99,-.5,-.01,0,.01,.5,.99,1,1.01,10,18,20,100,1e6]:
    for d in [1e-14,1e-12,1e-9,1e-6,.001,.004999999,.005,.005000001,.009999999,.01,.010000001,.1,1]:
        cases += [(centre-d,centre,centre+d),(centre,centre,centre+d),(centre,centre+d,centre),(centre-d,centre,centre)]
randomizer=random.Random(20261024)
for i in range(512):
    cases.append(tuple(randomizer.uniform(-100,100) for _ in range(3)))
# Slow ramps and abrupt gain/offset changes are supplied before the static shaper.
previous=older=0
for i in range(256):
    gain=mp.exp(i*.01) if i<128 else (1e6 if i%2 else 1e-6)
    current=float(gain*mp.sin(i*.317)+(0 if i<128 else (-1 if i%2 else 1)))
    cases.append((current,previous,older))
    older,previous=previous,current
with open(sys.argv[1],"w") as target:
    for values in cases:
        target.write(" ".join(format(float(x),".17g") for x in (*values,reference(values)))+"\n")
print(len(cases),"independent high-precision cases")
