/* Numerical regression tests. Failures are reported, never hidden by NDEBUG. */

/*
    This is where we test the fucking Zoo...
*/

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "bignums.h"
#include "complexFFT.h"
#include <string.h>

#define TEST_LIMBS 1536 /* A test size, beyond the former 1024-limb limit. */

static unsigned checks, failures;
#define CHECK(expr)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(expr)) {                                                                              \
            ++failures;                                                                            \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr);                                \
        }                                                                                          \
    } while(0)

/* Independent small-value oracle: do not use the parser or comparator to
 * validate arithmetic, since those functions may have their own bugs. */
static int equals_u64(const BigInt *a, uint64_t value) {
    if(a->sign != 1) { return(0); }
    if(!value) { return(a->size == 0); }
    size_t size = value > UINT32_MAX ? 2 : 1;
    return(a->size == size && a->limbs[0] == (uint32_t)value &&
           (size == 1 || a->limbs[1] == (uint32_t)(value >> 32)));
}

static int equals_i64(const BigInt *a, int64_t value) {
    uint64_t magnitude = value < 0 ? (uint64_t)(-(value + 1)) + 1 : (uint64_t)value;
    size_t size = !magnitude ? 0 : magnitude > UINT32_MAX ? 2 : 1;
    return(a->sign == (value < 0 ? -1 : 1) && a->size == size &&
           (!size || a->limbs[0] == (uint32_t)magnitude) &&
           (size < 2 || a->limbs[1] == (uint32_t)(magnitude >> 32)));
}

static long double float_value(const BigFloat *x) {
    long double value = 0;
    for(size_t i = x->mantissa.size; i-- > 0;) {
        value = ldexpl(value, 32) + x->mantissa.limbs[i];
    }
    return(x->sign * ldexpl(value, x->exp));
}

static int near(const BigFloat *x, long double expected) {
    /* This checks numerical accuracy to 1e-8, not arbitrary precision. */
    return(fabsl(float_value(x) - expected) <= 1e-8L * fmaxl(1, fabsl(expected)));
}

