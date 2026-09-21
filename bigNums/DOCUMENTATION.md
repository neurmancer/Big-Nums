## Big nums go brrrrrrrrrrrrrr

> I wanna start with something(after writing all the documentation)
> - To My teacher that said I can't write essays: You were wrong. anyways you may continue

# BigNums API and usage

> U a goblin that don't wanna read 300 lines of documentation? I got you too bruh! Worry not just 
check the demo.c and find your required function and check the man pages after setup (see the [setup](/README.md)) and you are good to go mwa... anyways corpo Neuro takes from here have fun!

The [Unix manual](../man/README.md) provides a `bignums(7)` overview,



BigNums is a side quest had been born for specifically to fuck with Tupper's self referntial formula and Ramajuan's $\pi$ fuckery
(does latex pi work? No fucking clue we'll see when I read the readme)

## Build and run

Use a Linux/ELF C99-or-later compiler, GNU Make, and the system math library.
From the repository root, `./build.sh` builds a position-independent shared
library and installs it, both public headers, and the manuals under
`$HOME/.local`. It calls `man_setup.sh` for manual installation. Override the
prefix with `./build.sh PREFIX=/usr/local` when you have write access, or use
`DESTDIR=/tmp/package PREFIX=/usr` for a staged package. Prefix-based installs do not
invoke sudo or modify shell startup files.

```sh
cc app.c -I"$HOME/.local/include" -L"$HOME/.local/lib" \
    -Wl,-rpath,"$HOME/.local/lib" -lbignums -o app
```

For native system-wide linking without path flags, use `./build.sh --system`.
This builds first and uses sudo (unless already root) to install into
`/usr/local`, including the headers and manuals. It writes `/usr/local/lib`
to `/etc/ld.so.conf.d/bignums.conf` and runs `ldconfig`. On a standard native
Linux toolchain, applications can then use `cc app.c -lbignums -o app` and
`#include <bignums.h>`. This mode requires Linux/glibc and `ldconfig`(but again...**do not run sudo commands before checking the files for youselves**)


The installed library is `libbignums.so.0.1.0`, with SONAME `libbignums.so.0`
and relative symlinks for the SONAME and `-lbignums` linker name. The library
records its own dependency on the math library. Applications that directly
call math functions should also link `-lm`. `-L` controls link-time lookup;
rpath controls runtime lookup. System installs may instead use the system
loader cache, managed by the administrator with `ldconfig` where applicable.

`make -C bigNums` builds the shared library under `build/lib` without installing.
`make -C bigNums test demo` runs the regression suite against it and builds
`build/demo`; these executables use a relative runtime path to `build/lib`.
`make -C bigNums install` installs both library and manuals, defaulting to
`/usr/local` (the shell scripts default to `$HOME/.local`).

The OG Makefile still works tho

```sh
make -C bigNums MAIN=demo.c TARGET=demo
./bigNums/demo
make -C bigNums MAIN=test.c TARGET=test_big
./bigNums/test_big
```

To compile directly from this directory:

```sh
cc -std=c99 -Wall -Wextra -O2 demo.c bigNumLibThingy.c smartFFTThingy.c -lm -o demo
```

Application code only needs `#include "bignums.h"`. There is no FFT-header
inclusion-order requirement.


## Representation and ownership

```c
typedef struct {
    uint32_t limbs[MAX_LIMBS];
    int size;
} BigInt;

typedef struct {
    BigInt mantissa;
    int32_t exp;
    int sign;
} BigFloat;
```

A `BigInt` stores the sum of `limbs[i] * 2^(32*i)` for `0 <= i < size`.
Limb 0 is the least significant limb, independently of machine byte order.
The intended canonical form has `1 <= size <= MAX_LIMBS`, no leading zero
limbs, and zero represented by `size = 1, limbs[0] = 0`.

A `BigFloat` represents `sign * mantissa * 2^exp`, where `sign` is +1 or -1.
Normalization shifts the mantissa until its highest active limb has bit 31 set,
and decreases the exponent to compensate. Zero is canonicalized to mantissa 0,
exponent 0, sign +1. Normalization does not enforce a unique mantissa limb count
for equivalent values.

Both types own their arrays directly; they can be stack variables, embedded in
other structs, or copied by assignment. No constructor allocation or caller-side
`free` is needed. FFT multiplication allocates and frees temporary buffers;
non-power-of-two FFT transforms also allocate internal buffers. Pass valid,
initialized objects and non-null pointers: the API does not validate them.

## Return values and failure handling

Most arithmetic functions return 0 on success. Error values vary by function:

- `INT_MAX`: integer capacity or float exponent overflow, or a zero
  divisor in integer division/modulo and float reciprocal/division.
- `-1`: invalid decimal characters, negative integer subtraction, FFT allocation
  or transform failure, or a negative square-root argument.
- Integer division and modulo return a `uint32_t` remainder, not a status. Their
  `INT_MAX` sentinel can also be a legitimate remainder for a larger divisor;
  validate the divisor before calling.
- Comparisons return -1, 0, or +1 for less, equal, or greater. Bit lookup returns
  0 or 1. Constructors, copy, truncation, and printers return `void`.

Integer scalar arithmetic and parsing can leave partial results on overflow.
Float arithmetic and shifts leave outputs unchanged on error. The void float
truncation and printing routines set `errno = ERANGE` for unsupported exponent
or output ranges; truncation leaves its input unchanged and printing writes
nothing on such errors. Include `<errno.h>` and check stdout for stream errors.

## BigInt functions
(yeah this is where I found out new markdown features)

| Function | Behavior and return value |
| --- | --- |
| `bigIntZero(a)` | Clears all limbs and sets size to 1. |
| `int_32ToBigInt(a, value)` | Initializes from a `uint32_t`. |
| `bigIntFromString(a, text)` | Parses decimal digits; empty text becomes zero and leading zeros are accepted. Signs, whitespace, and other characters return -1. Capacity failure returns `INT_MAX`. |
| `printBigInt(a)` | Writes decimal digits to stdout without a newline; does not modify `a`. |
| `bigIntAddUInt_32(a, b)` | In-place scalar addition; extends the active limbs for a carry. Returns 0 or `INT_MAX`. Inactive limbs are ignored. |
| `bigIntMulUInt_32(a, b)` | In-place scalar multiplication; returns 0 or `INT_MAX`. Multiplication by zero produces canonical zero. |
| `bigIntDivUInt32(a, divisor)` | Replaces `a` with the integer quotient and returns the remainder; zero divisor returns `INT_MAX`. |
| `bigIntModUInt32(a, divisor)` | Returns the remainder without modifying `a`; zero divisor returns `INT_MAX`. |
| `bigIntMulFFT(result, a, b)` | Full multiplication; returns 0, `INT_MAX` for capacity overflow, or -1 for allocation/FFT failure. Output may alias either input. |
| `bigIntSub(result, a, b)` | Unsigned subtraction; returns -1 if `a < b`, otherwise 0. Output may alias either input. |
| `bigIntCmp(a, b)` | Compares canonical unsigned integers; returns -1, 0, or +1. |
| `bigIntGetBit(a, index)` | Returns the selected bit, counting from LSB 0, or 0 above the active limbs. Negative indices return 0. |
| `bigIntShiftLeft(a, bits)` | In-place left shift; 0 or `INT_MAX`. Results may use all `MAX_LIMBS`; an extra limb is reserved only for a carry. |
| `bigIntShiftRight(a, bits)` | In-place right shift, discarding low bits; returns 0 for nonnegative counts. Shifting past all active limbs yields zero. |
| `bigIntFactorial(result, n)` | Computes unsigned `n!` using repeated scalar multiplication. Both 0! and 1! are 1. Returns 0 or `INT_MAX`. |

Negative shift counts delegate to the opposite shift. All `int` counts,
including `INT_MIN`, are handled safely. Shifting zero always succeeds.


```c
BigInt value;
if (bigIntFactorial(&value, 50) == 0) {
    printBigInt(&value);
    putchar('\n'); /* Requires <stdio.h>. */
}
```

## BigFloat functions

| Function | Behavior and return value |
| --- | --- |
| `bigFloatZero(x)` | Initializes canonical positive zero. |
| `bigFloatFromUint32(x, value)` | Initializes and normalizes an unsigned integer. |
| `bigFloatCopy(dst, src)` | Copies the value, clearing unused destination limbs; self-copy is allowed. |
| `bigFloatNormalize(x)` | Removes leading zero limbs, then shifts the mantissa left to set the top bit, compensating `exp`. Returns 0 or `INT_MAX`. |
| `bigFloatTruncate(x, target_limbs)` | Discards low whole limbs and increases the exponent to compensate for their position. Clamps targets to 1 through `MAX_LIMBS`. Returns void; exponent range errors set `errno = ERANGE` and leave the input unchanged. Discarded bits are not rounded to nearest. |
| `printBigFloat(x, decimal_places)` | Prints decimal text without a newline. Negative place counts become 0. Rounds to nearest, with ties away from zero. See output limits below. |
| `bigFloatShiftLeft(x, bits)` | Shifts the mantissa and normalizes, multiplying the value by 2^bits on success. Returns shift/normalization status. |
| `bigFloatShiftRight(x, bits)` | Shifts the mantissa and normalizes; low mantissa bits are discarded, so it can lose precision or become zero. Returns shift/normalization status. |
| `bigFloatCmpAbs(a, b)` | Compares magnitudes ignoring sign; returns -1, 0, or +1. Accounts for exponent and mantissa bit length, then compares aligned bits without truncation. Equivalent representations compare equal. |
| `bigFloatMul(result, a, b)` | Forms an exact double-width product, then rounds toward zero to 8192 significant bits. Returns 0 or `INT_MAX` for exponent range errors. |
| `bigFloatAdd(result, a, b)` | Aligns mantissas with guard bits, adds or subtracts according to signs, and rounds toward zero to 8192 significant bits. Returns 0 or `INT_MAX` for exponent range errors. |
| `bigFloatSub(result, a, b)` | Negates a copy of `b` and calls addition; propagates its status. |
| `bigFloatReciprocal(result, x, target_limbs)` | Scaled integer division for 1/x, rounded toward zero to the requested precision. Zero or exponent range errors return `INT_MAX`. |
| `bigFloatDiv(result, a, b, target_limbs)` | Direct scaled integer division for a/b, rounded toward zero to the requested precision. Zero divisor or exponent range errors return `INT_MAX`. |
| `bigFloatSqrt(result, x, target_limbs)` | Integer square root with rounding toward zero to the requested precision. Negative sign returns -1 (even on signed zero); positive zero succeeds. Exponent range errors return `INT_MAX`. |

Float arithmetic outputs may alias either input. Set `sign = -1` after construction
to create a negative nonzero value; there is no decimal float parser.

### Precision and rounding

Float addition, subtraction, and multiplication round toward zero to at most
8192 significant binary bits. Exact results that fit are preserved. Addition
retains guard bits and tracks discarded tails so that cancellation and
subtraction across widely separated exponents round in the correct direction.
Multiplication uses exact double-width limb arithmetic before rounding.

Reciprocal, division, and square root clamp `target_limbs` to 1 through
`MAX_LIMBS`. They compute the result rounded toward zero to
`32 * target_limbs` significant binary bits. Division uses scaled integer long
division; square root uses an integer square-root algorithm. These operations
use bounded wider intermediates and do not rely on approximate Newton seeds.
Division applies the precision limit to the final quotient.

Exponent arithmetic uses wider integers and checks the normalized result's
range before storing it. A range error leaves the output unchanged. Rounding
cannot recover precision already lost in the input values.

### Decimal printing

`printBigFloat` always prints a decimal point, even for zero decimal places:
zero with 0 places prints `0.`, and an exact integer such as 2 prints `2.`.
It rounds the complete scaled value to nearest with ties away from zero,
including a carry into the integer part. Conversion uses exact integer
arithmetic; it does not introduce intermediate float rounding.

`printBigInt` can print all 2467 digits of the largest 8192-bit integer.
`printBigFloat` accepts up to `MAX_LIMBS * 10` (2560) decimal places and an
integer part of at most 8192 bits. Larger requests set `errno = ERANGE` and
write nothing. Negative place counts mean zero places. Tiny values, including
those with `INT32_MIN` exponents, round safely to zero when appropriate.
More decimal places describe the stored value; they do not improve its accuracy.

## FFT implementation

`bigIntMulFFT` splits each limb into two base-65536 digits, pads the convolution
to a power of two, transforms both inputs, multiplies pointwise, and performs
an inverse transform. It rounds coefficients, clamps small negative values to
zero, propagates carries in base 65536, and packs pairs of digits into limbs.
All temporary buffers are freed before return.

The separate `complexFFT.h` exposes `complexNum`, `fft`, and `fft_arbitrary`.
`fft` requires a positive power-of-two length; `fft_arbitrary` uses Bluestein's
algorithm for other positive lengths. Both reject null pointers and invalid
lengths. Bluestein rejects unrepresentable convolution/allocation sizes and
checks allocations. Chirp phases are reduced modulo a full turn before
conversion to double, avoiding loss of accuracy from very large angles.
Forward transforms use a negative sign; inverse transforms divide by the
length. FFTs still use double precision and are subject to floating-point error.

## Known limitations

Storage and exponent ranges remain finite. Float arithmetic rounds toward
zero rather than to nearest; decimal printing uses ties away from zero.
There are no NaN/infinity representations, configurable rounding modes, or
public decimal float parser. Except for the documented FFT pointer/length
checks, callers must provide valid pointers, array sizes, signs, and initialized
number representations. Integer division/modulo use an ambiguous remainder
sentinel for a zero divisor, so validate the divisor before calling.

## Tests

[test.c](test.c) contains executable checks with independent 64-bit integer and
`long double` numerical oracles. It covers constructors, decimal parsing and
invalid input, scalar operations, division/remainders, bit lookup, shifts,
comparison, subtraction, factorial, capacity errors, FFT products and input
aliasing, float normalization/copy/truncation, signed arithmetic, reciprocal,
division, square root, and domain errors. Float approximation checks use a
relative tolerance of 1e-8 (absolute near zero); they do not certify hundreds of
bits of precision. Checks remain active when compiled with `NDEBUG`.

The current result is **804 checks, 0 failures**. Regression coverage includes
carry growth through the last available limb, addition with stale inactive
storage, parsing across limb boundaries, multiplication by zero, and float
comparison against zero or equivalent representations. Full-width mantissas,
extreme exponents, and mixed-sign subtraction are also checked.
[demo.c](demo.c) separately demonstrates the public API and both stdout printers,
checking arithmetic statuses before printing results.
