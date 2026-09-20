# Big-Nums

An experimental C library for unsigned big integers and signed binary floats,
with FFT multiplication and Newton reciprocal/square-root routines. Numbers use
fixed storage: 128 32-bit limbs (4096 integer or mantissa bits).

Build and run from the repository root:

```sh
make -C bigNums MAIN=demo.c TARGET=demo
./bigNums/demo
make -C bigNums MAIN=test.c TARGET=test_big
./bigNums/test_big
```

Requires a C99-or-later compiler, Make, and the math library (`-lm`).
The numerical test suite checks arithmetic results and exits nonzero on failure.
Precision and decimal formatting still have limitations; this library remains
under construction.

See the [API documentation and known limitations](bigNums/DOCUMENTATION.md),
[usage demo](bigNums/demo.c), and [numerical tests](bigNums/test.c).

Originally a side project for other math experiments:
[the library's origin](https://github.com/neurmancer/Basic-C-Examples/tree/main/reallyBasicThings/projects101/mathFuckery/DSPFuckery/bigNums).
