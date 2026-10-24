"""Intersect exact fixed-gain intervals from actual output energy coefficients.
Input: reverb-hall-original-fixtures log (stimulus0 white, stimulus1 sine).
The quadratic is measured in stereo mean energy, with g relative to fixed1.4.
Negative polarity is diagnostic only; it is not an adopted signal-path change.
"""
import math,re,sys
reference=float(sys.argv[2]) if len(sys.argv)>2 else 1.4
rows=[]
for line in open(sys.argv[1]):
    row={key:float(value) for key,value in re.findall(r'(wetRatio|mixA|mixB|mixC)([-+\de.]+)',line)}
    row['label']=line.split(' wetRatio')[0];row['stimulus']=int(re.search(r'stimulus(\d+)',line)[1]);rows.append(row)
def intersect(left,right):
    return [(max(a,c),min(b,d)) for a,b in left for c,d in right if max(a,c)<=min(b,d)]
def quadratic_intervals(a,b,c,lo,hi):
    points=[0.,1e6]
    for bound in (lo,hi):
        disc=b*b-4*a*(c-bound)
        if disc>=0:
            for root in ((-b-math.sqrt(disc))/(2*a),(-b+math.sqrt(disc))/(2*a)):
                if 0<root<1e6:points.append(root)
    points=sorted(set(points));return [(x,y) for x,y in zip(points,points[1:]) if lo<=a*((x+y)/2)**2+b*(x+y)/2+c<=hi]
for scope,subset in [('default_seed_phase',[r for r in rows if 'seed1 preroll0.00' in r['label']]),('expanded_seed_phase',rows)]:
    print('scope',scope,'cases',len(subset))
    for polarity in (1,-1):
        valid=[(0.,1e6)];wet=[(0.,1e6)];mixed=[(0.,1e6)]
        for row in subset:
            if row['stimulus']==0:
                limits=[(math.sqrt(10**(-3/10)/row['wetRatio']),math.sqrt(10**(3/10)/row['wetRatio']))];wet=intersect(wet,limits)
            else:
                limits=quadratic_intervals(row['mixA'],polarity*row['mixB'],row['mixC'],10**(-1.5/10),10**(1.5/10));mixed=intersect(mixed,limits)
            previous=valid;valid=intersect(valid,limits)
            if previous and not valid:print('polarity',polarity,'intersection_first_empty_at',row['label'],'prior_relative',previous,'new_relative',limits)
        print('polarity',polarity,'white_relative',wet,'mixed_relative',mixed,'combined_relative',valid,'combined_absolute',[(reference*a,reference*b) for a,b in valid])