static void integers(void) {
    BigInt a = BIGINT_INIT, b = BIGINT_INIT, r = BIGINT_INIT;
    bigIntZero(&a);
    CHECK(equals_u64(&a, 0));
    CHECK(int_32ToBigInt(&a, UINT32_MAX) == 0);
    CHECK(equals_u64(&a, UINT32_MAX));
    CHECK(bigIntFromString(&a, "12345678901234567890") == 0);
    CHECK(equals_u64(&a, UINT64_C(12345678901234567890)));
    CHECK(bigIntFromString(&a, "") == 0);
    CHECK(equals_u64(&a, 0));
    CHECK(bigIntFromString(&a, "00042") == 0);
    CHECK(equals_u64(&a, 42));
    CHECK(bigIntFromString(&a, "12x") == -2);
    CHECK(bigIntFromString(&a, "-1") == 0 && equals_i64(&a, -1));
    CHECK(bigIntFromString(&a, " 1") == -2);
    char oversized[TEST_LIMBS * 10 + 1];
    for(int i = 0; i < TEST_LIMBS * 10; ++i) {
        oversized[i] = '9';
    }
    oversized[TEST_LIMBS * 10] = '\0';
    CHECK(bigIntFromString(&a, oversized) == 0 && a.size > 1024);

    CHECK(int_32ToBigInt(&a, 42) == 0);
    CHECK(bigIntAddUInt_32(&a, 7) == 0 && equals_u64(&a, 49));
    CHECK(bigIntMulUInt_32(&a, 10) == 0 && equals_u64(&a, 490));
    CHECK(bigIntModUInt32(&a, 13) == 9 && equals_u64(&a, 490));
    CHECK(bigIntDivUInt32(&a, 13) == 9 && equals_u64(&a, 37));
    CHECK(bigIntDivUInt32(&a, 0) == INT64_MIN);
    CHECK(bigIntModUInt32(&a, 0) == INT64_MIN);
    CHECK(equals_u64(&a, 37));

    CHECK(int_32ToBigInt(&a, 128) == 0);
    CHECK(bigIntGetBit(&a, 7) == 1);
    CHECK(bigIntGetBit(&a, 0) == 0 && bigIntGetBit(&a, 32) == 0);
    CHECK(bigIntShiftLeft(&a, 33) == 0);
    CHECK(equals_u64(&a, UINT64_C(1) << 40));
    CHECK(bigIntShiftRight(&a, 35) == 0 && equals_u64(&a, 32));
    CHECK(bigIntShiftLeft(&a, -2) == 0 && equals_u64(&a, 8));
    CHECK(bigIntShiftRight(&a, -1) == 0 && equals_u64(&a, 16));
    CHECK(bigIntShiftRight(&a, TEST_LIMBS * 32) == 0 && equals_u64(&a, 0));
    CHECK(int_32ToBigInt(&a, 1) == 0);
    CHECK(bigIntShiftLeft(&a, TEST_LIMBS * 32) == 0);
    CHECK(a.size == TEST_LIMBS + 1 && bigIntGetBit(&a, TEST_LIMBS * 32));

    CHECK(int_32ToBigInt(&a, 100) == 0);
    CHECK(int_32ToBigInt(&b, 30) == 0);
    CHECK(bigIntCmp(&a, &b) == 1 && bigIntCmp(&b, &a) == -1);
    CHECK(bigIntCmp(&a, &a) == 0);
    CHECK(bigIntSub(&r, &a, &b) == 0 && equals_u64(&r, 70));
    CHECK(bigIntSub(&a, &a, &b) == 0 && equals_u64(&a, 70));
    CHECK(bigIntSub(&r, &b, &a) == 0 && equals_i64(&r, -40));
    CHECK(int_32ToBigInt(&a, 1) == 0);
    CHECK(bigIntShiftLeft(&a, 32) == 0);
    CHECK(int_32ToBigInt(&b, 1) == 0);
    CHECK(bigIntSub(&a, &a, &b) == 0 && equals_u64(&a, UINT32_MAX));

    const uint32_t values[] = {0, 1, 65535, 65536, 123456789, UINT32_MAX};
    for(unsigned i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        for(unsigned j = 0; j < sizeof(values) / sizeof(values[0]); ++j) {
            CHECK(int_32ToBigInt(&a, values[i]) == 0);
            CHECK(int_32ToBigInt(&b, values[j]) == 0);
            uint64_t expected = (uint64_t)values[i] * values[j];
            CHECK(bigIntMulFFT(&r, &a, &b) == 0 && equals_u64(&r, expected));
            CHECK(bigIntMulFFT(&a, &a, &b) == 0 && equals_u64(&a, expected));
            CHECK(int_32ToBigInt(&a, values[i]) == 0);
            CHECK(bigIntMulFFT(&b, &a, &b) == 0 && equals_u64(&b, expected));
        }
    }
    CHECK(bigIntFactorial(&r, 0) == 0 && equals_u64(&r, 1));
    CHECK(bigIntFactorial(&r, 1) == 0 && equals_u64(&r, 1));
    CHECK(bigIntFactorial(&r, 20) == 0 && equals_u64(&r, UINT64_C(2432902008176640000)));
    CHECK(bigIntFactorial(&r, 5000) == 0 && r.size > 1024);
    bigIntZero(&a);
    CHECK(bigIntReserve(&a, TEST_LIMBS) == 0);
    a.size = TEST_LIMBS;
    for(int i = 0; i < TEST_LIMBS; ++i) {
        a.limbs[i] = UINT32_MAX;
    }
    CHECK(int_32ToBigInt(&b, 2) == 0);
    CHECK(bigIntMulFFT(&r, &a, &b) == 0 && r.size == TEST_LIMBS + 1);
    CHECK(bigIntMulUInt_32(&a, 2) == 0 && a.size == TEST_LIMBS + 1);

    /* Carry growth, stale inactive limbs, and canonical zero regressions. */
    CHECK(int_32ToBigInt(&a, UINT32_MAX) == 0);
    CHECK(bigIntAddUInt_32(&a, 1) == 0 && equals_u64(&a, UINT64_C(1) << 32));
    CHECK(int_32ToBigInt(&a, 1) == 0);
    CHECK(bigIntShiftLeft(&a, 32) == 0);
    CHECK(bigIntMulUInt_32(&a, 0) == 0 && equals_u64(&a, 0));
    CHECK(bigIntCmp(&a, &a) == 0);
    CHECK(bigIntFromString(&a, "4294967296") == 0 && equals_u64(&a, UINT64_C(1) << 32));
    CHECK(bigIntFromString(&a, "18446744073709551616") == 0);
    CHECK(a.size == 3 && a.limbs[0] == 0 && a.limbs[1] == 0 && a.limbs[2] == 1);
    CHECK(int_32ToBigInt(&a, UINT32_MAX) == 0);
    a.limbs[1] = UINT32_MAX; /* Inactive storage is not part of the value. */
    CHECK(bigIntAddUInt_32(&a, UINT32_MAX) == 0 && equals_u64(&a, UINT64_C(8589934590)));
    bigIntZero(&a);
    CHECK(bigIntReserve(&a, TEST_LIMBS) == 0);
    a.size = TEST_LIMBS - 1;
    for(size_t i = 0; i < a.size; ++i) {
        a.limbs[i] = UINT32_MAX;
    }
    CHECK(bigIntAddUInt_32(&a, 1) == 0 && a.size == TEST_LIMBS);
    CHECK(a.limbs[TEST_LIMBS - 1] == 1 && a.limbs[0] == 0);
    for(int i = 0; i < TEST_LIMBS; ++i) {
        a.limbs[i] = UINT32_MAX;
    }
    CHECK(bigIntAddUInt_32(&a, 1) == 0 && a.size == TEST_LIMBS + 1);
    bigIntDestroy(&a);
    bigIntDestroy(&b);
    bigIntDestroy(&r);
}

