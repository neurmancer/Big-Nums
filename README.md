# Big-Nums go brrrrrrrrrrrrrrrrrrrrr

> Neuro Software Distribution Presents... the array finally moved out.

> This repo is under major changes so check [MIGRATION](MIGRATION.md) for further details...


An experimental C99 library for signed big integers and signed binary floats
Integers and float mantissas now own dynamically allocated, little-endian arrays
of 32-bit limbs. **There is no fixed 1024-limb storage limit.** Objects can remain
on the stack; their limb buffers grow on the heap

This is **ABI 3**, with SONAME `libbignums.so.3`. Recompile applications and
migrate their object lifetimes. Plain structure assignment is no longer a value
copy, Every input and output needs initialization and eventual destruction.

(can I take the suit off now? good...)

## Build and check

```sh
make -C bigNums shared
make -C bigNums check demo
./build/demo
```

The C build needs GNU Make, a Linux/ELF C99 toolchain, and the system math
library. The failure-injection suite also needs GNU-style linker `--wrap`.

A separate sanitizer build:

```sh
make -C bigNums check BUILD_DIR=../build/sanitize CC=clang \
  CFLAGS='-O1 -g -std=c99 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' \
  LDFLAGS='-fsanitize=address,undefined'
```

Use a distinct build directory when changing compiler flags; Make does not
track those flags as dependencies.

## Use the library

```c
#include <stdio.h>
#include <bignums.h>

int main(void)
{
    BigInt factorial = BIGINT_INIT;
    BigFloat two = BIGFLOAT_INIT, root = BIGFLOAT_INIT;
    int rc = bigIntFactorial(&factorial, 5000);
    if(!rc) { rc = printBigInt(&factorial); }
    if(!rc) { putchar('\n'); }
    if(!rc) { rc = bigFloatFromUint32(&two, 2); }
    if(!rc) { rc = bigFloatSqrt(&root, &two, 2048); }
    if(!rc) { rc = printBigFloat(&root, 12); }
    if(!rc) { putchar('\n'); }
    bigIntDestroy(&factorial);
    bigFloatDestroy(&two);
    bigFloatDestroy(&root);
    return(rc || ferror(stdout));
}
```

You can also call `bigIntInit` or `bigFloatInit` on a fresh object.
Use `bigIntCopy`/`bigFloatCopy` for independent copies and the corresponding
`Swap` functions to exchange ownership. `Zero` retains the allocation for reuse;
`Destroy` releases it. Never initialize a live object again without destroying it.

Integers now store a sign and an unsigned limb magnitude. Use
`bigIntFromInt64(&x, -42)` or `bigIntFromString(&x, "-42")` for negative values.
Decimal input accepts one optional leading `+` or `-`; zero is always positive.
`bigIntAdd` and `bigIntSub` handle either sign, `bigIntCmp` uses signed ordering,
and `bigIntCmpAbs` compares magnitudes. `bigIntNegate` changes the sign in place.
The existing `int_32ToBigInt` and `UInt32` scalar arguments remain unsigned.

Scalar division and right shifts truncate toward zero: `-7 / 3` is `-2` with
remainder `-1`, and shifting `-7` right one bit gives `-3`. Bit lookup reads the
magnitude, not a two's-complement representation. Float mantissas remain
nonnegative; their sign is still stored in `BigFloat.sign`.

## Precision and arithmetic

| Area | Behavior |
| --- | --- |
| Integers | Exact signed arithmetic with dynamic growth, limited by addressable/indexable storage and allocation success. |
| Multiplication | `bigIntMul` selects schoolbook or verified FFT; `bigIntMulFFT` requests verified FFT where its exactness bounds hold and uses schoolbook elsewhere. |
| Floats | Value is `sign * mantissa * 2^exp`; exponent remains a signed 32-bit bit count. |
| Float precision | Default 1024 significant 32-bit limbs; configure with `bigFloatSetPrecision`. Capacity and precision are independent. |
| Float add/subtract/multiply | Round toward zero using the destination's precision, including aliased outputs. |
| Float reciprocal/division/square root | Use the explicit positive limb precision and store it in the destination. |
| Decimal output | Exact conversion of the stored value; float formatting rounds to nearest, ties away from zero. Buffers are dynamically sized. |

Precision counts binary **limbs**, **not** decimal places. `bigFloatSetPrecision(&x, 4)`
requests 128 significant bits. `printBigFloat(&x, 4)` prints four decimal places.
Increasing precision cannot recover digits already lost.

All fallible number operations report a status: 0 success, -1 allocation/transform failure,
`INT_MAX` size/exponent range failure, -2 invalid input/domain, or -3 stream error.
Comparisons and bit lookup return values. Scalar integer division/modulo now
return an `int64_t` remainder with the dividend's sign. Their zero-divisor
sentinel is `INT64_MIN`, which cannot be a valid remainder; input is unchanged.

Numeric outputs remain unchanged on failure, including parsing, factorial,
and operations with aliased outputs. Printers may partially write on stream
failure. `bigIntToString` and `bigFloatToString` return an allocated string through
a `char **`; free successful results. Failed conversion leaves that pointer
unchanged. Printers now return status, rather than reporting range through errno.

## Install

Build and install into `$HOME/.local` (or the `PREFIX` environment variable):

```sh
./build.sh
cc app.c -I"$HOME/.local/include" -L"$HOME/.local/lib" \
    -Wl,-rpath,"$HOME/.local/lib" -lbignums -o app
man -M "$HOME/.local/share/man" 7 bignums
```

For a native system-wide installation:

```sh
./build.sh --system
cc app.c -lbignums -o app
```

System mode builds first, installs into `/usr/local` using sudo unless already
root, registers `/usr/local/lib`, and runs `ldconfig`.
**Inspect the build itself before using the sudo shit** Don't trust me!

```sh
./build.sh PREFIX=/usr DESTDIR=/tmp/bignums-package
make -C bigNums install PREFIX=/usr/local
```

`LIBDIR`, `INCLUDEDIR`, and `MANDIR` can also be overridden. Direct Make install
defaults to `/usr/local`. The shared library records its math-library dependency.
The legacy `make -C bigNums MAIN=demo.c TARGET=demo` driver still works.

## Limits and remaining work

Arbitrary size means allocation-driven growth, not infinite resources. Size
calculations are checked before allocation. Float exponent range remains finite;
there are no NaN/infinity values, decimal float parser, or selectable rounding modes.
Callers must supply initialized objects, valid pointers and canonical limb sizes.

FFT verification uses the prime 998244353. Runtime guards enforce both the
coefficient bound and the supported transform length. Unsupported sizes use
exact schoolbook multiplication(No more bluestein :/), so correctness does not depend on floating
point accuracy. That fallback is **quadratic**; binary division and restoring square
root are also quadratic. Dynamic allocation removes the storage ceiling, not
those scaling costs. Scratch buffers are reused within division and square root.

See the [API documentation](bigNums/DOCUMENTATION.md),
[migration roadmap](MIGRATION.md), [demo](bigNums/demo.c), and
[Unix manuals](man/README.md). Tests include growth beyond 1024 limbs,
multiplication across the NTT coefficient boundary, ownership, allocation and
transform failures, decimal conversion, and float arithmetic and precision.
They are regression evidence, not an exhaustive proof.



[This is where the all the madness started](https://github.com/neurmancer/Basic-C-Examples/tree/main/reallyBasicThings/projects101/mathFuckery/DSPFuckery/bigNums).
