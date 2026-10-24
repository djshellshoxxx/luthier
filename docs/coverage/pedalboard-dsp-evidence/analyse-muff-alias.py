import numpy as np
for model in ['ehx-big-muff-nyc','ehx-muff-wicker','ehx-green-russian','ehx-deluxe-muff']:
    x=np.fromfile('/tmp/'+model+'-5k.raw',dtype=np.float64)
    a=np.abs(np.fft.rfft(x))*2/len(x)
    carrier=a[5000]
    mask=np.ones(len(a),bool)
    for f in [0,5000,10000,15000,20000]:mask[max(0,f-1):f+2]=False
    i=np.where(mask)[0][np.argmax(a[mask])]
    print(f'{model}: 48k normal4x maxdrive 5k full-scale peak, largest non-harmonic {i}Hz {20*np.log10(a[i]):.9f}dBFS {20*np.log10(a[i]/carrier):.9f}dBc')