static void floats(void) {
    BigFloat a = BIGFLOAT_INIT, b = BIGFLOAT_INIT, r = BIGFLOAT_INIT;
    bigFloatZero(&a);
    CHECK(equals_u64(&a.mantissa, 0) && a.exp == 0 && a.sign == 1);
    CHECK(bigFloatFromUint32(&a, 42) == 0);
    CHECK(float_value(&a) == 42 && a.mantissa.limbs[0] == UINT32_C(0xa8000000));
    CHECK(bigFloatCopy(&b, &a) == 0);
    CHECK(bigFloatCopy(&b, &b) == 0);
    CHECK(float_value(&b) == 42);
    CHECK(bigFloatNormalize(&b) == 0 && float_value(&b) == 42);
    CHECK(bigFloatShiftLeft(&b, 3) == 0 && float_value(&b) == 336);
    CHECK(bigFloatShiftRight(&b, 3) == 0 && float_value(&b) == 42);
    bigFloatZero(&a);
    a.mantissa.size = 2;
    a.mantissa.limbs[0] = 123;
    a.mantissa.limbs[1] = UINT32_C(0x80000000);
    CHECK(bigFloatTruncate(&a, 0) == -2);
    CHECK(bigFloatTruncate(&a, 1) == 0);
    CHECK(a.mantissa.size == 1 && a.exp == 32 && float_value(&a) == ldexpl(1, 63));

    CHECK(bigFloatFromUint32(&a, 1000) == 0);
    CHECK(bigFloatFromUint32(&b, 250) == 0);
    CHECK(bigFloatCmpAbs(&a, &b) == 1 && bigFloatCmpAbs(&b, &a) == -1);
    CHECK(bigFloatCmpAbs(&a, &a) == 0);
    CHECK(bigFloatAdd(&r, &a, &b) == 0 && float_value(&r) == 1250);
    CHECK(bigFloatSub(&r, &a, &b) == 0 && float_value(&r) == 750);
    CHECK(bigFloatSub(&r, &b, &a) == 0 && float_value(&r) == -750);
    b.sign = -1;
    CHECK(bigFloatMul(&r, &a, &b) == 0 && float_value(&r) == -250000);
    CHECK(bigFloatAdd(&r, &a, &b) == 0 && float_value(&r) == 750);
    CHECK(bigFloatCopy(&b, &a) == 0);
    CHECK(bigFloatSub(&r, &a, &b) == 0 && float_value(&r) == 0);
    bigFloatZero(&b);
    CHECK(bigFloatMul(&r, &a, &b) == 0 && float_value(&r) == 0);
    CHECK(bigFloatAdd(&r, &a, &b) == 0 && float_value(&r) == 1000);
    CHECK(bigFloatReciprocal(&r, &b, 4) == -2);
    CHECK(bigFloatDiv(&r, &a, &b, 4) == -2);
    CHECK(bigFloatSqrt(&r, &b, 4) == 0 && float_value(&r) == 0);
    b.sign = -1;
    CHECK(bigFloatSqrt(&r, &b, 4) == -2);
    CHECK(bigFloatFromUint32(&a, 2) == 0);
    CHECK(bigFloatReciprocal(&r, &a, 4) == 0 && near(&r, 0.5L));
    CHECK(bigFloatFromUint32(&a, 7) == 0);
    CHECK(bigFloatReciprocal(&r, &a, 4) == 0 && near(&r, 1.0L / 7));
    CHECK(bigFloatFromUint32(&b, 22) == 0);
    CHECK(bigFloatDiv(&r, &b, &a, 4) == 0 && near(&r, 22.0L / 7));
    CHECK(bigFloatFromUint32(&a, 2) == 0);
    CHECK(bigFloatSqrt(&r, &a, 4) == 0 && near(&r, sqrtl(2)));
    CHECK(bigFloatFromUint32(&a, 144) == 0);
    CHECK(bigFloatSqrt(&r, &a, 4) == 0 && near(&r, 12));

    bigFloatZero(&b);
    CHECK(bigFloatCmpAbs(&a, &b) == 1);
    CHECK(bigFloatFromUint32(&a, 1) == 0);
    CHECK(bigFloatShiftLeft(&a, 32) == 0);
    CHECK(bigFloatFromUint32(&b, 2) == 0);
    CHECK(bigFloatCmpAbs(&a, &b) == 1);
    CHECK(bigFloatCmpAbs(&b, &a) == -1);
    CHECK(bigFloatSub(&r, &a, &b) == 0 && float_value(&r) == 4294967294.0L);
    CHECK(bigFloatSub(&r, &b, &a) == 0 && float_value(&r) == -4294967294.0L);
    bigFloatDestroy(&a);
    bigFloatDestroy(&b);
    bigFloatDestroy(&r);
}

