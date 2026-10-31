# Turing macro JIT differential fuzz

Runs random MME64 programs through `TuringMacroJit` and an interpreter copy (`inc/video_core/macro_turing.h`,
without the uninitialized-register check, since the JIT zero-initializes registers) and compares success,
error, consumed parameters, emits, register reads, data RAM and FIFO size.

From this directory, on Apple Silicon:

```sh
R=../../..
# arm64 backend (native)
clang++ -std=c++20 -O2 -Iinc -I$R/src -I$R/.cache/cpm/oaknut/v2.0.3/include -DARCHITECTURE_arm64 \
    '-DJIT_HEADER="video_core/macro_turing_jit.h"' fuzz.cpp -o /tmp/fuzz_arm64
# x86-64 backend (Rosetta); needs an include dir providing common/x64/xbyak.h and xbyak/
clang++ -arch x86_64 -std=c++20 -O2 -Iinc -I<xbyak-include> -I$R/src -DARCHITECTURE_x86_64 \
    '-DJIT_HEADER="video_core/macro_turing_jit.h"' fuzz.cpp -o /tmp/fuzz_x64

/tmp/fuzz_arm64 25000 1 > a.txt 2> a.err   # <cases> <seed>; stderr has the summary
/tmp/fuzz_x64   25000 1 > x.txt 2> x.err
cmp a.txt x.txt                             # the two backends must match line for line
```

`bench.cpp` builds the same way and times one loop macro (interpreter vs JIT).
