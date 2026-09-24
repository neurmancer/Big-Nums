## Big nums go brrrrrrrrrrrrrr

> I wanna start with something(after writing all the documentation)
> - To My teacher that said I can't write essays: You were sooooooo wrong _OwO_. anyways you may continue

# BigNums API and usage

> U a goblin that don't wanna read all this documentation? I got you too bruh!

> Check [demo.c](demo.c), find your function, and check the man pages after

> [setup](../README.md#build-the-thing). You are good to go mwa... anyways corpo Neuro takes from here. The return values are still your problem tho.

The [Unix manual](../man/README.md) provides a `bignums(7)` overview and section 3
pages for every public function. The contracts below describe the current
headers and implementation; the jokes do not override the array bounds.


BigNums is a side quest born specifically to fuck with Tupper's self-referential formula and Ramanujan's $\pi$ fuckery.
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
`#include <bignums.h>`. This mode requires Linux/glibc and `ldconfig`
(but again...**read the fucking scripts before running sudo commands**).


The installed library is `libbignums.so.1.0.0`, with SONAME `libbignums.so.1`
and relative symlinks for the SONAME and `-lbignums` linker name. The library
records its own dependency on the math library. Applications that directly
call math functions should also link `-lm`. `-L` controls link-time lookup;
rpath controls runtime lookup. System installs may instead use the system
loader cache, managed by the administrator with `ldconfig` where applicable.

`make -C bigNums` builds the shared library under `build/lib` without installing.
`make -C bigNums check demo` runs both C regression suites and builds the demo.
`build/test_big` and `build/demo` use a relative runtime path to `build/lib`.
`build/test_fft_multiply` links the same library object files directly, using
GNU-style linker `--wrap` support to inject FFT/allocation failures.
`make -C bigNums check` runs the same C regression suites as `test`.
Run `./build/demo` to execute the example. Building it does not run it for you;
the Makefile has limits to its enthusiasm.
`make -C bigNums install` installs both library and manuals, defaulting to
`/usr/local` (the shell scripts default to `$HOME/.local`).

The OG Makefile still works tho

```sh
make -C bigNums MAIN=demo.c TARGET=demo
./bigNums/demo
make -C bigNums MAIN=test.c TARGET=test_big
./bigNums/test_big
```

The custom `MAIN=test.c` driver runs only the general suite. Use `test` or
`check` to include the independent FFT multiplication suite.

To compile directly from this directory:

```sh
cc -std=c99 -Wall -Wextra -O2 demo.c bigNumLibThingy.c smartFFTThingy.c -lm -o demo
```

Application code only needs `#include "bignums.h"`. There is no FFT-header
inclusion-order requirement.


## Representation and ownership

```c
#define MAX_LIMBS 1024

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
for equivalent values. The exponent counts **bits**, not decimal digits. One
limb is 32 bits; the array did not secretly become base ten while you were away.

Both types own their arrays directly; they can be stack variables, embedded in
other structs, or copied by assignment. No constructor allocation or caller-side
`free` is needed. FFT integer multiplication and non-power-of-two standalone
FFT transforms allocate and free internal buffers. Input number objects must
be initialized and have valid sizes/signs; a distinct output object generally
need not be initialized. Pass non-null pointers: the number API does not
validate them. Unused limbs are not part of a value and need not remain cleared
after every operation.

Capacity is 1024 limbs (32768 bits), approximately 9864 significant decimal
digits. Rebuild applications when upgrading from the 256-limb layout. The
SONAME changed from `libbignums.so.0` to `libbignums.so.1` to distinguish the ABI.
`MAX_LIMBS` is fixed in the public header, not a runtime precision setting.
Changing it means rebuilding the library and every application using its types;
substituting a new layout under an old SONAME is asking the loader to lie for you.

## Return values and failure handling

Most arithmetic functions return 0 on success. Error values vary by function:

- `INT_MAX`: integer capacity overflow or float exponent range error, or a zero
  divisor in integer division/modulo and float reciprocal/division.
- `-1`: invalid decimal characters, negative integer subtraction, FFT allocation
  or transform failure, or a negative square-root argument.
- Integer division and modulo return a `uint32_t` remainder, not a status. Their
  `INT_MAX` sentinel can also be a legitimate remainder for a larger divisor;
  validate the divisor before calling.
- Comparisons return -1, 0, or +1 for less, equal, or greater. Bit lookup returns
  0 or 1. Constructors, copy, truncation, and printers return `void`.

Integer scalar arithmetic, parsing, and factorial can leave partial results
on failure. After an overflow, restore a saved value or reinitialize the object
before reusing it; the partial result is not a valid answer in a smaller hat.
Integer FFT multiplication, subtraction, and shifts leave their outputs unchanged
on error. Float arithmetic, normalization, and shifts also leave outputs
unchanged on error. The void float
truncation and printing routines set `errno = ERANGE` for unsupported exponent
or output ranges; truncation leaves its input unchanged and printing writes
nothing on such errors. Include `<errno.h>`, set `errno = 0` before those calls
when detecting a fresh range error, and check `ferror(stdout)` for stream errors.
Success does not promise to clear `errno`. An `INT_MAX` or `-1` arithmetic return
is its own error channel; do not assume those functions also set `errno`.

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
| `bigIntMulFFT(result, a, b)` | FFT multiplication with exact coefficient verification; returns 0, `INT_MAX` for capacity overflow, or -1 for allocation/transform failure. Output may alias either input; errors leave it unchanged. |
| `bigIntSub(result, a, b)` | Unsigned subtraction; returns -1 without changing the output if `a < b`, otherwise 0. Output may alias either input. |
| `bigIntCmp(a, b)` | Compares canonical unsigned integers; returns -1, 0, or +1. |
| `bigIntGetBit(a, index)` | Returns the selected bit, counting from LSB 0, or 0 above the active limbs. Negative indices return 0. |
| `bigIntShiftLeft(a, bits)` | In-place left shift; 0 or `INT_MAX`. Results may use all `MAX_LIMBS`; an extra limb is reserved only for a carry. |
| `bigIntShiftRight(a, bits)` | In-place right shift, discarding low bits; returns 0 for nonnegative counts. Shifting past all active limbs yields zero. |
| `bigIntFactorial(result, n)` | Computes unsigned `n!` using repeated scalar multiplication. Both 0! and 1! are 1. Returns 0 or `INT_MAX`. |

Negative shift counts delegate to the opposite shift. All `int` counts,
including `INT_MIN`, are handled safely. Shifting zero always succeeds.
Specifically, left shift by `INT_MIN` becomes zero; right shift by `INT_MIN`
returns `INT_MAX` for nonzero input and leaves it unchanged.


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
| `bigFloatCopy(dst, src)` | Copies the value and clears unused destination limbs when the objects differ. Self-copy is a no-op; no normalization is performed. |
| `bigFloatNormalize(x)` | Removes leading zero limbs, then shifts the mantissa left to set the top bit, compensating `exp`. Returns 0 or `INT_MAX`. |
| `bigFloatTruncate(x, target_limbs)` | Discards low whole limbs and increases the exponent to compensate for their position. Clamps targets to 1 through `MAX_LIMBS`. Returns void; exponent range errors set `errno = ERANGE` and leave the input unchanged. Discarded bits are not rounded to nearest. |
| `printBigFloat(x, decimal_places)` | Prints decimal text without a newline. Negative place counts become 0. Rounds to nearest, with ties away from zero. See output limits below. |
| `bigFloatShiftLeft(x, bits)` | For nonnegative counts, shifts the mantissa left and normalizes, multiplying the value by 2^bits on success. Negative counts shift right and may discard bits. Returns 0 or `INT_MAX`. |
| `bigFloatShiftRight(x, bits)` | Shifts the mantissa and normalizes; low mantissa bits are discarded, so it can lose precision or become zero. Returns shift/normalization status. |
| `bigFloatCmpAbs(a, b)` | Compares magnitudes ignoring sign; returns -1, 0, or +1. Accounts for exponent and mantissa bit length, then compares aligned bits without truncation. Equivalent representations compare equal. |
| `bigFloatMul(result, a, b)` | Forms an exact double-width product, then rounds toward zero to 32768 significant bits. Returns 0 or `INT_MAX` for exponent range errors. |
| `bigFloatAdd(result, a, b)` | Aligns mantissas with guard bits, adds or subtracts according to signs, and rounds toward zero to 32768 significant bits. Returns 0 or `INT_MAX` for exponent range errors. |
| `bigFloatSub(result, a, b)` | Negates a copy of `b` and calls addition; propagates its status. |
| `bigFloatReciprocal(result, x, target_limbs)` | Scaled integer division for 1/x, rounded toward zero to the requested precision. Zero or exponent range errors return `INT_MAX`. |
| `bigFloatDiv(result, a, b, target_limbs)` | Direct scaled integer division for a/b, rounded toward zero to the requested precision. Zero divisor or exponent range errors return `INT_MAX`. |
| `bigFloatSqrt(result, x, target_limbs)` | Integer square root with rounding toward zero to the requested precision. Negative sign returns -1 (even on signed zero); positive zero succeeds. Exponent range errors return `INT_MAX`. |

Float arithmetic outputs may alias either input. Set `sign = -1` after construction
to create a negative nonzero value; there is no decimal float parser.

Both float shifts operate on the mantissa before normalization. Even a value
that could be scaled by changing only its exponent can fail a left shift when
the mantissa would exceed capacity. Negative counts reverse the shift direction;
a zero count does nothing, including skipping normalization.

### Precision and rounding

Float addition, subtraction, and multiplication round toward zero to at most
32768 significant binary bits. Exact results that fit are preserved. Addition
retains guard bits and tracks discarded tails so that cancellation and
subtraction across widely separated exponents round in the correct direction.
Multiplication uses exact double-width limb arithmetic before rounding.
For negative values, rounding toward zero discards magnitude; it is not floor
toward negative infinity. Yes, the sign gets a vote.

Reciprocal, division, and square root clamp `target_limbs` to 1 through
`MAX_LIMBS`. They compute the result rounded toward zero to
`32 * target_limbs` significant binary bits. Division uses scaled integer long
division; square root uses an integer square-root algorithm. These operations
use bounded wider intermediates and do not rely on approximate Newton seeds.
Division applies the precision limit to the final quotient.
For example, `target_limbs = 4` requests 128 significant binary bits;
`printBigFloat(x, 4)` requests four digits after the decimal point. Those knobs
do different jobs.

Exponent arithmetic uses wider integers and checks the normalized result's
range before storing it. A range error leaves the output unchanged. Rounding
cannot recover precision already lost in the input values, and a sequence of
operations can accumulate error. Near the exponent limits, packing can move
whole zero limbs between mantissa and exponent without changing the value;
if no normalized representation fits the available precision and exponent,
the operation fails instead of producing infinity or a subnormal result.

`bigFloatTruncate` removes low **whole limbs** when the current size exceeds its
clamped target, then normalizes. If the size already fits, it is a no-op; it
does not increase precision or normalize an otherwise unnormalized input.

### Decimal printing

`printBigFloat` always prints a decimal point, even for zero decimal places:
zero with 0 places prints `0.`, and an exact integer such as 2 prints `2.`.
It rounds the complete scaled value to nearest with ties away from zero,
including a carry into the integer part. Conversion uses exact integer
arithmetic; it does not introduce intermediate float rounding.

`printBigInt` can print all 9865 digits of the largest 32768-bit integer.
`printBigFloat` accepts up to `MAX_LIMBS * 10` (10240) decimal places and an
integer part of at most 32768 bits. Larger requests set `errno = ERANGE` and
write nothing. Negative place counts mean zero places. Tiny values, including
those with `INT32_MIN` exponents, round safely to zero when appropriate.
More decimal places describe the stored value; they do not improve its accuracy.
A negative nonzero input that rounds to zero keeps its minus sign, for example
`-0.000`. Canonical zero prints without a minus sign. Neither printer appends a
newline. You bought the digits; the `putchar('\n')` is sold separately.

## Multiplication and FFT implementation

`bigIntMulFFT` splits each 32-bit limb into four base-256 digits, pads to a
power of two, and calls the complex FFT for both inputs and the inverse
convolution. At 1024 limbs it needs at most 8192 transform points.

Every rounded FFT coefficient is checked against an independent modular
convolution using a number-theoretic transform (NTT) modulo 998244353. The
modulus is `119 * 2^23 + 1`, with primitive root 3. Each true coefficient is at most
`4096 * 255 * 255 = 266342400`, below the modulus, so the modular result is the
exact integer coefficient. This is a deterministic coefficient check, not a
probabilistic checksum. If a rounded coefficient differs or is non-finite,
the exact modular coefficients supply the product. No schoolbook recomputation
is needed. Compile-time bounds prevent a capacity increase from silently
invalidating the verification range.

Carry propagation uses base 256 and checks all high product digits for overflow
before publishing the result. Aliasing is supported and errors leave the output
unchanged. Zero products bypass transforms. Nonzero products run in O(n log n)
time for `n = a.size + b.size`, with four temporary heap buffers totaling at
most 320 KiB. Exact verification adds three modular transforms to the three
complex transforms; this is more work than an unchecked FFT. All buffers are
freed on success and failure.
The modular convolution is computed for every nonzero product, not just after
a suspicious FFT result. Recovery from a coefficient mismatch is successful
multiplication; allocation failure or a nonzero FFT status returns -1 instead.
The checker costs actual CPU time. It is not a decorative assertion.

Float multiplication continues to use its exact double-width schoolbook kernel
before rounding to the public mantissa capacity.

The separate `complexFFT.h` exposes `complexNum`, `fft`, and `fft_arbitrary`.
`fft` requires a positive power-of-two length; `fft_arbitrary` uses Bluestein's
algorithm for other positive lengths. Both reject null pointers and invalid
lengths. Bluestein rejects unrepresentable convolution/allocation sizes and
checks allocations. Chirp phases are reduced modulo a full turn before
conversion to double, avoiding loss of accuracy from very large angles.
Forward transforms use a negative sign; inverse transforms divide by the
length. `inverse = 0` selects forward; any nonzero value selects inverse.
Both overwrite the caller's `n` initialized `complexNum` elements and return
0 on success or -1 for their documented argument/allocation errors. A non-null
pointer is not proof that the allocation has `n` elements; the caller owns that
contract. `fft` allocates nothing; Bluestein uses two padded arrays and one
chirp array. Standalone FFTs have no modular verification and remain subject to
floating-point error. Their length is independent of `MAX_LIMBS`; 8192 is the
integer multiplier's maximum transform size, not the standalone API's limit.

## Mechanics and resource bounds

Let `L = MAX_LIMBS` and `P = 32 * L`. The private `WideInt` has `3 * L + 2`
limbs (98368 bits at the current capacity). It is scratch storage, not a new
public integer size you can sneak through the header.

- Float multiplication forms a product of at most `2P` bits. One 32-bit
  multiply-add fits in 64 bits: `(2^32 - 1)^2 + 2 * (2^32 - 1) = 2^64 - 1`.
- Float addition aligns each operand to at most `P + 2` bits, with room for an
  addition carry. A discarded-tail flag makes subtraction round correctly.
- Division scales integer operands to obtain the requested quotient bits, then
  uses binary long division. Square root consumes pairs of scaled radicand bits
  in a restoring algorithm. Their scaled dividend/radicand needs at most `2P` bits.
- Decimal printing computes `mantissa * 5^places * 2^(exp + places)` and rounds
  that integer scaling once. The supported ranges need at most
  `P + ceil(10 * L * log2(10)) = 66785` bits, within the private buffer.

Integer FFT multiplication and its verification take O(n log n) time and O(n)
temporary heap space for `n = a.size + b.size`. The four buffers use at most
`8192 * (2 * sizeof(complexNum) + 2 * sizeof(uint32_t))` bytes: 320 KiB on the
tested ABI, excluding allocator overhead. This can be slower than schoolbook
at these fixed sizes despite the better asymptotic complexity.

Float multiplication is O(a.mantissa.size * b.mantissa.size). Division and square
root have quadratic worst-case work as operand/requested precision grows.
Float temporaries live on the stack; some call chains used about 100 KiB on the
tested build. Actual stack use depends on compiler and optimization, and callers
need room for their own objects too. A smaller `target_limbs` does not resize
the fixed arrays. Big nums bring luggage.

## Known limitations

Storage and exponent ranges remain finite. Float arithmetic rounds toward
zero rather than to nearest; decimal printing uses ties away from zero.
There are no NaN/infinity representations, configurable rounding modes, or
public decimal float parser. Except for the documented FFT pointer/length
checks, callers must provide valid pointers, array sizes, signs, and initialized
number representations(I am not your fucking pointer baby-sitter). Integer division/modulo use an ambiguous remainder sentinel for a zero divisor, so validate the divisor before calling.

## Tests

[test.c](test.c) contains executable checks with independent 64-bit integer and
`long double` numerical oracles. It covers constructors, decimal parsing and
invalid input, scalar operations, division/remainders, bit lookup, shifts,
comparison, subtraction, factorial, capacity errors, exact products and input
aliasing, float normalization/copy/truncation, signed arithmetic, reciprocal,
division, square root, and domain errors. Float approximation checks use a
relative tolerance of 1e-8 (absolute near zero); they do not certify hundreds of
bits of precision. Checks remain active when compiled with `NDEBUG`.

The C suite reports **24633 checks, 0 failures**. It includes full-capacity
products, carries, reciprocal and square-root bit patterns, direct DFT
comparisons, and large FFT round trips.

[test_fft_multiply.c](test_fft_multiply.c) adds **1605 checks, 0 failures**. An
independent base-65536 integer convolution checks every product limb for dense,
sparse, alternating and deterministic random inputs. Linker wrappers confirm
the actual FFT calls and inject allocation failures, transform failures, and
incorrect/non-finite inverse coefficients. Tests verify exact recovery, aliasing,
overflow, unchanged outputs on errors, and cleanup. The wrappers are confined
to the test executable; the shared library has no fault-injection hooks.

Together these are **26238 checks** at 1024 limbs. The suites check selected
full-capacity float patterns, not every bit of every float operation. Neither
suite captures and asserts printer output. Keep that distinction when using
the test count as evidence of accuracy.

Both C suites passed with AddressSanitizer, UndefinedBehaviorSanitizer, and
LeakSanitizer. To reproduce the ordinary checks from the repository root:

```sh
make -C bigNums check
```

For a separate sanitizer build with Clang:

```sh
make -C bigNums check BUILD_DIR=../build/sanitize CC=clang \
  CFLAGS='-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined'
```

Use a separate `BUILD_DIR` when changing compiler flags; Make does not track
flag changes as dependencies. The failure-injection executable requires a
linker supporting GNU-style `--wrap`.

[demo.c](demo.c) separately demonstrates the public API and both stdout printers,
checking arithmetic statuses before printing results.