static void magnitude_comparison(void) {
    BigFloat a = BIGFLOAT_INIT, b = BIGFLOAT_INIT, r = BIGFLOAT_INIT;
    bigFloatZero(&a);
    bigFloatZero(&b);
    CHECK(bigFloatCmpAbs(&a, &b) == 0);
    CHECK(bigFloatFromUint32(&a, 1) == 0);
    CHECK(bigFloatCmpAbs(&b, &a) == -1);

    /* Same value with different mantissa widths, including unnormalized seeds. */
    CHECK(bigFloatCopy(&b, &a) == 0);
    CHECK(bigIntShiftLeft(&b.mantissa, 32) == 0);
    b.exp -= 32;
    b.sign = -1;
    CHECK(bigFloatCmpAbs(&a, &b) == 0);
    CHECK(bigFloatAdd(&r, &a, &b) == 0 && float_value(&r) == 0);
    b.mantissa.limbs[0] = 1;
    CHECK(bigFloatCmpAbs(&a, &b) == -1 && bigFloatCmpAbs(&b, &a) == 1);
    CHECK(int_32ToBigInt(&b.mantissa, 1) == 0);
    b.exp = 0;
    CHECK(bigFloatCmpAbs(&a, &b) == 0);

    /* Full-width comparison must preserve even the lowest bit. */
    bigFloatZero(&b);
    CHECK(bigIntReserve(&b.mantissa, TEST_LIMBS) == 0);
    memset(b.mantissa.limbs, 0, TEST_LIMBS * sizeof(*b.mantissa.limbs));
    b.mantissa.size = TEST_LIMBS;
    b.mantissa.limbs[TEST_LIMBS - 1] = UINT32_C(0x80000000);
    b.exp = -(32 * TEST_LIMBS - 1);
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
    for(uint32_t i = 1; i <= 200; ++i) {
        CHECK(bigFloatFromUint32(&a, i * 7919) == 0);
        CHECK(bigFloatFromUint32(&b, i * 3571) == 0);
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
    bigFloatDestroy(&a);
    bigFloatDestroy(&b);
    bigFloatDestroy(&r);
}

static void full_capacity(void) {
    BigInt a = BIGINT_INIT, b = BIGINT_INIT, r = BIGINT_INIT;
    bigIntZero(&a);
    CHECK(bigIntReserve(&a, TEST_LIMBS) == 0);
    a.size = TEST_LIMBS / 2;
    for(size_t i = 0; i < a.size; ++i) {
        a.limbs[i] = UINT32_MAX;
    }
    CHECK(bigIntMulFFT(&r, &a, &a) == 0);
    CHECK(r.size == TEST_LIMBS);
    for(int i = 0; i < TEST_LIMBS; ++i) {
        uint32_t expected = i == 0                ? 1
                            : i < TEST_LIMBS / 2  ? 0
                            : i == TEST_LIMBS / 2 ? UINT32_MAX - 1
                                                  : UINT32_MAX;
        CHECK(r.limbs[i] == expected);
    }
    CHECK(bigIntReserve(&a, TEST_LIMBS) == 0);
    a.size = TEST_LIMBS;
    for(size_t i = 0; i < a.size; ++i) {
        a.limbs[i] = UINT32_MAX;
    }
    CHECK(int_32ToBigInt(&b, 1) == 0);
    CHECK(bigIntMulFFT(&a, &a, &b) == 0);
    CHECK(a.size == TEST_LIMBS);
    for(size_t i = 0; i < a.size; ++i) {
        CHECK(a.limbs[i] == UINT32_MAX);
    }
    CHECK(int_32ToBigInt(&r, 123) == 0);
    CHECK(bigIntMulFFT(&r, &a, &a) == 0 && r.size == 2 * TEST_LIMBS);
    for(size_t i = 0; i < a.size; ++i) {
        CHECK(a.limbs[i] == UINT32_MAX);
    }

    BigFloat x = BIGFLOAT_INIT, y = BIGFLOAT_INIT, z = BIGFLOAT_INIT;
    CHECK(bigFloatSetPrecision(&z, TEST_LIMBS) == 0);
    CHECK(bigFloatFromUint32(&x, 3) == 0);
    CHECK(bigFloatReciprocal(&x, &x, TEST_LIMBS) == 0);
    CHECK(x.mantissa.size == TEST_LIMBS && x.exp == -32 * TEST_LIMBS - 1);
    for(int i = 0; i < TEST_LIMBS; ++i) {
        CHECK(x.mantissa.limbs[i] == UINT32_C(0xaaaaaaaa));
    }
    CHECK(bigIntCopy(&x.mantissa, &a) == 0);
    x.exp = 0;
    x.sign = 1;
    CHECK(bigFloatCopy(&y, &x) == 0);
    CHECK(bigFloatMul(&z, &x, &y) == 0);
    CHECK(z.mantissa.size == TEST_LIMBS && z.exp == 32 * TEST_LIMBS);
    CHECK(z.mantissa.limbs[0] == UINT32_MAX - 1);
    for(int i = 1; i < TEST_LIMBS; ++i) {
        CHECK(z.mantissa.limbs[i] == UINT32_MAX);
    }
    CHECK(bigFloatAdd(&z, &x, &y) == 0 && z.exp == 1);
    CHECK(z.mantissa.size == TEST_LIMBS);
    for(int i = 0; i < TEST_LIMBS; ++i) {
        CHECK(z.mantissa.limbs[i] == UINT32_MAX);
    }
    y.mantissa.limbs[0]--;
    CHECK(bigFloatSub(&z, &x, &y) == 0 && float_value(&z) == 1);
    x.exp = -32 * TEST_LIMBS;
    CHECK(bigFloatSqrt(&x, &x, TEST_LIMBS) == 0);
    CHECK(x.mantissa.size == TEST_LIMBS && x.exp == -32 * TEST_LIMBS);
    for(int i = 0; i < TEST_LIMBS; ++i) {
        CHECK(x.mantissa.limbs[i] == UINT32_MAX);
    }
    bigIntDestroy(&a);
    bigIntDestroy(&b);
    bigIntDestroy(&r);
    bigFloatDestroy(&x);
    bigFloatDestroy(&y);
    bigFloatDestroy(&z);
}

static void transforms(void) {
    const int lengths[] = {1, 2, 3, 5, 17, 32};
    const long double tau = 2 * acosl(-1.0L);
    for(unsigned t = 0; t < sizeof(lengths) / sizeof(lengths[0]); ++t) {
        int n = lengths[t];
        for(int inverse = 0; inverse <= 1; ++inverse) {
            complexNum input[32], transformed[32];
            for(int i = 0; i < n; ++i) {
                input[i].re = (i * 7 % 11) - 5;
                input[i].im = (i * 3 % 7) - 3;
                transformed[i] = input[i];
            }
            CHECK(fft_arbitrary(transformed, n, inverse) == 0);
            for(int k = 0; k < n; ++k) {
                long double re = 0, im = 0;
                for(int j = 0; j < n; ++j) {
                    long double angle = (inverse ? tau : -tau) * j * k / n;
                    re += input[j].re * cosl(angle) - input[j].im * sinl(angle);
                    im += input[j].re * sinl(angle) + input[j].im * cosl(angle);
                }
                if(inverse) {
                    re /= n;
                    im /= n;
                }
                CHECK(fabsl(transformed[k].re - re) < 1e-9L);
                CHECK(fabsl(transformed[k].im - im) < 1e-9L);
            }
        }
    }
    /* Transform sizes relevant to large limb convolutions, plus Bluestein. */
    const int large_lengths[] = {4 * TEST_LIMBS, 4 * TEST_LIMBS - 1};
    for(unsigned t = 0; t < sizeof(large_lengths) / sizeof(large_lengths[0]); ++t) {
        int n = large_lengths[t];
        complexNum *x = calloc((size_t)n, sizeof(*x));
        CHECK(x != NULL);
        if(!x) { continue; }
        for(int i = 0; i < n; ++i) {
            x[i].re = (i * 17 % 101) - 50;
        }
        CHECK(fft_arbitrary(x, n, 0) == 0);
        CHECK(fft_arbitrary(x, n, 1) == 0);
        for(int i = 0; i < n; ++i) {
            CHECK(fabs(x[i].re - ((i * 17 % 101) - 50)) < 1e-8);
            CHECK(fabs(x[i].im) < 1e-8);
        }
        free(x);
    }
    complexNum x = {0, 0};
    CHECK(fft(NULL, 1, 0) == -1);
    CHECK(fft(&x, 0, 0) == -1);
    CHECK(fft(&x, 3, 0) == -1);
    CHECK(fft_arbitrary(NULL, 1, 0) == -1);
    CHECK(fft_arbitrary(&x, -1, 0) == -1);
    CHECK(fft_arbitrary(&x, INT_MAX, 0) == -1);
}

static void ownership_and_conversion(void) {
    BigInt a = BIGINT_INIT, b = BIGINT_INIT;
    CHECK(!a.size && !a.capacity && !a.limbs);
    CHECK(bigIntReserve(&a, 2000) == 0);
    size_t capacity = a.capacity;
    CHECK(int_32ToBigInt(&a, 42) == 0);
    CHECK(bigIntCopy(&b, &a) == 0 && a.limbs != b.limbs);
    CHECK(bigIntCopy(&a, &a) == 0);
    CHECK(bigIntAddUInt_32(&b, 1) == 0 && equals_u64(&a, 42));
    bigIntSwap(&a, &b);
    CHECK(equals_u64(&a, 43) && equals_u64(&b, 42));
    bigIntZero(&b);
    CHECK(!b.size && b.capacity == capacity);
    CHECK(bigIntReserve(&a, SIZE_MAX) == INT_MAX && equals_u64(&a, 43));
    CHECK(bigIntShiftLeft(&a, INT64_MAX) == INT_MAX && equals_u64(&a, 43));
    CHECK(bigIntShiftRight(&a, INT64_MIN) == INT_MAX && equals_u64(&a, 43));
    CHECK(bigIntShiftLeft(&a, INT64_MIN) == 0 && !a.size);
    CHECK(bigIntFromString(&a, "1000000000000000000000000001") == 0);
    char *text = NULL;
    CHECK(bigIntToString(&a, &text) == 0);
    CHECK(text && strcmp(text, "1000000000000000000000000001") == 0);
    CHECK(bigIntFromString(&b, text) == 0 && bigIntCmp(&a, &b) == 0);
    free(text);
    text = NULL;
    CHECK(bigIntFromString(&a, "12x") == -2 && bigIntCmp(&a, &b) == 0);
    CHECK(bigIntGetBit(&a, SIZE_MAX) == 0);
    bigIntZero(&b);
    CHECK(bigIntToString(&b, &text) == 0 && strcmp(text, "0") == 0);
    free(text);
    text = NULL;
    bigIntDestroy(&a);
    bigIntDestroy(&b);
    bigIntDestroy(&b);

    BigFloat x = BIGFLOAT_INIT, y = BIGFLOAT_INIT, z = BIGFLOAT_INIT;
    CHECK(bigFloatSetPrecision(&z, 1) == 0);
    CHECK(bigFloatFromUint32(&x, 1) == 0);
    CHECK(bigFloatCopy(&y, &x) == 0 && y.mantissa.limbs != x.mantissa.limbs);
    y.exp -= 1000;
    CHECK(bigFloatSub(&z, &x, &y) == 0);
    CHECK(z.precision == 1 && z.mantissa.size == 1 && z.mantissa.limbs[0] == UINT32_MAX &&
          z.exp == -32);
    /* Cancellation must retain input precision even with a narrow output. */
    CHECK(bigIntShiftLeft(&x.mantissa, 640) == 0);
    CHECK(bigFloatCopy(&y, &x) == 0);
    CHECK(bigIntAddUInt_32(&x.mantissa, 1) == 0);
    CHECK(bigFloatSub(&z, &x, &y) == 0 && float_value(&z) == ldexpl(1, x.exp));
    CHECK(bigFloatFromUint32(&x, 1999) == 0);
    CHECK(bigFloatFromUint32(&y, 2000) == 0);
    CHECK(bigFloatDiv(&z, &x, &y, 4) == 0);
    CHECK(bigFloatToString(&z, 2, &text) == 0 && strcmp(text, "1.00") == 0);
    free(text);
    text = NULL;
    CHECK(bigFloatFromUint32(&x, 1) == 0);
    x.exp -= 3;
    x.sign = -1; /* Exact -0.125: decimal ties away from zero. */
    CHECK(bigFloatToString(&x, 2, &text) == 0 && strcmp(text, "-0.13") == 0);
    free(text);
    text = NULL;
    x.exp = INT32_MIN;
    CHECK(bigFloatToString(&x, 3, &text) == 0 && strcmp(text, "-0.000") == 0);
    free(text);
    text = NULL;
    CHECK(bigFloatToString(&x, SIZE_MAX, &text) == INT_MAX && text == NULL);
    CHECK(bigFloatSetPrecision(&x, SIZE_MAX) == INT_MAX && x.exp == INT32_MIN);
    CHECK(bigFloatFromUint32(&x, 2) == 0);
    CHECK(bigFloatToString(&x, 0, &text) == 0 && strcmp(text, "2.") == 0);
    free(text);
    text = NULL;
    CHECK(bigFloatToString(&x, 11000, &text) == 0);
    CHECK(text && strlen(text) == 11002 && text[0] == '2' && text[11001] == '0');
    free(text);
    text = NULL;
    x.exp = INT32_MAX;
    CHECK(bigFloatCopy(&y, &x) == 0);
    CHECK(bigFloatCopy(&z, &x) == 0);
    CHECK(bigFloatMul(&x, &x, &y) == INT_MAX);
    CHECK(bigFloatCmpAbs(&x, &z) == 0);
    bigFloatDestroy(&x);
    bigFloatDestroy(&y);
    bigFloatDestroy(&z);
}

static void signed_integers(void) {
    BigInt a = BIGINT_INIT, b = BIGINT_INIT, out = BIGINT_INIT;
    const int64_t values[] = {-1000000, -33, -7, -1, 0, 1, 2, 7, 33, 1000000};
    int (*operations[])(BigInt *, const BigInt *, const BigInt *) = {bigIntAdd, bigIntSub,
                                                                     bigIntMul, bigIntMulFFT};
    for(size_t i = 0; i < sizeof(values) / sizeof(*values); ++i) {
        for(size_t j = 0; j < sizeof(values) / sizeof(*values); ++j) {
            int64_t av = values[i], bv = values[j];
            int64_t expected[] = {av + bv, av - bv, av * bv, av * bv};
            CHECK(bigIntFromInt64(&a, av) == 0);
            CHECK(bigIntFromInt64(&b, bv) == 0);
            CHECK(bigIntCmp(&a, &b) == ((av > bv) - (av < bv)));
            int64_t aa = av < 0 ? -av : av, ba = bv < 0 ? -bv : bv;
            CHECK(bigIntCmpAbs(&a, &b) == ((aa > ba) - (aa < ba)));
            for(size_t op = 0; op < sizeof(operations) / sizeof(*operations); ++op) {
                for(int alias = 0; alias < 3; ++alias) {
                    CHECK(bigIntFromInt64(&a, av) == 0);
                    CHECK(bigIntFromInt64(&b, bv) == 0);
                    BigInt *dest = alias == 1 ? &a : alias == 2 ? &b : &out;
                    CHECK(operations[op](dest, &a, &b) == 0);
                    CHECK(equals_i64(dest, expected[op]));
                    if(alias != 1) { CHECK(equals_i64(&a, av)); }
                    if(alias != 2) { CHECK(equals_i64(&b, bv)); }
                }
            }
        }
        CHECK(bigIntFromInt64(&a, values[i]) == 0);
        CHECK(bigIntAdd(&a, &a, &a) == 0 && equals_i64(&a, 2 * values[i]));
        CHECK(bigIntSub(&a, &a, &a) == 0 && equals_i64(&a, 0));
        bigIntNegate(&a);
        CHECK(equals_i64(&a, 0));
    }
    const int64_t dividends[] = {
        INT64_MIN, INT64_MAX, -INT64_C(4294967296), -INT64_C(4294967294), -7, -1, 0,
        1,         7,         INT64_C(4294967294)};
    const uint32_t divisors[] = {1, 2, 3, 13, UINT32_MAX};
    for(size_t i = 0; i < sizeof(dividends) / sizeof(*dividends); ++i) {
        for(size_t j = 0; j < sizeof(divisors) / sizeof(*divisors); ++j) {
            int64_t v = dividends[i], d = divisors[j];
            CHECK(bigIntFromInt64(&a, v) == 0 && equals_i64(&a, v));
            CHECK(bigIntModUInt32(&a, divisors[j]) == v % d && equals_i64(&a, v));
            CHECK(bigIntDivUInt32(&a, divisors[j]) == v % d && equals_i64(&a, v / d));
        }
    }
    CHECK(bigIntFromInt64(&a, -7) == 0);
    CHECK(bigIntDivUInt32(&a, 0) == INT64_MIN && equals_i64(&a, -7));
    CHECK(bigIntModUInt32(&a, 0) == INT64_MIN && equals_i64(&a, -7));
    CHECK(bigIntShiftRight(&a, 1) == 0 && equals_i64(&a, -3));
    CHECK(bigIntGetBit(&a, 0) == 1 && bigIntGetBit(&a, 1) == 1);
    CHECK(bigIntShiftLeft(&a, 33) == 0 && equals_i64(&a, -INT64_C(25769803776)));
    CHECK(bigIntShiftLeft(&a, -33) == 0 && equals_i64(&a, -3));
    CHECK(bigIntShiftRight(&a, -2) == 0 && equals_i64(&a, -12));
    CHECK(bigIntShiftRight(&a, 100) == 0 && equals_i64(&a, 0));
    CHECK(bigIntFromInt64(&a, -1) == 0);
    CHECK(bigIntShiftLeft(&a, INT64_MIN) == 0 && equals_i64(&a, 0));

    const int64_t negatives[] = {-1, -7, -INT64_C(4294967295), -INT64_C(4294967296)};
    for(size_t i = 0; i < sizeof(negatives) / sizeof(*negatives); ++i) {
        for(size_t j = 0; j < sizeof(divisors) / sizeof(*divisors); ++j) {
            CHECK(bigIntFromInt64(&a, negatives[i]) == 0);
            CHECK(bigIntAddUInt_32(&a, divisors[j]) == 0);
            CHECK(equals_i64(&a, negatives[i] + divisors[j]));
        }
    }
    CHECK(bigIntFromInt64(&a, -7) == 0);
    CHECK(bigIntMulUInt_32(&a, UINT32_MAX) == 0 && equals_i64(&a, -7 * (int64_t)UINT32_MAX));
    CHECK(bigIntMulUInt_32(&a, 0) == 0 && equals_i64(&a, 0));
    CHECK(bigIntFromString(&a, "-0000") == 0 && equals_i64(&a, 0));
    CHECK(bigIntFromString(&a, "+00042") == 0 && equals_i64(&a, 42));
    const char *invalid[] = {"-", "+", "--1", "+-1", "++1", "- 1", "1-2"};
    for(size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) {
        CHECK(bigIntFromString(&a, invalid[i]) == -2 && equals_i64(&a, 42));
    }
    CHECK(bigIntFromInt64(&a, INT64_MIN) == 0);
    char *text = NULL;
    CHECK(bigIntToString(&a, &text) == 0 && strcmp(text, "-9223372036854775808") == 0);
    CHECK(bigIntFromString(&b, text) == 0 && equals_i64(&b, INT64_MIN));
    free(text);
    text = NULL;
    bigIntNegate(&a);
    CHECK(equals_u64(&a, UINT64_C(1) << 63));
    CHECK(bigIntCopy(&out, &b) == 0 && equals_i64(&out, INT64_MIN));
    bigIntSwap(&a, &out);
    CHECK(equals_i64(&a, INT64_MIN) && equals_u64(&out, UINT64_C(1) << 63));
    bigIntZero(&a);
    CHECK(equals_i64(&a, 0));
    /* Negative values larger than the former fixed storage limit. */
    CHECK(bigIntFromInt64(&a, -1) == 0);
    CHECK(bigIntShiftLeft(&a, 32 * TEST_LIMBS) == 0 && a.sign == -1);
    CHECK(bigIntCopy(&b, &a) == 0);
    CHECK(bigIntToString(&a, &text) == 0 && text[0] == '-');
    CHECK(bigIntFromString(&out, text) == 0 && bigIntCmp(&out, &a) == 0);
    free(text);
    bigIntNegate(&b);
    CHECK(bigIntAdd(&a, &a, &b) == 0 && equals_i64(&a, 0));
    bigIntDestroy(&a);
    bigIntDestroy(&b);
    bigIntDestroy(&out);
}

int main(void) {
    integers();
    signed_integers();
    floats();
    magnitude_comparison();
    full_capacity();
    transforms();
    ownership_and_conversion();
    printf("%u checks, %u failures\n", checks, failures);
    return(failures ? EXIT_FAILURE : EXIT_SUCCESS);
}
