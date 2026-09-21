/* Numerical regression tests. Failures are reported, never hidden by NDEBUG. */
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "bignums.h"

static unsigned checks, failures;
#define CHECK(expr) do { \
    ++checks; \
    if (!(expr)) { \
        ++failures; \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); \
    } \
} while (0)

/* Independent small-value oracle: do not use the parser or comparator to
 * validate arithmetic, since those functions may have their own bugs. */
static int equals_u64(const BigInt *a, uint64_t value) {
    int size = value > UINT32_MAX ? 2 : 1;
    return a->size == size && a->limbs[0] == (uint32_t)value &&
           (size == 1 || a->limbs[1] == (uint32_t)(value >> 32));
}

static long double float_value(const BigFloat *x) {
    long double value = 0;
    for (int i = x->mantissa.size - 1; i >= 0; --i)
        value = ldexpl(value, 32) + x->mantissa.limbs[i];
    return x->sign * ldexpl(value, x->exp);
}

static int near(const BigFloat *x, long double expected) {
    /* This checks numerical accuracy to 1e-8, not arbitrary precision. */
    return fabsl(float_value(x) - expected) <= 1e-8L * fmaxl(1, fabsl(expected));
}

static void integers(void) {
    BigInt a, b, r;
    bigIntZero(&a);
    CHECK(equals_u64(&a, 0));
    int_32ToBigInt(&a, UINT32_MAX);
    CHECK(equals_u64(&a, UINT32_MAX));
    CHECK(bigIntFromString(&a, "12345678901234567890") == 0);
    CHECK(equals_u64(&a, UINT64_C(12345678901234567890)));
    CHECK(bigIntFromString(&a, "") == 0);
    CHECK(equals_u64(&a, 0));
    CHECK(bigIntFromString(&a, "00042") == 0);
    CHECK(equals_u64(&a, 42));
    CHECK(bigIntFromString(&a, "12x") == -1);
    CHECK(bigIntFromString(&a, "-1") == -1);
    CHECK(bigIntFromString(&a, " 1") == -1);
    char oversized[MAX_LIMBS * 10 + 1];
    for (int i = 0; i < MAX_LIMBS * 10; ++i) oversized[i] = '9';
    oversized[MAX_LIMBS * 10] = '\0';
    CHECK(bigIntFromString(&a, oversized) == INT_MAX);

    int_32ToBigInt(&a, 42);
    CHECK(bigIntAddUInt_32(&a, 7) == 0 && equals_u64(&a, 49));
    CHECK(bigIntMulUInt_32(&a, 10) == 0 && equals_u64(&a, 490));
    CHECK(bigIntModUInt32(&a, 13) == 9 && equals_u64(&a, 490));
    CHECK(bigIntDivUInt32(&a, 13) == 9 && equals_u64(&a, 37));
    CHECK(bigIntDivUInt32(&a, 0) == (uint32_t)INT_MAX);
    CHECK(bigIntModUInt32(&a, 0) == (uint32_t)INT_MAX);
    CHECK(equals_u64(&a, 37));

    int_32ToBigInt(&a, 128);
    CHECK(bigIntGetBit(&a, 7) == 1);
    CHECK(bigIntGetBit(&a, 0) == 0 && bigIntGetBit(&a, 32) == 0);
    CHECK(bigIntShiftLeft(&a, 33) == 0);
    CHECK(equals_u64(&a, UINT64_C(1) << 40));
    CHECK(bigIntShiftRight(&a, 35) == 0 && equals_u64(&a, 32));
    CHECK(bigIntShiftLeft(&a, -2) == 0 && equals_u64(&a, 8));
    CHECK(bigIntShiftRight(&a, -1) == 0 && equals_u64(&a, 16));
    CHECK(bigIntShiftRight(&a, MAX_LIMBS * 32) == 0 && equals_u64(&a, 0));
    int_32ToBigInt(&a, 1);
    CHECK(bigIntShiftLeft(&a, MAX_LIMBS * 32) == INT_MAX);

    int_32ToBigInt(&a, 100);
    int_32ToBigInt(&b, 30);
    CHECK(bigIntCmp(&a, &b) == 1 && bigIntCmp(&b, &a) == -1);
    CHECK(bigIntCmp(&a, &a) == 0);
    CHECK(bigIntSub(&r, &a, &b) == 0 && equals_u64(&r, 70));
    CHECK(bigIntSub(&a, &a, &b) == 0 && equals_u64(&a, 70));
    CHECK(bigIntSub(&r, &b, &a) == -1);
    int_32ToBigInt(&a, 1);
    CHECK(bigIntShiftLeft(&a, 32) == 0);
    int_32ToBigInt(&b, 1);
    CHECK(bigIntSub(&a, &a, &b) == 0 && equals_u64(&a, UINT32_MAX));

    const uint32_t values[] = {0, 1, 65535, 65536, 123456789, UINT32_MAX};
    for (unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        for (unsigned j = 0; j < sizeof(values) / sizeof(values[0]); ++j) {
            int_32ToBigInt(&a, values[i]);
            int_32ToBigInt(&b, values[j]);
            uint64_t expected = (uint64_t)values[i] * values[j];
            CHECK(bigIntMulFFT(&r, &a, &b) == 0 && equals_u64(&r, expected));
            CHECK(bigIntMulFFT(&a, &a, &b) == 0 && equals_u64(&a, expected));
            int_32ToBigInt(&a, values[i]);
            CHECK(bigIntMulFFT(&b, &a, &b) == 0 && equals_u64(&b, expected));
        }
    }
    CHECK(bigIntFactorial(&r, 0) == 0 && equals_u64(&r, 1));
    CHECK(bigIntFactorial(&r, 1) == 0 && equals_u64(&r, 1));
    CHECK(bigIntFactorial(&r, 20) == 0 && equals_u64(&r, UINT64_C(2432902008176640000)));
    CHECK(bigIntFactorial(&r, 1000) == INT_MAX);
    bigIntZero(&a);
    a.size = MAX_LIMBS;
    for (int i = 0; i < MAX_LIMBS; ++i) a.limbs[i] = UINT32_MAX;
    int_32ToBigInt(&b, 2);
    CHECK(bigIntMulFFT(&r, &a, &b) == INT_MAX);
    CHECK(bigIntMulUInt_32(&a, 2) == INT_MAX);

    /* Carry growth, stale inactive limbs, and canonical zero regressions. */
    int_32ToBigInt(&a, UINT32_MAX);
    CHECK(bigIntAddUInt_32(&a, 1) == 0 && equals_u64(&a, UINT64_C(1) << 32));
    int_32ToBigInt(&a, 1);
    CHECK(bigIntShiftLeft(&a, 32) == 0);
    CHECK(bigIntMulUInt_32(&a, 0) == 0 && equals_u64(&a, 0));
    CHECK(bigIntCmp(&a, &a) == 0);
    CHECK(bigIntFromString(&a, "4294967296") == 0 &&
          equals_u64(&a, UINT64_C(1) << 32));
    CHECK(bigIntFromString(&a, "18446744073709551616") == 0);
    CHECK(a.size == 3 && a.limbs[0] == 0 && a.limbs[1] == 0 && a.limbs[2] == 1);
    int_32ToBigInt(&a, UINT32_MAX);
    a.limbs[1] = UINT32_MAX; /* Inactive storage is not part of the value. */
    CHECK(bigIntAddUInt_32(&a, UINT32_MAX) == 0 &&
          equals_u64(&a, UINT64_C(8589934590)));
    bigIntZero(&a);
    a.size = MAX_LIMBS - 1;
    for (int i = 0; i < a.size; ++i) a.limbs[i] = UINT32_MAX;
    CHECK(bigIntAddUInt_32(&a, 1) == 0 && a.size == MAX_LIMBS);
    CHECK(a.limbs[MAX_LIMBS - 1] == 1 && a.limbs[0] == 0);
    for (int i = 0; i < MAX_LIMBS; ++i) a.limbs[i] = UINT32_MAX;
    CHECK(bigIntAddUInt_32(&a, 1) == INT_MAX);
}

