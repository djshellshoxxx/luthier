from pathlib import Path
import csv,json,hashlib
import numpy as np
from scipy.signal import lfilter
root=Path(__file__).parent
stages=['raw','preCab','mic','output']
summary={}; bandrows=[];replays=[]; hashes=[]
for variant in ['original','cabinet']:
 rows=list(csv.DictReader(open(root/variant/'metrics.csv')))
 summary[variant]={'cases':len(rows),'rawMinimumDb':min(float(x['rawDrop']) for x in rows),'failures35dB':[{'rate':int(float(x['rate'])),'type':int(x['type']),'name':x['name'],'outputDrop':float(x['outputDrop'])} for x in rows if float(x['outputDrop'])<35], 'eventOffsetMismatches':sum(x['actualOff']!=x['scheduledOff'] for x in rows),'nonfiniteSamples':sum(int(x['nonfiniteRaw'])+int(x['nonfiniteAmp'])+int(x['nonfiniteMic']) for x in rows),'nonfiniteOutputCases':sum(x['finiteOutput']!='1' for x in rows),'missingBlocks':sum(int(x['missingRaw'])+int(x['missingMic']) for x in rows),'queueDrops':sum(int(x['activityDrops']) for x in rows),'hostEngineLatencyMismatches':sum(x['hostLatency']!=x['engineLatency'] for x in rows)}
 for r in rows:
  sr=int(float(r['rate']));t=int(r['type']);path=root/variant/f'r{sr}-t{t}.f32';data=np.fromfile(path,dtype='<f4').reshape(-1,4).astype(float)
  hashes.append({'variant':variant,'rate':sr,'type':t,'frames':len(data),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
  assert np.isfinite(data).all()
  n=int(.03*sr);pre=int(.5*sr)-n;post=int(.5*sr)+int(.25*sr);f0=440*2**((int(r['note'])-69)/12)
  # Fixed bands and harmonic-centred probes, specified before measurement.
  bands=[('0_35',0,35),('35_65',35,65),('65_125',65,125),('125_250',125,250),('250_1000',250,1000),('1000_5000',1000,5000),('5000_nyquist',5000,sr/2+1),('fundamental',.75*f0,1.25*f0),('harmonic2',1.75*f0,2.25*f0),('harmonic3',2.75*f0,3.25*f0)]
  for ch,stage in enumerate(stages):
   for window,start in [('before',pre),('after',post)]:
    x=data[start:start+n,ch];N=n*16;freq=np.fft.rfftfreq(N,1/sr);power=abs(np.fft.rfft(x,n=N))**2/(n*N);power[1:-1]*=2
    total=np.mean(x*x);assert abs(power.sum()-total)<1e-12
    for name,lo,hi in bands:
     p=power[(freq>=lo)&(freq<hi)].sum()
     bandrows.append({'variant':variant,'rate':sr,'type':t,'stage':stage,'window':window,'band':name,'loHz':lo,'hiHz':hi,'bandRms':np.sqrt(p),'fractionOfWindowEnergy':p/max(total,1e-300),'windowMean':x.mean(),'windowRms':np.sqrt(total)})
  if t==19:
   # Known downstream linear components, evaluated in the ORIGINAL windows.
   # float boundary matches cabinet output. This is not a new acceptance signal.
   mic=data[:,2];cab=lfilter([1,-1],[1,-(1-2*np.pi*10/sr)],mic).astype(np.float32).astype(float)
   master=lfilter([1,-1],[1,-(1-2*np.pi*5/sr)],cab)
   delay=int(sr*.0015);delayed=np.r_[np.zeros(delay),master[:-delay]]
   row={'variant':variant,'rate':sr,'masterLookaheadSamples':delay}
   for name,x in [('mic',mic),('cabDc10',cab),('masterDc5',master),('knownLinearPathWithLookahead',delayed),('actualOutput',data[:,3])]:
    a=np.sqrt(np.mean(x[pre:pre+n]**2));b=np.sqrt(np.mean(x[post:post+n]**2));row[name+'DropDb']=20*np.log10(a/b)
   row['linearReplayResidualDropDb']=row['actualOutputDropDb']-row['knownLinearPathWithLookaheadDropDb'];replays.append(row)
for filename,rows in [('bands.csv',bandrows),('linear-downstream-replay.csv',replays)]:
 with open(root/filename,'w') as f:
  w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
(root/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');(root/'audio-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')
print(json.dumps(summary,indent=2))
for r in replays:
 if r['rate']==48000:print(r)
