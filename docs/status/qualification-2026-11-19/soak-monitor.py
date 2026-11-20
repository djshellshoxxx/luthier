import datetime
import hashlib
import json
import os
import pathlib
import platform
import subprocess
import time

root=pathlib.Path(__file__).parent
binary=root/'build/board_qualification'
source='51fd4de677fd0d4a056ded5ee2e8845b9e81584d'
env=os.environ.copy()
env['LUTHIER_SOAK']='1'
env.pop('LUTHIER_SOAK_SECONDS',None)
jobs=[]
for name,suite in [('generic-board','RefPedalSoak'),('phaser-board','PhaserBoardQualificationSoak')]:
    stream=(root/(name+'-30min.log')).open('w')
    stamp=datetime.datetime.now(datetime.timezone.utc).isoformat()
    process=subprocess.Popen([str(binary),suite],cwd=root,env=env,stdout=stream,stderr=subprocess.STDOUT)
    jobs.append(dict(name=name,suite=suite,process=process,stream=stream,started_at=stamp,
                     started_monotonic=time.monotonic(),max_rss_kib=0,observations=[]))
    print('Started',name,'pid',process.pid,'at',stamp,flush=True)
report={'source_sha':source,'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest(),
        'platform':platform.platform(),'duration_mode':'1800 seconds wall clock per test',
        'concurrent_runs':True,'scope':'Native Linux component tests; no DAW, audio interface or listening test',
        'results':[]}
next_observation=0.0
while any(job['process'].poll() is None for job in jobs):
    now=time.monotonic()
    if now>=next_observation:
        for job in jobs:
            p=job['process']
            if p.poll() is not None: continue
            elapsed=round(now-job['started_monotonic'],2)
            try:
                status=pathlib.Path(f'/proc/{p.pid}/status').read_text()
                values={line.split(':')[0]:line.split(':',1)[1].strip() for line in status.splitlines() if ':' in line}
                rss=int(values.get('VmRSS','0 kB').split()[0])
                job['max_rss_kib']=max(job['max_rss_kib'],rss)
                job['observations'].append({'elapsed_seconds':elapsed,'rss_kib':rss})
                print(job['name'],'elapsed_seconds',elapsed,'rss_kib',rss,flush=True)
            except (FileNotFoundError,ProcessLookupError): pass
        next_observation=now+60
    time.sleep(2)
for job in jobs:
    code=job['process'].wait()
    job['stream'].close()
    result={key:job[key] for key in ['name','suite','started_at','max_rss_kib','observations']}
    result.update(exit_code=code,finished_at=datetime.datetime.now(datetime.timezone.utc).isoformat(),
                  log=job['name']+'-30min.log')
    report['results'].append(result)
    print(job['name'],'exit_code',code,flush=True)
(root/'soak-results.json').write_text(json.dumps(report,indent=2)+'\n')
raise SystemExit(0 if all(x['exit_code']==0 for x in report['results']) else 1)
