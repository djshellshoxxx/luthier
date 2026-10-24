"""Run the complete E66 four-sign estimated output-allpass experiment.
Usage: python3 reverb-e66-variants.py /tmp/experiment /path/to/juce_core.o
Run from repository root. Product source is read only; each source variant and
raw fixture/density output is retained in the explicitly supplied directory.
"""
from pathlib import Path
import subprocess,sys
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
core=Path(sys.argv[2]);root=Path.cwd();juce=Path('/workspace/luthier/ThirdParty/JUCE/modules')
source=(root/'Source/ReferencePedals/Generic/ReverbFdn.cpp').read_text()
wrapper=(root/'Source/ReferencePedals/Generic/Reverb.cpp').read_text()
# Archive the complete grid at the same positive1.4 reference calibration,
# independently of a later selected signed product calibration.
import re
wrapper,count=re.subn(r'calibration\[\]\{1\.25,[^,]+,1\.45,\.9,1\.9\}', 'calibration[]{1.25,1.4,1.45,.9,1.9}',wrapper)
assert count==1
wrapperPath=out/'Reverb-reference-calibration.cpp';wrapperPath.write_text(wrapper)
needle='const double x=output[c],y=line.read(hallOutputDelays[c][k]-1)-.6*x;\n        line.push(x+.6*y);output[c]=y;'
assert needle in source,'Source changed: review this experiment before regenerating variants.'
for name,g0,g1 in [('pp',.6,.6),('nn',-.6,-.6),('pn',.6,-.6),('np',-.6,.6)]:
    modified=source.replace(needle,f'const double g=k==0?{g0}:{g1};\n        const double x=output[c],y=line.read(hallOutputDelays[c][k]-1)-g*x;\n        line.push(x+g*y);output[c]=y;')
    path=out/(name+'-ReverbFdn.cpp');path.write_text(modified)
    for probe in ['reverb-hall-original-fixtures','reverb-density-seeds']:
        exe=out/(name+'-'+probe)
        cmd=['g++','-std=c++17','-O3','-DNDEBUG','-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1','-DJUCE_STANDALONE_APPLICATION=1','-DJUCE_USE_CURL=0','-I'+str(juce),'-I.','-ISource/ReferencePedals/Generic','docs/coverage/probes/'+probe+'.cpp',str(wrapperPath),str(path),'Source/ReferencePedals/Generic/ReverbPlate.cpp','Source/ReferencePedals/Generic/ReverbSpring.cpp',str(juce/'juce_core/juce_core_CompilationTime.cpp'),str(core),'-pthread','-ldl','-lrt','-o',str(exe)]
        subprocess.run(cmd,check=True)
        with open(out/(name+'-'+probe+'.txt'),'w') as log:subprocess.run([str(exe)],stdout=log,check=True)
    with open(out/(name+'-intervals.txt'),'w') as log:subprocess.run([sys.executable,'docs/coverage/probes/reverb-calibration-intervals.py',str(out/(name+'-reverb-hall-original-fixtures.txt'))],stdout=log,check=True)
    print(name,(out/(name+'-reverb-density-seeds.txt')).read_text().splitlines()[-1],flush=True)
    print((out/(name+'-intervals.txt')).read_text(),flush=True)