static void floats(void) {
    BigFloat a, b, r;
    bigFloatZero(&a);
    CHECK(equals_u64(&a.mantissa, 0) && a.exp == 0 && a.sign == 1);
    bigFloatFromUint32(&a, 42);
    CHECK(float_value(&a) == 42 && a.mantissa.limbs[0] == UINT32_C(0xa8000000));
    bigFloatCopy(&b, &a);
    bigFloatCopy(&b, &b);
    CHECK(float_value(&b) == 42);
    CHECK(bigFloatNormalize(&b) == 0 && float_value(&b) == 42);
    CHECK(bigFloatShiftLeft(&b, 3) == 0 && float_value(&b) == 336);
    CHECK(bigFloatShiftRight(&b, 3) == 0 && float_value(&b) == 42);
    bigFloatZero(&a);
    a.mantissa.size = 2;
    a.mantissa.limbs[0] = 123;
    a.mantissa.limbs[1] = UINT32_C(0x80000000);
    bigFloatTruncate(&a, 0); /* Clamped to one limb. */
    CHECK(a.mantissa.size == 1 && a.exp == 32 && float_value(&a) == ldexpl(1, 63));

    bigFloatFromUint32(&a, 1000);
    bigFloatFromUint32(&b, 250);
    CHECK(bigFloatCmpAbs(&a, &b) == 1 && bigFloatCmpAbs(&b, &a) == -1);
    CHECK(bigFloatCmpAbs(&a, &a) == 0);
    CHECK(bigFloatAdd(&r, &a, &b) == 0 && float_value(&r) == 1250);
    CHECK(bigFloatSub(&r, &a, &b) == 0 && float_value(&r) == 750);
    CHECK(bigFloatSub(&r, &b, &a) == 0 && float_value(&r) == -750);
    b.sign = -1;
    CHECK(bigFloatMul(&r, &a, &b) == 0 && float_value(&r) == -250000);
    CHECK(bigFloatAdd(&r, &a, &b) == 0 && float_value(&r) == 750);
    bigFloatCopy(&b, &a);
    CHECK(bigFloatSub(&r, &a, &b) == 0 && float_value(&r) == 0);
    bigFloatZero(&b);
    CHECK(bigFloatMul(&r, &a, &b) == 0 && float_value(&r) == 0);
    CHECK(bigFloatAdd(&r, &a, &b) == 0 && float_value(&r) == 1000);
    CHECK(bigFloatReciprocal(&r, &b, 4) == INT_MAX);
    CHECK(bigFloatDiv(&r, &a, &b, 4) == INT_MAX);
    CHECK(bigFloatSqrt(&r, &b, 4) == 0 && float_value(&r) == 0);
    b.sign = -1;
    CHECK(bigFloatSqrt(&r, &b, 4) == -1);
    bigFloatFromUint32(&a, 2);
    CHECK(bigFloatReciprocal(&r, &a, 4) == 0 && near(&r, 0.5L));
    bigFloatFromUint32(&a, 7);
    CHECK(bigFloatReciprocal(&r, &a, 4) == 0 && near(&r, 1.0L / 7));
    bigFloatFromUint32(&b, 22);
    CHECK(bigFloatDiv(&r, &b, &a, 4) == 0 && near(&r, 22.0L / 7));
    bigFloatFromUint32(&a, 2);
    CHECK(bigFloatSqrt(&r, &a, 4) == 0 && near(&r, sqrtl(2)));
    bigFloatFromUint32(&a, 144);
    CHECK(bigFloatSqrt(&r, &a, 4) == 0 && near(&r, 12));

    bigFloatZero(&b);
    CHECK(bigFloatCmpAbs(&a, &b) == 1);
    bigFloatFromUint32(&a, 1);
    CHECK(bigFloatShiftLeft(&a, 32) == 0);
    bigFloatFromUint32(&b, 2);
    CHECK(bigFloatCmpAbs(&a, &b) == 1);
    CHECK(bigFloatCmpAbs(&b, &a) == -1);
    CHECK(bigFloatSub(&r, &a, &b) == 0 && float_value(&r) == 4294967294.0L);
    CHECK(bigFloatSub(&r, &b, &a) == 0 && float_value(&r) == -4294967294.0L);
}

