# Big-Nums go brrrrrrrrrrrrrrrrrrrrr


> well...At this point I can put NSD tags everywhere I guess so here you go: **Neuro Software Distribution Presents...**

> Lowkey...you can keep the 'actuaL' explanation of the big nums but for me it's just a side quest...

An experimental C library for unsigned big integers and signed binary floats.
The side quest now has **1024 32-bit limbs: 32768 bits** per integer or float
mantissa, about **9864 significant decimal digits** of capacity. The largest
integer has 9865 decimal digits. Big nums, yes. Infinite nums? Fuck no, there's
still an array in there.

`BigInt` stores values from 0 through `2^32768 - 1`. `BigFloat` stores
`sign * mantissa * 2^exp`, with a signed 32-bit binary exponent. Both types own
their storage; use them on the stack or copy them by assignment. FFT integer
multiplication and standalone Bluestein transforms allocate temporary buffers
internally and free them before returning.

## What (allegedly) works right now

| Area | Current implementation |
| --- | --- |
| Integers | Decimal parsing/printing, scalar addition and multiplication, scalar division/remainder, comparison, subtraction, bit lookup, shifts, and factorial. |
| Full integer multiplication | FFT convolution with 8-bit digits and exact modular verification of every coefficient. Output may alias either input. |
| Signed floats | Addition/subtraction with guard bits; multiplication with an exact schoolbook product. Results round toward zero to at most 32768 significant bits. |
| Reciprocal and division | Scaled integer long division, rounded toward zero to `32 * target_limbs` significant bits. |
| Square root | Integer square-root algorithm with the same requested precision and rounding rule. |
| Decimal float output | Exact conversion of the stored value, rounded to nearest with ties away from zero; up to 10240 decimal places. |
| Complex FFT API | In-place radix-2 FFT, plus Bluestein transforms for other positive lengths, exposed through `complexFFT.h`. |

`target_limbs` is clamped to 1 through 1024. These are **32-bit limbs**, not
decimal places: `target_limbs = 4` requests 128 significant binary bits.
Reciprocal/division use integer long division; square root uses a restoring
integer algorithm. More requested digits do not resurrect precision you already
threw away. Necromancy is not in the header yet. A long calculation can accumulate
rounding error even when every operation follows its rounding contract.

