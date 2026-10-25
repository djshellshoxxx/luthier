import pathlib, subprocess, concurrent.futures, sys, os
base=pathlib.Path(__file__).resolve().parent
sanitize=os.environ.get('ENGINE_SANITIZE','')
builddir=base/('objects-'+sanitize.replace(',','-') if sanitize else 'objects')
builddir.mkdir(exist_ok=True)
root=base.parent/'source'
sources=['BoardSnapshots.cpp','BoardState.cpp','Services/AssetStore.cpp','ExtendedControls.cpp','Board.cpp','BoardEngine.cpp','BoardModel.cpp','TopologyCompiler.cpp','PedalPool.cpp','NodeTable.cpp','PedalRegistry.cpp','Models/PrototypePedals.cpp','Midi/MidiRouter.cpp','Midi/ClockSync.cpp','Midi/MidiMap.cpp','Midi/MidiState.cpp','Midi/MidiDefaults.cpp','Common/GestureDecoder.cpp']
sources += [str(p.relative_to(root/'Source/ReferencePedals')) for p in (root/'Source/ReferencePedals/Generic').glob('*.cpp') if not p.name.startswith('Register')]
sources += [str(x.relative_to(root/'Source/ReferencePedals')) for x in (root/'Source/ReferencePedals').glob('**/Register*.cpp')]
sources += [str(x.relative_to(root/'Source/ReferencePedals')) for x in (root/'Source/ReferencePedals/Routing').glob('*.cpp')]
files=[root/'Source/ReferencePedals'/s for s in sources]
files += [root/'Source/Tests'/s for s in ['RefPedalBoardCoreTests.cpp','RefPedalRoutingTests.cpp','ReferencePedalBoardTests.cpp','RefPedalCpuBudgetTests.cpp','RefPedalRetirementTests.cpp','RefPedalMidiPublicationTests.cpp','AllocationFailureTests.cpp','RefPedalSweepTests.cpp','RefPedalMemoryTests.cpp','RefPedalExtraStateTests.cpp','RefPedalMemoryRegistryTests.cpp','RefPedalStateTests.cpp','RefPedalHistoryFailureTests.cpp','RefPedalSoakTests.cpp','RefPedalControlRaceTests.cpp']]
files += [base/'hooks.cpp',base/'main.cpp']
headerTime=max(p.stat().st_mtime for p in (root/'Source/ReferencePedals').rglob('*.h'))
flags=['g++','-std=c++17','-O1','-g','-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1','-DJUCE_USE_CURL=0','-DJUCE_WEB_BROWSER=0','-DNDEBUG','-DLUTHIER_ALLOCATION_COUNTER=1','-I'+str(root),'-I/workspace/luthier/ThirdParty/JUCE/modules']
if sanitize: flags += ['-fsanitize='+sanitize,'-fno-omit-frame-pointer']
def build(file):
 out=builddir/(file.stem+'.o')
 if not out.exists() or out.stat().st_mtime < max(file.stat().st_mtime,headerTime):
  p=subprocess.run(flags+['-c',str(file),'-o',str(out)],capture_output=True,text=True)
  if p.returncode: print(p.stdout+p.stderr,flush=True); return None
 return str(out)
with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool: objects=list(pool.map(build,files))
if None in objects: sys.exit(1)
objects += [str(pathlib.Path('/workspace/luthier-toolchain/objects')/f) for f in ['juce_core.o','juce_core_CompilationTime.o','juce_audio_basics.o','juce_cryptography.o']]
sys.exit(subprocess.run(['g++']+(['-fsanitize='+sanitize] if sanitize else [])+objects+['-lpthread','-ldl','-lrt','-o',str(builddir/'tests')]).returncode)
