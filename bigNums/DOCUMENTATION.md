## Big nums go brrrrrrrrrrrrrr

> I wanna start with something(after writing all the documentation) 
> - To My teacher that said I can't write essays: You were wrong. anyways you may continue
>
> A C library for arbitrary-precision integers and floats. 
> Little-endian limb storage, FFT-based multiplication, Newton-Raphson division/sqrt. 
> Capped at 2048 bits (MAX_LIMBS = 64 × 32-bit limbs). Built as a side-quest for Tupper's self-referential formula and Ramanujan fuckery.


## ToC

- [How To Use](#how-to-use)

  - [Data Structures](#data-structures)

  - [BigInt – Arbitrary-Precision Integers](#bigint--arbitrary-precision-integers)

  - [BigFloat – Arbitrary-Precision Floats](#bigfloat--arbitrary-precision-floats)

- [Implementation Details](#implementation-details)

- [Compile Instructions](#compile-instructions)

- [New Additions](#new)

---

### How to Use
 
- All the use cases documented in [demo.c](demo.c) with explanations in the comments  

#### Data Structures

All values are stored little-endian (least significant limb at index 0). Carry propagation flows naturally this way.

BigInt definition:

- limbs: array of 32-bit unsigned integers (fixed size MAX_LIMBS = 64)
- size: number of active limbs currently in use
```c
    typedef struct {
        uint32_t limbs[MAX_LIMBS];
        int size;
    } BigInt;

```


BigFloat definition:

- mantissa: a BigInt holding the significand
- exp: exponent as power of 2 (int32_t, can be negative)
- sign: 1 for positive, -1 for negative

```c
    typedef struct {
        BigInt mantissa;
        int32_t exp;
        int sign;
    } BigFloat;
```

Both structs live entirely on the stack. No heap allocation needed for the numbers themselves.

---

#### BigInt – Arbitrary-Precision Integers

- Unsigned only. 
- All functions that can fail return an error code (0 = success).

**Initialization and Conversion:**

- bigIntZero(a): Sets a to 0 (size = 1, all limbs zero)
- int_32ToBigInt(a, val): Sets a to a 32-bit unsigned integer value
- bigIntFromString(a, dec_str): Parses a decimal string into a. Returns 0 on success, -1 if the string contains non-digit characters, INT_MAX if the number would exceed MAX_LIMBS

**Basic Arithmetic:**

- bigIntAddUInt_32(a, b): Adds a 32-bit unsigned integer b to a in-place. Returns 0 or INT_MAX on overflow

- bigIntMulUInt_32(a, b): Multiplies a by a 32-bit unsigned integer b in-place. Returns 0 or INT_MAX on overflow

- bigIntDivUInt32(a, divisor): Divides a by a 32-bit divisor in-place, returns the remainder. 
Returns INT_MAX if divisor is 0

- bigIntModUInt32(a, divisor): Returns a modulo divisor without modifying a. Returns INT_MAX if divisor is 0

- bigIntMulFFT(result, a, b): Multiplies a and b using FFT, 
stores the product in result. Returns 0 on success, INT_MAX if the result would exceed MAX_LIMBS, -1 on memory allocation failure. result can safely alias a or b

- bigIntSub(result, a, b): Subtracts b from a, stores in result. Requires a >= b, returns -1 if b is larger. 
Safe when result aliases a (in-place subtraction), **NOT** safe when result aliases b

**Comparison and Bit Operations:**

- bigIntCmp(a, b): Compares a and b. Returns -1 if a < b, 0 if equal, 1 if a > b

- bigIntGetBit(a, bit_index): Returns the bit at position bit_index (0-indexed, LSB = 0). Returns 0 for out-of-range indices

- bigIntShiftLeft(a, bits): Left-shifts a in-place by bits. Returns 0 or INT_MAX on overflow. 
Negative bits delegates to right-shift

- bigIntShiftRight(a, bits): Right-shifts a in-place by bits, truncating toward zero. Returns 0. 
Negative bits delegates to left-shift

**Usage Example:**

Start by zeroing a BigInt, set it to 42, multiply by 10 to get 420, add 69 to get 489.
Parse another BigInt from a decimal string like "12345678901234567890". 
Multiply them together using bigIntMulFFT for the full product.

---

#### BigFloat – Arbitrary-Precision Floats

Signed arbitrary-precision floats. Mantissa precision is controlled per-operation via a target_limbs parameter.
Exponent is power-of-2 based.

**Initialization and Copying:**

- bigFloatZero(x): Sets x to zero (mantissa = 0, exp = 0, sign = 1)
- bigFloatFromUint32(x, v): Sets x to an unsigned 32-bit integer value and normalizes
- bigFloatCopy(dst, src): Deep copies src into dst. Safe even if dst == src

**Normalization and Truncation:**

- bigFloatNormalize(x): Strips leading zero limbs and shifts mantissa so the most significant bit of the top limb is 1. Adjusts exponent accordingly. 
Returns 0 or INT_MAX

- bigFloatTruncate(x, target_limbs): Chops the mantissa down to target_limbs limbs by shifting right and adjusting the exponent.
Used to remove iteration noise from Newton-Raphson. 
Internal function, called automatically by reciprocal, division, and sqrt

**Arithmetic:**
- bigFloatMul(result, a, b): Multiplies a and b. Handles signs correctly. Returns 0 or INT_MAX on overflow
- bigFloatAdd(result, a, b): Adds a and b. Handles mixed signs by falling back to subtraction. Aligns exponents by shifting the smaller operand's mantissa right. Returns 0 on success, -1 or INT_MAX on error
- bigFloatSub(result, a, b): Subtracts b from a by negating b's sign and calling bigFloatAdd. Returns 0 on success
- bigFloatReciprocal(result, x, target_limbs): Computes 1/x using Newton-Raphson iteration. Returns INT_MAX if x is zero. target_limbs controls how many limbs of precision the result will have
- bigFloatDiv(result, a, b, target_limbs): Divides a by b by computing the reciprocal of b then multiplying. Returns INT_MAX on division by zero or overflow
- bigFloatSqrt(result, x, target_limbs): Computes the square root of x using Newton-Raphson. Returns -1 if x is negative. target_limbs controls final precision

**About target_limbs:**

This parameter sets how many limbs of mantissa precision you want. Higher values mean more accurate results but slower computation. Internally, the Newton-Raphson functions work with target_limbs + 2 guard limbs so floating-point noise from the iterative process stays below your requested precision. The result is automatically truncated back to target_limbs before returning.

**Usage Example:**

Create two BigFloats from integers (2 and 10), divide them with 8 limbs of precision to get 0.2 at roughly 256 bits. Create another BigFloat set to 2 and call sqrt on it with 16 limbs to compute sqrt(2) at roughly 512 bits of precision.

**Shift Functions:**

- bigFloatShiftLeft(x, bits): Shifts the mantissa left by bits, then normalizes. Returns 0 or error code
- bigFloatShiftRight(x, bits): Shifts the mantissa right by bits, then normalizes. Returns 0 or error code

**Comparison:**

- bigFloatCmpAbs(a, b): Compares absolute values ignoring sign. Returns **-1, 0, or 1**.
- Comparison functions are the only kind of function with a return value where -1 is **NOT** an error value
---

### Implementation Details

**Prefered Case**
- The said structs use *PascalCase* 
- The rest of the functions all use the same _camelCase_ typing style


**Limb Layout:**

- The value of a BigInt is the sum over i of (limbs[i] * 2^(32*i)). 
- Little-endian storage means limb index 0 holds the least significant 32 bits. 
- The size field tracks how many limbs are actually in use, with the invariant that the most significant limb is always
non-zero (unless the value is exactly zero, in which case size is 1 and limb[0] is 0).

**FFT Multiplication (bigIntMulFFT):**

- Each 32-bit limb is split into two 16-bit digits (low 16 bits and high 16 bits) so that the FFT operates on values small enough to avoid precision loss in double-precision floating point. 

- The two operand arrays are zero-padded and transformed forward, multiplied pointwise in the frequency domain, then inverse-transformed.

- The convolution result is rounded to the nearest integer (values below zero are clamped to 0 to handle floating-point noise). 

- A single forward carry propagation pass in base 2^16 resolves any digit overflows, then pairs of 16-bit digits are packed back into 32-bit limbs. 

- The FFT size is rounded up to the next power of two for speed; the underlying fft_arbitrary function handles non-power-of-two sizes by falling back to the power-of-two fast path.

- Memory for the FFT arrays and carry buffer is heap-allocated inside the function and freed before returning. No cleanup is required from the caller.

**Normalization (bigFloatNormalize):**

- Normalization ensures the most significant bit of the most significant limb is 1, maximizing precision. 

- The function counts leading zeros of the top limb using clz32 (which uses GCC/Clang's builtin __builtin_clz when available, otherwise falls back to a software loop (_check [implementation file](bigNumLibThingy.c) for more details_). 

- The entire mantissa is then shifted left by that count, and the exponent is decreased by the same amount. Leading zero limbs are stripped first.

**Newton-Raphson Iterations:**

- For both reciprocal and square root, the algorithm starts with a bitwise seed that guarantees the initial guess is within a factor of 2 of the correct answer. This seed is constructed by examining the bit length and exponent of the input.

- The iteration count starts at 6 base iterations (enough for roughly 32 bits of precision from the seed) and adds one extra iteration for each doubling of the working limb count. This gives quadratic convergence: each iteration doubles the number of correct bits.

- Working precision is set to target_limbs + 2 guard limbs.

- These extra limbs absorb the unconverged noise at the bottom of the mantissa during iteration. After the loop finishes, bigFloatTruncate chops the mantissa down to exactly target_limbs, 
shifting the exponent to compensate so the mathematical value is preserved.

**Bitwise Seed for Reciprocal:**

- The seed is y with mantissa = 1 and exponent = -total_bits + 1 - x.exp, where total_bits is the bit position of the most significant 1-bit in x's mantissa. 

 - This guarantees that 1 <= x * y < 2, putting the Newton iteration in the quadratic convergence zone immediately.

**Bitwise Seed for Square Root:**

- The seed is y with mantissa = 1 and exponent = floor(true_exp / 2), where true_exp is the actual binary exponent of x (total_bits - 1 + x.exp). 

- For negative odd true exponents, the floor is computed as (true_exp - 1) / 2 to round correctly toward negative infinity.

**Error Handling Conventions:**

- All functions that can fail return an integer status code. 

- A return value of 0 means **success**.

- INT_MAX indicates overflow (the operation would need more than MAX_LIMBS limbs). 

- A return value of -1 indicates an invalid operation(_except comparison functions_) (division by zero, subtraction that would produce a negative result, square root of a negative number, or memory allocation failure in the FFT function). 

- String parsing returns -1 for invalid characters.

**Memory Management:**

- BigInt and BigFloat structs are designed to live on the stack. 

- Their limbs arrays are fixed-size (MAX_LIMBS elements each). 

- No dynamic memory allocation is needed to create or copy these types.
 
- The only heap allocations in the library are internal to bigIntMulFFT, which allocates temporary arrays for the FFT computation and frees them before returning. 

- Users never need to call free on a BigInt or BigFloat.

**clz32 Implementation:**

- Count leading zeros on a 32-bit value. 

- Uses the compiler builtin __builtin_clz when GCC or Clang is detected, otherwise falls back to a bit-by-bit loop.
- Returns 32 when the input is 0.

**Subtraction Safety Note:**

- bigIntSub is safe when the result pointer equals the a pointer (in-place a = a - b) because it copies a into result first if they differ, and the subtraction loop reads from result and b.

- **It is NOT safe when result aliases b, because the borrow propagation may overwrite b's limbs before they are read.**

---

### Compile Instructions

**Requirements:**

- C99 or later compiler (GCC or Clang recommended for __builtin_clz support)
- The custom FFT 
- Math library linked with -lm
- or make installed on the system to use given [Makefile](Makefile)


**Basic Build Command:**

Compile with GCC or Clang using -O3 for maximum optimization (FFT math benefits heavily from this) and -march=native to enable CPU-specific SIMD instructions that speed up double-precision operations.

Link with -lm for the math library.

```bash
Example: gcc -O3 -march=native -o your_program your_program.c bigNumLibThingy.c smartFFTThingy.c -lm
```

**Header Inclusion Order:**

complexFFT.h must be included before bignums.h because bignums.c depends on the FFT types and functions.

Correct order in your source file should look like:
```c

#include "complexFFT.h"
#include "bignums.h"

```

Using the given Makefile is strongly recommended to simplify this process
 - To use it:

```bash
#Makefile + all the headers and c files(except demo.c and test.c) must be on the same folder hierarchy
make MAIN=source.c TARGET=programName 

```

## New

- Added printBigFloat() and printBigInt() helpers 

- - printBigInt() only takes the number you want to print to screen(uses stdio.h)
- - printBigFloat() takes both the number (Big Float struct pointer) and the decimal precision you want to print **'0' only prints the integer part**