The regression run at 1024 limbs reports **26238 C checks, all passing**.
Tests include full-capacity products, carries, and reciprocal and square-root
bit patterns, plus independent multiplication oracles and injected FFT/allocation
failures. They also check the standalone FFT against a direct DFT and test large
transform round trips. AddressSanitizer, UndefinedBehaviorSanitizer, and
LeakSanitizer runs passed. The suite does **not** check every bit of every float
operation or assert the printers' output. Passing tests are evidence; the test
counter is not a mathematical blessing. See the [test details](bigNums/DOCUMENTATION.md#tests).

## Yes, the FFT function actually does FFT

`bigIntMulFFT` splits limbs into 8-bit digits and uses up to 8192 FFT points.
It also computes an exact modular convolution using a number-theoretic transform
(NTT). Every rounded FFT coefficient must agree with that result; if it doesn't,
the exact coefficients supply the product. Floating-point noise does not get to
freestyle your integer.

This verification runs on every nonzero product. It adds three modular transforms
to the three complex transforms, keeps O(n log n) complexity, and can still be
slower than schoolbook multiplication at these sizes. Big-O does not pay your
constant factors. `bigFloatMul` uses its own exact schoolbook kernel; scalar
integer multiplication and factorial also have their own paths.

## Build the thing

From the repository root, build without installing, run the numerical tests,
and build the demo:

```sh
make -C bigNums shared
make -C bigNums check demo
./build/demo
```

The build needs a Linux/ELF C99-or-later toolchain, GNU Make, and the system math
library. It produces `build/lib/libbignums.so.1.0.0` with SONAME
`libbignums.so.1`. `build/test_big` and `build/demo` load that library through a
relative runtime path. The second test, `build/test_fft_multiply`, links the
library object files directly so linker wrappers can inject failures. Running
the tests requires a linker supporting GNU-style `--wrap`.

`make -C bigNums test` and `make -C bigNums check` run the same two C suites.
`demo` builds the example; `./build/demo` actually runs it. The build does not
install anything until you ask for an install target or run `build.sh`.

To build and install the library, both public headers, and man pages under
`$HOME/.local` (or your `PREFIX` environment variable):

```sh
./build.sh
cc app.c -I"$HOME/.local/include" -L"$HOME/.local/lib" \
    -Wl,-rpath,"$HOME/.local/lib" -lbignums -o app
man -M "$HOME/.local/share/man" 7 bignums
```

Use `#include <bignums.h>` in your application. `-L` tells the linker where to
look; rpath tells the runtime loader where to look. Yes, those are separate jobs.
The shared library records its math-library dependency; add `-lm` if your own
application calls math functions directly. No groff is needed to install the
manual sources.

For the usual system-wide `-lbignums` experience:

```sh
./build.sh --system
cc app.c -lbignums -o app
man 7 bignums
```

System mode builds first, then uses sudo unless already root to install into
`/usr/local`. It registers `/usr/local/lib` in
`/etc/ld.so.conf.d/bignums.conf` and runs `ldconfig`. On a standard native Linux
toolchain, no extra compiler path flags or shell path edits are needed.
This mode requires Linux/glibc with `ldconfig` and accepts no path overrides.

For custom prefixes or package staging:

```sh
./build.sh PREFIX=/usr/local
./build.sh PREFIX=/usr DESTDIR=/tmp/bignums-package
```

Prefix-based installs require write access, do not invoke sudo, and do not
refresh the loader cache. `LIBDIR`, `INCLUDEDIR`, and `MANDIR` can also be
overridden. Direct `make -C bigNums install` defaults to `/usr/local`; the shell
scripts default to `$HOME/.local`.

The OG custom-driver commands still work too:

```sh
make -C bigNums MAIN=demo.c TARGET=demo
./bigNums/demo
make -C bigNums MAIN=test.c TARGET=test_big
./bigNums/test_big
```

That custom `MAIN=test.c` command runs the general suite only. Use `check` for
the FFT multiplication and failure-injection suite as well.

**Upgrading from 256 limbs? Rebuild the library and every application using it.**
The public struct layouts changed. The new SONAME is `libbignums.so.1`;
applications built for `libbignums.so.0` must keep using their old library until
rebuilt. Do not rename or symlink the new library to the old SONAME.

## Small example, big number

Save this as `app.c` and use one of the compile commands above:

```c
#include <stdio.h>
#include <bignums.h>

int main(void)
{
    BigInt factorial;
    BigFloat two, root;

    if (bigIntFactorial(&factorial, 50) != 0)
        return 1;
    printBigInt(&factorial);
    putchar('\n');

    bigFloatFromUint32(&two, 2);
    if (bigFloatSqrt(&root, &two, 4) != 0)
        return 1;
    printBigFloat(&root, 6); /* 1.414214 */
    putchar('\n');
    return ferror(stdout) ? 1 : 0;
}
```

## The fine print, because math does not give a shit about vibes

- Storage is fixed. There are no NaN/infinity values, configurable rounding
  modes, or decimal float parser. Float arithmetic rounds toward zero; decimal
  printing rounds to nearest with ties away from zero.
- Most arithmetic returns `0` on success, with `INT_MAX` or `-1` on failure.
  Check the function's contract: comparisons and bit lookup return values,
  and integer division/modulo return a remainder. Their zero-divisor sentinel
  can also be a valid remainder, so check the divisor yourself.
- Integer scalar operations, parsing, and factorial can leave partial results on failure.
  Float arithmetic leaves its output unchanged on error. The void float
  truncation and printing functions report range errors through `errno = ERANGE`;
  clear `errno` before calling if you need to detect a new error.
- Float shifts move the mantissa, then normalize. Right shifts can discard bits
  or produce zero; left shifts can exhaust mantissa capacity. They are not just
  exponent edits. Negative counts reverse the direction, including its capacity
  limits and possible bit loss.
- The float printer accepts at most 10240 decimal places and an integer part of
  at most 32768 bits. Unsupported ranges write nothing and set `errno = ERANGE`.
  Both printers omit the newline; float output always includes a decimal point,
  even with zero decimal places. A negative nonzero value rounded to zero keeps
  its minus sign. Those 10240 output places describe the stored value; they do
  not promise 10240 accurate digits of your original calculation.
- Initialize inputs and provide valid pointers, sizes, and signs. There is no
  general input validation. Standalone FFT transforms use double precision;
  integer multiplication uses FFT with exact coefficient verification in
  O(n log n) time and up to 320 KiB of temporary heap storage. Verification adds
  transform work. Float multiplication, division, and square root take quadratic
  time as operand/requested precision grows. Float routines can use about
  100 KiB of stack including callees on the tested build. See the
  [mechanics and resource bounds](bigNums/DOCUMENTATION.md#mechanics-and-resource-bounds).

## Read the docs or go feral responsibly

See the [API documentation](bigNums/DOCUMENTATION.md),
[usage demo](bigNums/demo.c), [numerical tests](bigNums/test.c), and
[FFT multiplication tests](bigNums/test_fft_multiply.c).
If you're a man-pages goblin, the [Unix manual](man/README.md) has you covered:

```sh
man -M "$PWD/man" 7 bignums
man -M "$PWD/man" 3 bigIntFactorial
man -M "$PWD/man" 3 bigFloatDiv
```

Well...this shit started as a side-quest from this repo and here we're...
[the library's origin](https://github.com/neurmancer/Basic-C-Examples/tree/main/reallyBasicThings/projects101/mathFuckery/DSPFuckery/bigNums).
