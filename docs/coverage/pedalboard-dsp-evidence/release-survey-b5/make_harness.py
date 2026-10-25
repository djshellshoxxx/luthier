from pathlib import Path
root=Path('/workspace/luthier-evidence/release-survey-b5')
s=Path('/workspace/luthier-evidence/normalization-b5/source/Source/Tests/ComboHarness.h').read_text()
s=s.replace('luthier::combo','luthier::releasesurvey').replace('inline constexpr double kSr    = 48000.0;','inline double kSr = 48000.0;')
s=s.replace('std::vector<float> mono;','''std::vector<float> mono, raw, preCab, mic;
    int missingRaw=0, missingMic=0, actualOn=-1, actualOff=-1, onNote=-1, offNote=-1, activityDrops=0;
    int nonfiniteRaw=0, nonfinitePreCab=0, nonfiniteMic=0;''')
s=s.replace('processor->prepareToPlay (kSr, kBlock);','''auto layout = processor->getBusesLayout();
        layout.outputBuses.set(2, juce::AudioChannelSet::stereo());
        if (!processor->setBusesLayout(layout)) throw std::runtime_error("amp aux layout refused");
        processor->prepareToPlay (kSr, kBlock);''',1)
s=s.replace('stats.mono.assign ((size_t) total, 0.0f);','''stats.mono.assign ((size_t) total, 0.0f);
        stats.raw.assign(total,0);stats.preCab.assign(total,0);stats.mic.assign(total,0);''')
needle='''            processor->processBlock (buffer, midi);
            const double ms'''
replacement='''            processor->processBlock (buffer, midi);
            auto& e = processor->getEngine();
            auto& activity=e.getStringActivity();stats.activityDrops+=activity.getDroppedCount();
            for(int j=0;j<activity.size();++j){const auto& a=activity[j];
                if(a.isNoteOn&&stats.actualOn<0){stats.actualOn=pos+a.sampleOffset;stats.onNote=a.midiNote;}
                if(!a.isNoteOn&&stats.actualOff<0){stats.actualOff=pos+a.sampleOffset;stats.offNote=a.midiNote;}}
            const auto* raw=e.getPreBodyBuffer();
            const auto* amp=e.getTapBuffers().auxRead((int)AuxBus::ampPreCab,0);
            const auto* mic=e.getCabinetEngine().getMicTap(0);
            if(e.getLastSubBlockNumSamples()!=n) ++stats.missingRaw;
            if(!mic||e.getCabinetEngine().getMicTapNumSamples()!=n) ++stats.missingMic;
            for(int j=0;j<n;++j){
                const double r=raw[j],a=amp[j],m=mic?mic[j]:0;
                stats.nonfiniteRaw+=!std::isfinite(r);stats.nonfinitePreCab+=!std::isfinite(a);stats.nonfiniteMic+=!std::isfinite(m);
                stats.raw[pos+j]=(float)r;stats.preCab[pos+j]=(float)a;stats.mic[pos+j]=(float)m;
            }
            const double ms'''
assert needle in s;s=s.replace(needle,replacement)
# Inherit original timing fields only for class compatibility; remove external CPU timing calls.
s=s.replace('const double t0 = luthier::tests::threadCpuTimeSeconds();','const double t0 = 0.0;').replace('1000.0 * (luthier::tests::threadCpuTimeSeconds() - t0)','0.0')
(root/'SurveyHarness.h').write_text(s)
h=s.replace('releasesurvey','stagesurvey')
h=h.replace('layout.outputBuses.set(2,','layout.outputBuses.set(1, juce::AudioChannelSet::stereo());\n        layout.outputBuses.set(2,')
h=h.replace('mono, raw, preCab, mic;','mono, raw, preCab, mic, di;').replace('stats.raw.assign(total,0);','stats.di.assign(total,0);stats.raw.assign(total,0);')
h=h.replace('const auto* amp=e.getTapBuffers()', 'const auto* di=e.getTapBuffers().auxRead((int)AuxBus::di,0);\n            const auto* amp=e.getTapBuffers()')
h=h.replace('stats.raw[pos+j]=(float)r;', 'stats.di[pos+j]=di[j];stats.raw[pos+j]=(float)r;')
(root/'StageHarness.h').write_text(h)
