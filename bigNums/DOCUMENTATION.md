# BigNums API and usage — ABI 3

> To my teacher that said I can't write essays...You were **WRONG**...petty revenge done! you may continue lol...


BigNums is an experimental C99 library for dynamically sized signed integers
and signed binary floats. See the [README](../README.md) for build/install commands,
[demo.c](demo.c) for complete cleanup, and [migration notes](../MIGRATION.md).

## Representation and ownership

```c
typedef struct {
    uint32_t *limbs;
    size_t size;
    size_t capacity;
    int sign;
} BigInt;

typedef struct {
    BigInt mantissa;
    int32_t exp;
    int sign;
    size_t precision;
} BigFloat;
```

Limb 0 is least significant. The integer value is `sign` times the sum of
`limbs[i] * 2^(32*i)` for `0 <= i < size`, with sign +1 or -1.
Canonical zero has `size == 0` and `sign == +1`;
it may have a retained allocation. Nonzero values have a nonzero highest active
limb. Always maintain `size <= capacity`. Inactive limbs are unspecified.

Each object uniquely owns its allocation. Initialize **every object, including
destinations**, using `BIGINT_INIT`/`BIGFLOAT_INIT` or its Init function. Fresh
initialization allocates nothing. Destroy every initialized object when finished.
Destroy restores the initial state and is safe to repeat. Zero resets the value
while retaining capacity; float Zero also retains precision.

Use Copy for an independent value copy. It can allocate and returns status.
Float Copy copies precision, sign and exponent too. Self-copy succeeds without
allocation. Swap exchanges ownership and metadata without allocation.
**Never use structure assignment or memcpy to copy an owning value.**
Init is for fresh/destroyed objects; calling it on a live allocation leaks it.

Reserve grows integer storage geometrically, never changes the value, never
shrinks, and leaves the object unchanged on failure. Newly reserved limbs are
uninitialized. Before manually increasing size, initialize every newly active
limb and maintain canonical form. Number APIs assume valid pointers, sizes and
signs; they do not generally validate malformed structures.

Floats represent `sign * mantissa * 2^exp`, with sign +1 or -1.
Their mantissas must remain nonnegative (`mantissa.sign == +1`); the float's
own sign field supplies the sign. Do not assign a negative integer mantissa.
Normalization sets bit 31 of the highest mantissa limb and adjusts exp.
Canonical zero has exponent 0 and sign +1. Equivalent values can have different
mantissa widths. The exponent remains signed 32-bit; precision does not enlarge
its range. Internal calculations use checked wider bit counts.

## Status and failure contracts

| Return | Meaning |
| --- | --- |
| 0 | Success. |
| -1 | Allocation failure, or transform failure from integer FFT multiplication. |
| INT_MAX | Unrepresentable allocation/index size or normalized float exponent. |
| -2 | Invalid decimal input, zero float divisor, negative square root (including signed zero), or zero requested precision. |
| -3 | Printer stream failure. |

Include `<limits.h>` when using `INT_MAX`. Comparisons return -1/0/+1 as values;
bit lookup returns 0/1 from the magnitude. Scalar integer division and modulo
return an int64_t remainder with the dividend's sign, or zero for exact division.
For positive uint32_t divisor d, division satisfies a = q*d + r with q truncated
toward zero and |r| < d. A zero divisor returns INT64_MIN without modification;
this sentinel is outside the possible remainder range.

All fallible numeric operations leave public numeric outputs unchanged on failure.
This includes scalar growth, parsing, factorial, copying, normalization, precision
changes, and arithmetic with aliased inputs. Private scratch is discarded on error.
All binary arithmetic outputs may alias either or both inputs. Distinct objects
must not share an owning limb pointer.

ToString functions do not modify inputs and assign `*text` only on success.
The returned buffer belongs to the caller and must be freed. They do not free
a previous `*text`; callers must manage an existing string themselves.
Printers allocate/convert before writing; allocation/range failure writes nothing.
A stream failure can leave partial output. Check their return status and
`ferror(stdout)`. The number API no longer uses errno as its error channel.

## Integer API

