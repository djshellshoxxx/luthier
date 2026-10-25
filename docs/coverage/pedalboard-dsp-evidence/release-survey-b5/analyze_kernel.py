from pathlib import Path
import csv
import numpy as np
from scipy.signal import fftconvolve
root=Path(__file__).parent;rows=[]
for v in ['original','cabinet']:
 for sr in [44100,48000,88200,96000,192000]:
  h=np.fromfile(root/v/f'kernel-r{sr}.f32',dtype='<f4').reshape(-1,2)[:,0].astype(float)
  x=np.fromfile(root/v/f'r{sr}-t19.f32',dtype='<f4').reshape(-1,4).astype(float);pred=fftconvolve(x[:,1],h)[:len(x)];mic=x[:,2]
  n=int(.03*sr);before=int(.5*sr)-n;after=int(.5*sr)+int(.25*sr)
  rms=lambda a: np.sqrt(np.mean(a*a));drop=lambda a:20*np.log10(rms(a[before:before+n])/rms(a[after:after+n]))
  # Fourier derivative gives actual kernel group delay at the note frequency;
  # includes the measured convolution latency. No output window is moved.
  f0=440*2**((42-69)/12);ages=np.arange(len(h));phase=np.exp(-2j*np.pi*f0*ages/sr);H=sum(h*phase);dH=sum(-2j*np.pi*ages/sr*h*phase);gd=-np.imag(dH/H)/(2*np.pi)
  e=np.cumsum(h*h);e/=e[-1]
  row={'variant':v,'rate':sr,'firstAbove1eMinus8':int(np.argmax(abs(h)>1e-8)),'peakSample':int(np.argmax(abs(h))),'energy99Sample':int(np.searchsorted(e,.99)),'energy999Sample':int(np.searchsorted(e,.999)),'gainAtNoteDb':20*np.log10(abs(H)),'groupDelayAtNoteMs':gd*1000,'actualMicDropDb':drop(mic),'firReplayMicDropDb':drop(pred),'replayErrorRmsAfter':rms((mic-pred)[after:after+n]),'actualMicRmsAfter':rms(mic[after:after+n])}
  rows.append(row)
with open(root/'cabinet-kernel-replay.csv','w') as f:
 w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
for r in rows:print(r)
