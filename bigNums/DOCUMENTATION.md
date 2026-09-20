## Big nums go brrrrrrrrrrrrrr

> I wanna start with something(after writing all the documentation)
> - To My teacher that said I can't write essays: You were wrong. anyways you may continue

# BigNums API and usage

BigNums is an experimental C library for unsigned integers and signed binary
floats. Storage is fixed at `MAX_LIMBS = 128` 32-bit limbs: integers can represent
0 through 2^4096 - 1, and floats have at most 4096 mantissa bits. It is not an
unbounded arbitrary-precision library. Current restrictions are listed
under [Known limitations](#known-limitations).

## Build and run

Use a C99-or-later compiler, Make, and the system math library. Both library
source files are required; the FFT implementation is included in this directory.
From the repository root:

```sh
make -C bigNums MAIN=demo.c TARGET=demo
./bigNums/demo
make -C bigNums MAIN=test.c TARGET=test_big
./bigNums/test_big
```

Always supply `MAIN`: the Makefile default is `main.c`, which is not included.
The test program reports failed checks and returns a nonzero exit status when
any fail. See [Tests](#tests) for coverage and regression cases.

To compile directly from this directory:

```sh
cc -std=c99 -Wall -Wextra -O2 demo.c bigNumLibThingy.c smartFFTThingy.c -lm -o demo
```

Application code only needs `#include "bignums.h"`. There is no FFT-header
inclusion-order requirement. Include `<limits.h>` yourself when checking
`INT_MAX`. The header currently declares the private `clz32` helper as `static`,
which can produce an unused/undefined-static-function compiler warning in
application translation units.

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

- `INT_MAX`: capacity/exponent overflow in operations that check it, or a zero
  divisor in integer division/modulo and float reciprocal/division.
- `-1`: invalid decimal characters, negative integer subtraction, FFT allocation
  or transform failure, or a rejected/failed square root. Float addition and
  subtraction can also propagate `-1`.
- Integer division and modulo return a `uint32_t` remainder, not a status. Their
  `INT_MAX` sentinel can also be a legitimate remainder for a larger divisor;
  validate the divisor before calling.
- Comparisons return -1, 0, or +1 for less, equal, or greater. Bit lookup returns
  0 or 1. Constructors, copy, truncation, and printers return `void`.

An error does not guarantee that the output is unchanged. In-place operations
can partially mutate their operand; parsing can leave a partial result. Check
status before consuming a result. The printers cannot report internal failures.

## BigInt functions

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
| `bigIntSub(result, a, b)` | Unsigned subtraction; returns -1 if `a < b`, otherwise 0. Output may alias `a`, but must not alias a distinct `b`: copying `a` into output would overwrite `b` before subtraction. |
| `bigIntCmp(a, b)` | Compares canonical unsigned integers; returns -1, 0, or +1. |
| `bigIntGetBit(a, index)` | Returns the selected bit, counting from LSB 0, or 0 above the active limbs. Requires a nonnegative index. |
| `bigIntShiftLeft(a, bits)` | In-place left shift; 0 or `INT_MAX`. Its capacity check conservatively reserves an extra limb. |
| `bigIntShiftRight(a, bits)` | In-place right shift, discarding low bits; returns 0 for nonnegative counts. Shifting past all active limbs yields zero. |
| `bigIntFactorial(result, n)` | Computes unsigned `n!` using repeated scalar multiplication. Both 0! and 1! are 1. Returns 0 or `INT_MAX`. |

Negative shift counts delegate to the opposite shift. Do not pass `INT_MIN`,
whose negation is not representable as an `int`, or extreme counts that overflow
intermediate signed arithmetic. Do not use negative bit indices.

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
| `bigFloatTruncate(x, target_limbs)` | Discards low whole limbs and increases the exponent to compensate for their position. Clamps targets below 1 to 1. Returns void. This loses precision; it does not preserve the exact value or round to nearest. |
| `printBigFloat(x, decimal_places)` | Prints decimal text without a newline. Negative place counts become 0. See formatting limitations below. |
| `bigFloatShiftLeft(x, bits)` | Shifts the mantissa and normalizes, multiplying the value by 2^bits on success. Returns shift/normalization status. |
| `bigFloatShiftRight(x, bits)` | Shifts the mantissa and normalizes; low mantissa bits are discarded, so it can lose precision or become zero. Returns shift/normalization status. |
| `bigFloatCmpAbs(a, b)` | Compares magnitudes ignoring sign; returns -1, 0, or +1. Accounts for exponent and mantissa bit length, then compares aligned bits without truncation. Equivalent representations compare equal. |
| `bigFloatMul(result, a, b)` | Multiplies mantissas using FFT, combines signs and exponents, and normalizes. Returns 0 or `INT_MAX`, including when the underlying FFT returns -1. |
| `bigFloatAdd(result, a, b)` | Aligns to the larger exponent by discarding low bits of the other mantissa, then adds or subtracts according to signs. Returns 0, -1, or `INT_MAX`. |
| `bigFloatSub(result, a, b)` | Negates a copy of `b` and calls addition; propagates its status. |
| `bigFloatReciprocal(result, x, target_limbs)` | Newton iteration for 1/x. Zero returns `INT_MAX`; arithmetic errors propagate. Truncates the final result. |
| `bigFloatDiv(result, a, b, target_limbs)` | Computes a reciprocal, then multiplies by `a`; propagates status. Does **not** truncate the final product to `target_limbs`. |
| `bigFloatSqrt(result, x, target_limbs)` | Newton iteration for sqrt(x). Negative sign returns -1 (even on signed zero); positive zero succeeds. Inner division/addition failures become -1; final normalization can return `INT_MAX`. |

Use distinct outputs in floating-point examples; there is no general documented
aliasing contract for all float operations. Set `sign = -1` after construction
to create a negative nonzero value; there is no decimal float parser.

### Precision and iterative routines

Reciprocal and square root clamp `target_limbs` to 1 through `MAX_LIMBS`.
They calculate `working_limbs = min(target_limbs + 2, MAX_LIMBS)` and perform
`6 + ceil(log2(working_limbs))` iterations. This working count controls iteration
count only: intermediate values are **not** truncated to a guard-limb budget.
Intermediate multiplication can therefore overflow before final truncation.
Square root additionally requests `MAX_LIMBS` precision for each inner division.

Reciprocal iterates `y = y * (2 - x*y)` from a signed power-of-two seed.
Square root iterates `y = (y + x/y) / 2`, starting at the power of two given by
flooring half the input's binary magnitude exponent. Final truncation limits
storage, but does not establish a guarantee of 32 correct bits per requested
limb. Exponent alignment and FFT rounding can also affect results.

### Decimal printing

`printBigFloat` always prints a decimal point, even for zero decimal places:
zero with 0 places prints `0.`, and an exact integer such as 2 prints `2.`.
The routine attempts to round the scaled fractional part by adding 0.5, but it
prints the integer part first and does not carry a rounded fraction into it.
It is not a reliable correctly rounded decimal formatter.

The fractional digit buffer is only 22 bytes (21 digits plus a terminator).
Large place counts can overrun it. Keep examples to modest counts such as 6;
do not use this function for arbitrary-length decimal output. Internal
arithmetic errors are ignored, so large magnitudes or scales can also produce
invalid output. More requested decimal places do not establish more accuracy.

## FFT implementation

`bigIntMulFFT` splits each limb into two base-65536 digits, pads the convolution
to a power of two, transforms both inputs, multiplies pointwise, and performs
an inverse transform. It rounds coefficients, clamps small negative values to
zero, propagates carries in base 65536, and packs pairs of digits into limbs.
All temporary buffers are freed before return.

The separate `complexFFT.h` exposes `complexNum`, `fft`, and `fft_arbitrary`.
`fft` requires a positive power-of-two length. `fft_arbitrary` rejects
nonpositive lengths and uses Bluestein's algorithm for non-power-of-two lengths;
it does not simply pad the input and return a different-length transform.
BigInt multiplication always chooses a power-of-two length, so it takes the
radix-2 fast path. Neither FFT routine is a BigInt constructor or arithmetic API.

## Known limitations

Implementation restrictions include conservative left-shift capacity
checks, unvalidated bit/shift arguments, incomplete exponent-overflow checking,
and the precision/printing limitations above. The library sources are experimental and should not
be treated as numerically validated for every supported size.

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
