from pathlib import Path
import json,shlex,subprocess
b=Path(__file__).resolve().parent;inputs=Path('/workspace/luthier-evidence/20261025-host-validators-280e182a-build6/capacity-fixtures-ab');commands=[]
def run(a):
 commands.append(a);(b/'probe-commands.json').write_text(json.dumps(commands,indent=2)+'\n');subprocess.run(a,cwd=b,check=True)
e=next(e for e in json.loads((b/'compile_commands-snapshot.json').read_text()) if e['file'].endswith('/Source/Tests/CombinationTests.cpp') and 'CMakeFiles/LuthierTests.dir' in e['command'])
a=shlex.split(e['command']);a[a.index('-o')+1]=str(b/'ReleaseCapProbe.o');a[a.index('-c')+1]=str(b/'source/Source/Tests/ReleaseCapProbe.cpp');a=[v.replace('-I/workspace/luthier/Source','-I'+str(b/'source/Source')) for v in a];run(a)
for variant in ['baseline','candidate']:
 a=json.loads((inputs/'original-link-command.json').read_text());a[a.index('-o')+1]=str(b/(variant+'-probe'))
 for i,v in enumerate(a):
  if variant=='candidate' and v.endswith('Source/DSP/String/StringEngine.cpp.o'):a[i]=str(b/'StringEngine-candidate.o')
  elif v.endswith(('.o','.a')):a[i]=str(inputs/'objects'/v)
  elif v.startswith('-Wl,--dependency-file='):a[i]='-Wl,--dependency-file='+str(b/(variant+'-probe-link.d'))
 a.append(str(b/'ReleaseCapProbe.o'));run(a)
print('baseline/candidate probes linked from same copied objects; only StringEngine differs')