| Function | Contract |
| --- | --- |
| `bigIntInit(a)` | Initialize fresh allocation-free zero. |
| `bigIntDestroy(a)` | Free storage and restore initial state. |
| `bigIntZero(a)` | Set zero and retain capacity. |
| `bigIntReserve(a, limbs)` | Ensure capacity for at least limbs uint32_t elements. |
| `bigIntCopy(dst, src)` | Deep copy into an initialized destination. |
| `bigIntSwap(a, b)` | Exchange complete objects without allocation. |
| `int_32ToBigInt(a, value)` | Set a uint32_t value, allocating if necessary. |
| `bigIntFromInt64(a, value)` | Set a signed int64_t, including INT64_MIN, without intermediate signed overflow. |
| `bigIntNegate(a)` | Negate in place without allocation; zero stays positive. |
| `bigIntFromString(a, text)` | Parse decimal digits with an optional leading + or -. Empty text is zero; a lone sign, spaces and separators are invalid. -0 becomes positive zero. |
| `bigIntToString(a, text)` | Produce an allocated, NUL-terminated decimal string. |
| `printBigInt(a)` | Write decimal digits without a newline; return status. |
| `bigIntAddUInt_32(a, b)` | Add a nonnegative uint32_t to a signed value; may cross zero. Reserve before growth. |
| `bigIntMulUInt_32(a, b)` | Multiply by a uint32_t in place; zero is canonicalized. |
| `bigIntDivUInt32(a, divisor)` | Replace with quotient truncated toward zero; return an int64_t signed remainder or INT64_MIN for divisor zero. |
| `bigIntModUInt32(a, divisor)` | Same signed remainder without modifying input. |
| `bigIntCmp(a, b)` | Compare signed values, with negative < zero < positive. |
| `bigIntCmpAbs(a, b)` | Compare magnitudes, ignoring sign. |
| `bigIntGetBit(a, index)` | Read a size_t magnitude bit index; bits beyond size are zero. |
| `bigIntAdd(out, a, b)` | Exact signed addition. |
| `bigIntSub(out, a, b)` | Exact signed subtraction, including negative results. |
| `bigIntShiftLeft(a, bits)` | Exact left shift by an int64_t count. |
| `bigIntShiftRight(a, bits)` | Shift the magnitude right, truncating the signed value toward zero. |
| `bigIntMul(out, a, b)` | Select exact schoolbook or verified FFT multiplication. |
| `bigIntMulFFT(out, a, b)` | Verified FFT within its bounds, exact schoolbook otherwise. |
| `bigIntFactorial(out, n)` | Exact uint32_t n factorial by scalar multiplication; 0! = 1. |

Negative shift counts reverse direction. INT64_MIN is handled without signed
negation overflow. A right shift beyond all bits yields zero; unrepresentably
large left shifts fail without changing the value. Shifting zero allocates nothing.
For example, -7 shifted right one bit is -3. These are magnitude shifts, not
two's-complement arithmetic shifts. Multiplication applies the product of the
operand signs, and all operations canonicalize zero to positive. The uint32_t
scalar parameters and factorial's nonnegative argument retain their types.
There is no MAX_LIMBS macro or fixed integer overflow threshold. Limits derive
from available memory and checked size/bit-count arithmetic.

## Float precision and API

Each initialized float defaults to `BIGFLOAT_DEFAULT_PRECISION` (1024 limbs).
This is an arithmetic policy, not an allocation size or maximum. One precision
limb means 32 significant binary bits. Set it with
`bigFloatSetPrecision(&x, limbs)`, where limbs must be positive. Reducing precision
rounds toward zero; increasing it preserves the stored value without recovering
lost information. A huge unrepresentable request returns INT_MAX.

Add, Sub and Mul round toward zero using the **destination's** precision. This
also applies when the destination aliases an input. Div, Reciprocal and Sqrt
use their explicit precision argument and store it on success. Copy transfers
the source precision. Constructors, Zero and shifts retain destination precision.

| Function | Contract |
| --- | --- |
| `bigFloatInit(x)` | Fresh zero, default precision, no allocation. |
| `bigFloatDestroy(x)` | Free mantissa and restore initial state. |
| `bigFloatZero(x)` | Positive zero; preserve precision and capacity. |
| `bigFloatCopy(dst, src)` | Deep copy value and precision. |
| `bigFloatSwap(a, b)` | Exchange ownership and all metadata. |
| `bigFloatFromUint32(x, v)` | Set and normalize an unsigned value. |
| `bigFloatSetPrecision(x, limbs)` | Set positive precision, rounding and normalizing as necessary. |
| `bigFloatTruncate(x, limbs)` | Alias of SetPrecision; now returns status and changes the precision policy. |
| `bigFloatNormalize(x)` | Normalize/round to the current precision. |
| `bigFloatShiftLeft/Right(x, bits)` | Shift the mantissa, then normalize/round at current precision. |
| `bigFloatCmpAbs(a, b)` | Exact magnitude comparison without allocation. |
| `bigFloatAdd/Sub/Mul(out, a, b)` | Signed arithmetic at destination precision. |
| `bigFloatDiv(out, a, b, limbs)` | Scaled exact integer division, rounded toward zero. |
| `bigFloatReciprocal(out, x, limbs)` | Division of one by x at requested precision. |
| `bigFloatSqrt(out, x, limbs)` | Restoring integer square root at requested precision. |
| `bigFloatToString(x, places, text)` | Allocated decimal text with places fractional digits. |
| `printBigFloat(x, places)` | Same formatting to stdout, without a newline. |

Right shifts discard low mantissa bits and can produce zero; they are not merely
exponent changes. Left shifts grow scratch before normalizing and may round to
the destination's precision. Negative counts reverse direction. A zero shift
does nothing, including skipping normalization.

Addition retains guard bits and discarded-tail information, with working
precision at least as wide as both inputs and the requested output. This
preserves cancellation when inputs are more precise than the destination while
bounding alignment work for distant exponents. Multiplication forms an exact
integer product before rounding. Division and square root use dynamic integer
intermediates. Scratch buffers are reserved before their bit loops.