static void magnitude_comparison(void) {
    BigFloat a, b, r;
    bigFloatZero(&a);
    bigFloatZero(&b);
    CHECK(bigFloatCmpAbs(&a, &b) == 0);
    bigFloatFromUint32(&a, 1);
    CHECK(bigFloatCmpAbs(&b, &a) == -1);

    /* Same value with different mantissa widths, including unnormalized seeds. */
    bigFloatCopy(&b, &a);
    CHECK(bigIntShiftLeft(&b.mantissa, 32) == 0);
    b.exp -= 32;
    b.sign = -1;
    CHECK(bigFloatCmpAbs(&a, &b) == 0);
    CHECK(bigFloatAdd(&r, &a, &b) == 0 && float_value(&r) == 0);
    b.mantissa.limbs[0] = 1;
    CHECK(bigFloatCmpAbs(&a, &b) == -1 && bigFloatCmpAbs(&b, &a) == 1);
    int_32ToBigInt(&b.mantissa, 1);
    b.exp = 0;
    CHECK(bigFloatCmpAbs(&a, &b) == 0);

    /* Full-width comparison must preserve even the lowest bit. */
    bigFloatZero(&b);
    b.mantissa.size = MAX_LIMBS;
    b.mantissa.limbs[MAX_LIMBS - 1] = UINT32_C(0x80000000);
    b.exp = -(32 * MAX_LIMBS - 1);
    CHECK(bigFloatCmpAbs(&a, &b) == 0);
    b.mantissa.limbs[0] = 1;
    CHECK(bigFloatCmpAbs(&a, &b) == -1);
    a.exp = b.exp = INT32_MAX;
    CHECK(bigFloatCmpAbs(&a, &b) == -1);
    a.exp = INT32_MIN;
    CHECK(bigFloatCmpAbs(&a, &b) == -1);
    b.exp = INT32_MIN;
    CHECK(bigFloatCmpAbs(&a, &b) == -1);

    /* Cross-check many exact binary values against an independent oracle. */
    for (uint32_t i = 1; i <= 200; ++i) {
        bigFloatFromUint32(&a, i * 7919);
        bigFloatFromUint32(&b, i * 3571);
        a.exp += (int)(i % 17) - 8;
        b.exp += (int)(i % 13) - 6;
        CHECK(bigIntShiftLeft(&b.mantissa, 32) == 0);
        b.exp -= 32;
        b.sign = -1;
        long double av = fabsl(float_value(&a));
        long double bv = fabsl(float_value(&b));
        int expected = (av > bv) - (av < bv);
        CHECK(bigFloatCmpAbs(&a, &b) == expected);
        CHECK(bigFloatCmpAbs(&b, &a) == -expected);
    }
}

int main(void) {
    integers();
    floats();
    magnitude_comparison();
    printf("%u checks, %u failures\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
