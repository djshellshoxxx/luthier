# Minimal Generic A E69 patch for main b5

The accompanying `root-generic-a-memory.patch` targets the four existing Generic A
headers and their registration file at b5bec374. It does not import the DSP worker's
new FIR adapter, named candidates, changed audio algorithms or newer descriptor layout.
It requires the E69 interface from engine commit 9adef3c8 (or its integrated successor).

Read-only ownership audit: Boost/Overdrive/Fuzz/Compressor, PedalBase, CommonProcessor,
PedalDsp and the legacy allpass OversamplerAdapter/Oversampler own fixed embedded arrays,
filters, smoothers and scalar state. Compressor external sidechain pointers are nonowning.
No member container allocates in prepare. The per-model payload is therefore sizeof(model),
with zero additional prepared heap or temporary heap payload. Stack scratch and allocator
metadata follow the engine E69 accounting rules. No DSP behavior changes are needed.

The separate snapshot `/workspace/luthier-evidence/root-generic-a-memory/` copies root's
actual ReferencePedals and DSP/Common sources, applies the E69 API and this five-file
patch, and compiles the four actual source implementations with the independent allocation
counter. Root files were not edited. ASAN/UBSAN/leak detection reported zero diagnostics.
The audit passed 432 checks over 8/44.1/48/88.2/96/192 kHz, all three quality policies,
and block size 4096: estimator allocates nothing; estimate equals sizeof; temporary is
zero; prepare itself allocates nothing; actual report and report after reset match.
Registry callback registration also passed a separate syntax compile against the API.

On this Linux x64 compiler: Boost 2792, Overdrive 2472, Fuzz 2904, Compressor 2672 bytes.
These numbers are evidence, not hardcoded ABI assumptions. The code uses sizeof on the
build target. This verifies logical allocation-payload accounting, not RSS or DSP CPU/alias
acceptance. Exact compiler arguments are saved in the snapshot `command.json`;
`results.txt`, empty `stderr.txt`, `build.log` and `registry-syntax.log` preserve the run.
The compiler was GCC with C++17/O1/debug info, address+undefined sanitizers and no omitted
frame pointer; JUCE core used the existing shared compiled core object.