Exponent checks occur before publishing results. Packing may move whole zero
limbs between mantissa and exponent near exponent boundaries when precision
permits. Failure leaves value and precision unchanged. There are no NaN,
infinity or selectable rounding modes.

## Decimal conversion

Integer conversion uses repeated division of the magnitude by 10^9 with dynamic
output storage and a leading minus sign for negative nonzero values.
Float conversion computes the exact scaled integer
`mantissa * 5^places * 2^(exp + places)`, rounds to nearest with ties away from
zero, and inserts the decimal point. Carry into the integer part is preserved.

The former 10240-place and 32768-bit integer-part limits are removed.
Output is bounded by representable sizes, available memory and computation time.
Very large exponent/output requests can require enormous allocations.

Float text always includes a decimal point: two at zero places is `2.`.
A negative nonzero value rounded to zero keeps its sign, such as `-0.000`.
Canonical zero is positive. More output digits describe the stored value and
cannot improve the accuracy of the preceding calculation. Places now has
type size_t: negative counts are not supported.

## Multiplication and standalone FFT

The generic multiplier uses schoolbook when either operand has fewer than
32 limbs. Otherwise it requests the verified FFT backend. Each limb is split
into four base-256 digits. A complex FFT convolution is checked coefficient by
coefficient against an exact NTT modulo 998244353 (primitive root 3).

The runtime checks require:

- `4 * min(a.size, b.size) * 255^2 < 998244353`.
- The padded transform length is at most 2^23.
- The output size is representable before any narrowing or allocation.

The first bound allows a shorter operand of at most 3837 limbs. A shorter
operand of 3838 limbs exceeds the uniform coefficient bound. Unsupported
operand sizes use exact schoolbook multiplication. Do not remove those guards:
a modular residue ceases to identify an exact coefficient once it can wrap.

Within the supported range, mismatched or non-finite FFT coefficients recover
from the exact NTT result. A transform failure returns -1. All temporary buffers
are freed on success and error. Products are committed by ownership exchange
after success, preserving input/output aliasing.

`complexFFT.h`, `fft` and `fft_arbitrary` retain their original standalone API.
Lengths are positive int counts; radix-2 FFT requires a power of two, and
Bluestein handles other lengths. Forward uses a negative exponential sign;
inverse divides by length. Both modify the supplied array. Null pointers,
invalid lengths and unsupported allocation sizes fail with -1. These standalone
double-precision transforms are approximate and have no modular verification.

## Source organization and scaling

- `bigNumLibThingy.c`: ownership, scalar integers, shared exact integer kernels.
- `multiply.c`: schoolbook dispatch and verified FFT/NTT.
- `bigFloat.c`: precision, normalization, signed float arithmetic.
- `conversion.c`: allocating decimal conversion and stdout wrappers.
- `smartFFTThingy.c`: standalone complex transforms.
- `bignums_internal.h`: shared private declarations; not installed.

There is no WideInt or operand-sized limb array on the arithmetic stack.
Heap scratch scales with operands/requested precision. Objects and fixed-size
scalar temporaries still use the stack. Allocations grow geometrically and
division/square-root buffers are reused within each operation.

Verified FFT takes O(n log n) time and O(n) scratch. Its schoolbook fallback
and general schoolbook multiplication take O(a.size * b.size). Division,
square root, and decimal conversion retain quadratic worst-case work.
Large-number algorithm improvements and cross-operation scratch reuse remain
future work; see the roadmap.

## Tests

`make -C bigNums check` runs the C numerical suite and a separate executable
with linker-wrapped malloc, calloc, realloc, free and FFT. They cover former
capacity boundaries, 1536-limb float results, exact multiplication oracles,
NTT coefficient bounds, initialization/copy/zero/destruction, aliases, size
errors, decimal conversion, and allocation failures at every allocation in
representative operation paths. The test allocation counter must return to
zero after each complete fixture.

These finite regression suites do not constitute a proof over every input.

For compiler/sanitizer commands see the README. Sanitizers can require a host
environment where LeakSanitizer can inspect threads without process tracing.

## Migrating ABI 1 or ABI 2 callers

Rebuild against libbignums.so.3 and the new header. Never replace an old SONAME
with an incompatible library. In every caller:

1. Initialize inputs and destinations and destroy them on every exit path.
2. Replace structure assignment with checked Copy or ownership Swap.
3. Treat size as size_t, zero as size == 0 and sign == +1, and reserve before direct limb writes.
4. Check constructors, copies, truncation and printers: they now return status.
5. Use positive size_t float precision and explicitly choose output precision.
6. Update domain errors to -2 and range/output handling away from errno.
7. Replace fixed-capacity overflow assumptions with growth and allocation handling.
8. ABI 2 callers must account for the added BigInt sign field, which also changes
   BigFloat layout. Use signed comparison/subtraction semantics and int64_t
   scalar remainders with the INT64_MIN zero-divisor sentinel.
9. Use bigIntFromInt64 or signed decimal text for negative values; int_32ToBigInt
   still accepts uint32_t. Keep float mantissas nonnegative.

The installation scripts and prefix/staging options remain unchanged.
