# Native-library portability checks

These tests compile production code and real JUCE 8.0.10, without a GUI or an
audio device. They supplement the full Free/Pro Windows/macOS/Linux matrix.

On Ubuntu, install `clang libc++-dev libc++abi-dev`, fetch the pinned JUCE into
`ThirdParty/JUCE`, then run:

```sh
cmake -S scripts/portability_checks -B build-portability \
  -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DUSE_LIBCXX=ON -DSANITIZE=address,undefined
cmake --build build-portability --parallel 1
build-portability/portability_checks
```

On an x86 CPU advertising FMA, add `-DEXERCISE_FMA=ON`. That deliberately enables
the multiply/add contraction used by ARM builds and verifies exact 0 dB capture.
Do not enable this option on older x86 CPUs or ARM; native ARM builds already
exercise their own floating-point instruction set.

The queued asset tests hold the worker while destruction starts, then require
all queued writes/reads and callbacks to finish. libc++ is essential to the
original reproducer: its `unique_ptr` clears the stored pointer before deleting
the object, exposing jobs that access their owner's pointer during its destructor.
The host-capture tests require exact unity-gain controls, monotonic control
values and exact range endpoints. No state-comparison tolerance is introduced.
