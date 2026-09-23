# Big-Nums go brrrrrrrrrrrrrrrrrrrrr

> Lowkey...you can keep the 'actuaL' explanation of the big nums but for me it's just a side quest...

An experimental C library for unsigned big integers and signed binary floats.
The side quest now has **256 32-bit limbs: 8192 bits** per integer or float
mantissa. Big nums, yes. Infinite nums? Fuck no, there's still an array in there.

`BigInt` stores values from 0 through `2^8192 - 1`. `BigFloat` stores
`sign * mantissa * 2^exp`, with a signed 32-bit binary exponent. Both types own
their storage; use them on the stack or copy them by assignment. FFT operations
allocate temporary buffers internally.

## What actually works right now

| Area | Current implementation |
| --- | --- |
| Integers | Decimal parsing/printing, scalar addition and multiplication, scalar division/remainder, comparison, subtraction, bit lookup, shifts, and factorial. |
| Full integer multiplication | FFT convolution using 16-bit digits and double-precision transforms. Output may alias either input. |
| Signed floats | Addition, subtraction, and multiplication using wider integer intermediates, rounded toward zero to at most 8192 significant bits. |
| Reciprocal and division | Scaled integer long division, rounded toward zero to `32 * target_limbs` significant bits. |
| Square root | Integer square-root algorithm with the same requested precision and rounding rule. |
| Decimal float output | Exact conversion of the stored value, rounded to nearest with ties away from zero; up to 2560 decimal places. |
| Complex FFT API | In-place radix-2 FFT, plus Bluestein transforms for other positive lengths, exposed through `complexFFT.h`. |

`target_limbs` is clamped to 1 through 256. Reciprocal, division, and square root
no longer use Newton iteration. More requested digits do not resurrect precision
you already threw away. Necromancy is not in the header yet.

The current regression run (2026-09-22) reports **804 checks, 0 failures**.
It covers integer arithmetic, aliasing, capacity errors, float arithmetic and
domain errors, and magnitude comparison across different representations and
extreme exponents. Approximate float checks use `long double` with a tolerance
of `1e-8 * max(1, |expected|)`; that is **not a full 8192-bit accuracy check**.
The demo exercises both printers, but the regression suite does not assert their
output or directly test the standalone FFT API.

## Build the thing

From the repository root, build without installing, run the numerical tests,
and build the demo:

```sh
make -C bigNums shared
make -C bigNums test demo
./build/demo
```

The build needs a Linux/ELF C99-or-later toolchain, GNU Make, and the system math
library. It produces `build/lib/libbignums.so.0.1.0` with SONAME
`libbignums.so.0`. The test and demo executables load that library through a
relative runtime path.

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

**Upgrading from 128 limbs? Rebuild the library and every application using it.**
The public struct layouts changed. The library still uses SONAME
`libbignums.so.0`, so the loader name alone will not catch that mismatch.

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
- Integer scalar operations and parsing can leave partial results on failure.
  Float arithmetic leaves its output unchanged on error. The void float
  truncation and printing functions report range errors through `errno = ERANGE`;
  clear `errno` before calling if you need to detect a new error.
- Float shifts move the mantissa, then normalize. Right shifts can discard bits
  or produce zero; left shifts can exhaust mantissa capacity. They are not just
  exponent edits.
- The float printer accepts at most 2560 decimal places and an integer part of
  at most 8192 bits. Unsupported ranges write nothing and set `errno = ERANGE`.
  Both printers omit the newline; float output always includes a decimal point,
  even with zero decimal places.
- Initialize inputs and provide valid pointers, sizes, and signs. There is no
  general input validation. FFT integer multiplication uses double precision
  and does not independently verify the recovered integer product.

## Read the docs or go feral responsibly

See the [API documentation](bigNums/DOCUMENTATION.md),
[usage demo](bigNums/demo.c), and [numerical tests](bigNums/test.c).
If you're a man-pages goblin, the [Unix manual](man/README.md) has you covered:

```sh
man -M "$PWD/man" 7 bignums
man -M "$PWD/man" 3 bigIntFactorial
man -M "$PWD/man" 3 bigFloatDiv
```

Well...this shit started as a side-quest from this repo and here we're...
[the library's origin](https://github.com/neurmancer/Basic-C-Examples/tree/main/reallyBasicThings/projects101/mathFuckery/DSPFuckery/bigNums).
