from pathlib import Path
import json,shlex,subprocess,hashlib,shutil
b=Path(__file__).resolve().parent
inputs=Path('/workspace/luthier-evidence/20261025-host-validators-280e182a-build6/capacity-fixtures-ab')
root=Path('/workspace/luthier'); commands=[]
manifest=json.loads((inputs/'object-manifest.json').read_text())
for m in manifest: assert hashlib.sha256(Path(m['copy']).read_bytes()).hexdigest()==m['sha256']
compdb=json.loads((root/'build/compile_commands.json').read_text());(b/'compile_commands-snapshot.json').write_text(json.dumps(compdb,indent=2)+'\n')
def run(args):
 commands.append({'argv':args,'cwd':str(b)});(b/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');subprocess.run(args,cwd=b,check=True)
def compile(original,source,out):
 entry=next(e for e in compdb if e['file'].endswith(original) and 'CMakeFiles/LuthierTests.dir' in e['command'])
 args=shlex.split(entry['command']);args[args.index('-o')+1]=str(out);args[args.index('-c')+1]=str(source)
 args=[a.replace('-I/workspace/luthier/Source','-I'+str(b/'source/Source')) for a in args]
 args+=['-I'+str(b/'source/Source/DSP/String')];run(args)
compile('/Source/DSP/String/StringEngine.cpp',b/'candidate-source/Source/DSP/String/StringEngine.cpp',b/'StringEngine-candidate.o')
args=json.loads((inputs/'original-link-command.json').read_text());args[args.index('-o')+1]=str(b/'candidate')
for i,a in enumerate(args):
 if a.endswith('Source/DSP/String/StringEngine.cpp.o'):args[i]=str(b/'StringEngine-candidate.o')
 elif a.endswith(('.o','.a')):args[i]=str(inputs/'objects'/a)
 elif a.startswith('-Wl,--dependency-file='):args[i]='-Wl,--dependency-file='+str(b/'candidate-link.d')
run(args)
shutil.copyfile(root/'build/LuthierTests_artefacts/Release/LuthierTests',b/'baseline');(b/'baseline').chmod(0o700)
print('candidate linked against819 copied inputs; one production translation unit replaced')
