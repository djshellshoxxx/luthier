from pathlib import Path
import csv,json,hashlib
import numpy as np
b=Path(__file__).resolve().parent;g=b/'generation3'
def rows(p):return list(csv.DictReader(p.open()))
a=rows(g/'baseline-data/metrics.csv');c=rows(g/'candidate-data/metrics.csv');assert len(a)==len(c)==125
mismatch=[];frames=0
for x,y in zip(a,c):
 key=(x['rate'],x['type']);assert key==(y['rate'],y['type'])
 n='r'+str(int(float(x['rate'])))+'-t'+x['type']+'.f32';u=np.fromfile(g/'baseline-data'/n,dtype=np.float32).reshape(-1,4);v=np.fromfile(g/'candidate-data'/n,dtype=np.float32).reshape(-1,4);off=int(x['scheduledOff']);frames+=off
 if not np.array_equal(u[:off].view(np.uint32),v[:off].view(np.uint32)):mismatch.append(key)
ia=rows(g/'baseline-data/identity.csv');ic=rows(g/'candidate-data/identity.csv');assert len(ia)==len(ic)==120
preMismatch=[];heldMismatch=[]
for x,y in zip(ia,ic):
 key=tuple(x[k] for k in ['rate','index','fret','scenario']);assert key==tuple(y[k] for k in ['rate','index','fret','scenario'])
 if x['preHash']!=y['preHash']:preMismatch.append(key)
 if x['scenario'] in ['0','1'] and x['fullHash']!=y['fullHash']:heldMismatch.append(key)
g4=b/'generation4';pa=rows(g4/'baseline-data/pedal-identity.csv');pc=rows(g4/'candidate-data/pedal-identity.csv');assert len(pa)==len(pc)==30
pedalMismatch=[]
for x,y in zip(pa,pc):
 key=tuple(x[k] for k in ['rate','type','cc']);assert key==tuple(y[k] for k in ['rate','type','cc'])
 if any(x[k]!=y[k] for k in ['rawPrefixHash','ampPrefixHash','micPrefixHash','outputPrefixHash']):pedalMismatch.append(key)
 assert int(x['heldOpenCount'])>0 and int(x['releasedCount'])>0
 assert int(y['heldOpenCount'])>0 and int(y['releasedCount'])>0
hashErrors=[]
for gen in [g,g4]:
 for p,h in json.loads((gen/'pre-run-sha256.json').read_text()).items():
  if hashlib.sha256((gen/p).read_bytes()).hexdigest()!=h:hashErrors.append(str(gen/p))
result={'source':'280e182a8f033457f3d84384b9917a024b1d1ed9','hostPrefixComparison':'uint32 bit patterns, including signed zero','cases':125,'rates':[44100,48000,88200,96000,192000],'candidateMinOutputDropDb':min(float(y['outputDrop']) for y in c),'candidateMinRawDropDb':min(float(y['rawDrop']) for y in c),'baselineBelow35':[(x['rate'],x['name'],x['outputDrop']) for x in a if float(x['outputDrop'])<35],'candidateBelow35':[(x['rate'],x['name'],x['outputDrop']) for x in c if float(x['outputDrop'])<35],'preReleaseComparedFrames':frames,'preReleaseComparedValues':frames*4,'preReleaseMismatches':mismatch,'directStringIdentityCases':120,'directStringPreMismatches':preMismatch,'fullHeldLetRingCases':60,'fullHeldLetRingMismatches':heldMismatch,'actualHostPedalCases':30,'actualHostPedalPrefixMismatches':pedalMismatch,'frozenInputHashErrors':hashErrors,'pstyle':[{k:y[k] for k in ['rate','rawDrop','outputDrop']} for y in c if y['type']=='19']}
assert not any([mismatch,preMismatch,heldMismatch,pedalMismatch,hashErrors]);assert not result['candidateBelow35'];assert result['candidateMinRawDropDb']>40
(b/'final-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
