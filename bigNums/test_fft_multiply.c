/* Exact multiplication oracle and failure injection. Link with --wrap for
 * fft/malloc/calloc/realloc/free; no test hooks or fault switches enter the shared library. */
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bignums.h"
#include "complexFFT.h"

static unsigned checks, failures;
#define CHECK(expr)                                                                                \
    do {                                                                                           \
        ++checks;                                                                                  \
        if(!(expr)) {                                                                              \
            ++failures;                                                                            \
            fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr);                                \
        }                                                                                          \
    } while(0)

static int fft_calls, allocation_calls, live_allocations, largest_transform;
static int fail_fft_call, fail_allocation_call, corrupt_inverse;

int __real_fft(complexNum *x, int n, int inverse);
void *__real_malloc(size_t size);
void *__real_realloc(void *pointer, size_t size);
void *__real_calloc(size_t count, size_t size);
void __real_free(void *pointer);

int __wrap_fft(complexNum *x, int n, int inverse) {
    ++fft_calls;
    if(n > largest_transform) { largest_transform = n; }
    if(fft_calls == fail_fft_call) { return(-1); }
    int status = __real_fft(x, n, inverse);
    if(status == 0 && inverse) {
        /* Corrupt both low and high coefficients, including values whose
         * conversion to an integer would be undefined without validation. */
        switch(corrupt_inverse) {
        case 1:
            x[0].re += 1;
            break;
        case 2:
            x[n / 2].re += 1;
            break;
        case 3:
            x[0].re = NAN;
            break;
        case 4:
            x[0].re = INFINITY;
            break;
        case 5:
            x[0].re = -INFINITY;
            break;
        case 6:
            x[0].re = 1e300;
            break;
        case 7:
            x[0].re = -1;
            break;
        }
    }
    return(status);
}

void *__wrap_calloc(size_t count, size_t size) {
    ++allocation_calls;
    if(allocation_calls == fail_allocation_call) { return(NULL); }
    void *pointer = __real_calloc(count, size);
    if(pointer) { ++live_allocations; }
    return(pointer);
}

void __wrap_free(void *pointer) {
    if(pointer) { --live_allocations; }
    __real_free(pointer);
}

void *__wrap_malloc(size_t size) {
    ++allocation_calls;
    if(allocation_calls == fail_allocation_call) { return(NULL); }
    void *p = __real_malloc(size);
    if(p) { ++live_allocations; }
    return(p);
}
void *__wrap_realloc(void *pointer, size_t size) {
    ++allocation_calls;
    if(allocation_calls == fail_allocation_call) { return(NULL); }
    int fresh = pointer == NULL;
    void *p = __real_realloc(pointer, size);
    if(p && fresh) { ++live_allocations; }
    return(p);
}
static void reset_calls(void) {
    fft_calls = allocation_calls = largest_transform = 0;
    fail_fft_call = fail_allocation_call = corrupt_inverse = 0;
}
static void require(int rc) {
    if(rc) {
        fprintf(stderr, "Fixture allocation failed: %d\n", rc);
        exit(1);
    }
}
static int equal(const BigInt *a, const BigInt *b) {
    return(a->sign == b->sign && a->size == b->size &&
           (!a->size || memcmp(a->limbs, b->limbs, a->size * sizeof(*a->limbs)) == 0));
}
static int float_equal(const BigFloat *a, const BigFloat *b) {
    return(equal(&a->mantissa, &b->mantissa) && a->exp == b->exp && a->sign == b->sign &&
           a->precision == b->precision);
}
static uint32_t random_word(void) {
    static uint32_t state = UINT32_C(0xf17c032b);
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return(state);
}
static void fill(BigInt *a, size_t size, int pattern) {
    require(bigIntReserve(a, size));
    memset(a->limbs, 0xa5, a->capacity * sizeof(*a->limbs));
    a->size = size;
    a->sign = 1;
    for(size_t i = 0; i < size; ++i) {
        switch(pattern) {
        case 0:
            a->limbs[i] = UINT32_MAX;
            break;
        case 1:
            a->limbs[i] = i % 2 ? UINT32_C(0xaaaaaaaa) : UINT32_C(0x55555555);
            break;
        case 2:
            a->limbs[i] = 0;
            break;
        default:
            a->limbs[i] = random_word();
            break;
        }
    }
    if(size) { a->limbs[size - 1] |= UINT32_C(0x80000000); }
}
/* Independent diagonal convolution in base 65536. Test sizes keep each
 * coefficient well within uint64_t; this is not a general library kernel. */
static void reference_product(BigInt *out, const BigInt *a, const BigInt *b) {
    size_t da = 2 * a->size, db = 2 * b->size;
    uint64_t *coefficients = calloc(da + db, sizeof(*coefficients));
    if(!coefficients) { exit(1); }
    for(size_t i = 0; i < da; ++i) {
        for(size_t j = 0; j < db; ++j) {
            coefficients[i + j] += (uint64_t)((a->limbs[i / 2] >> (16 * (i % 2))) & 65535u) *
                                   ((b->limbs[j / 2] >> (16 * (j % 2))) & 65535u);
        }
    }
    require(bigIntReserve(out, a->size + b->size));
    memset(out->limbs, 0, (a->size + b->size) * sizeof(*out->limbs));
    out->size = 0;
    uint64_t carry = 0;
    for(size_t i = 0; i < da + db; ++i) {
        uint64_t v = coefficients[i] + carry;
        uint32_t digit = (uint32_t)(v & 65535u);
        carry = v >> 16;
        out->limbs[i / 2] |= digit << (16 * (i % 2));
        if(digit) { out->size = i / 2 + 1; }
    }
    CHECK(carry == 0);
    out->sign = out->size ? a->sign * b->sign : 1;
    free(coefficients);
}
static void product_case(const BigInt *a, const BigInt *b, int transforms) {
    BigInt expected = BIGINT_INIT;
    reference_product(&expected, a, b);
    for(int alias = 0; alias < 3; ++alias) {
        BigInt acopy = BIGINT_INIT, bcopy = BIGINT_INIT, result = BIGINT_INIT;
        require(bigIntCopy(&acopy, a));
        require(bigIntCopy(&bcopy, b));
        require(int_32ToBigInt(&result, 123));
        BigInt *out = alias == 1 ? &acopy : alias == 2 ? &bcopy : &result;
        reset_calls();
        CHECK(bigIntMulFFT(out, &acopy, &bcopy) == 0);
        CHECK(equal(out, &expected));
        CHECK(fft_calls == transforms);
        if(alias != 1) { CHECK(equal(&acopy, a)); }
        if(alias != 2) { CHECK(equal(&bcopy, b)); }
        bigIntDestroy(&acopy);
        bigIntDestroy(&bcopy);
        bigIntDestroy(&result);
    }
    bigIntDestroy(&expected);
}
static void full_products(void) {
    const size_t sizes[][2] = {{1, 1},     {2, 3},       {15, 17},    {31, 32},   {63, 65},
                               {127, 129}, {255, 257},   {512, 512},  {512, 513}, {1023, 1},
                               {1024, 1},  {1024, 1024}, {1536, 1537}};
    BigInt a = BIGINT_INIT, b = BIGINT_INIT, expected = BIGINT_INIT;
    for(size_t i = 0; i < sizeof(sizes) / sizeof(*sizes); ++i) {
        for(int pattern = 0; pattern < 4; ++pattern) {
            fill(&a, sizes[i][0], pattern);
            fill(&b, sizes[i][1], pattern);
            a.sign = pattern & 1 ? -1 : 1;
            b.sign = pattern & 2 ? -1 : 1;
            product_case(&a, &b, 3);
        }
    }
    for(int i = 0; i < 20; ++i) {
        size_t size = 1 + random_word() % 2047;
        fill(&a, size, 3);
        fill(&b, 2048 - size, 3);
        product_case(&a, &b, 3);
    }
    /* Both sides of the single-modulus coefficient bound. */
    fill(&a, 3837, 0);
    fill(&b, 3837, 0);
    a.sign = -1;
    product_case(&a, &b, 3);
    fill(&a, 3838, 0);
    fill(&b, 3838, 0);
    b.sign = -1;
    product_case(&a, &b, 0);
    fill(&a, 4096, 3);
    fill(&b, 4096, 3);
    a.sign = b.sign = -1;
    product_case(&a, &b, 0);
    fill(&a, 512, 3);
    reference_product(&expected, &a, &a);
    CHECK(bigIntMulFFT(&a, &a, &a) == 0 && equal(&a, &expected));
    bigIntZero(&b);
    reset_calls();
    CHECK(bigIntMulFFT(&a, &b, &a) == 0 && !a.size);
    CHECK(fft_calls == 0 && allocation_calls == 0);
    /* A very unbalanced product crosses the transform-length bound without
     * crossing the coefficient bound. Only the exact fallback should run. */
    size_t long_size = (size_t)1 << 21;
    require(bigIntReserve(&a, long_size));
    memset(a.limbs, 0, long_size * sizeof(*a.limbs));
    a.size = long_size;
    a.limbs[long_size - 1] = 1;
    require(int_32ToBigInt(&b, 1));
    reset_calls();
    CHECK(bigIntMulFFT(&expected, &a, &b) == 0 && equal(&expected, &a));
    CHECK(fft_calls == 0);
    bigIntDestroy(&a);
    bigIntDestroy(&b);
    bigIntDestroy(&expected);
    CHECK(live_allocations == 0);
}
static void transform_failures(void) {
    BigInt a = BIGINT_INIT, b = BIGINT_INIT, expected = BIGINT_INIT;
    fill(&a, 512, 3);
    fill(&b, 512, 3);
    a.sign = -1;
    reference_product(&expected, &a, &b);
    for(int mode = 1; mode <= 10; ++mode) {
        for(int alias = 0; alias < 3; ++alias) {
            BigInt ac = BIGINT_INIT, bc = BIGINT_INIT, r = BIGINT_INIT;
            require(bigIntCopy(&ac, &a));
            require(bigIntCopy(&bc, &b));
            require(int_32ToBigInt(&r, 123));
            BigInt *out = alias == 1 ? &ac : alias == 2 ? &bc : &r;
            BigInt before = BIGINT_INIT;
            require(bigIntCopy(&before, out));
            int live = live_allocations;
            reset_calls();
            if(mode <= 7) {
                corrupt_inverse = mode;
            } else {
                fail_fft_call = mode - 7;
            }
            int rc = bigIntMulFFT(out, &ac, &bc);
            CHECK(rc == (mode <= 7 ? 0 : -1));
            CHECK(equal(out, mode <= 7 ? &expected : &before));
            CHECK(live_allocations == live);
            if(alias != 1 || mode > 7) { CHECK(equal(&ac, &a)); }
            if(alias != 2 || mode > 7) { CHECK(equal(&bc, &b)); }
            reset_calls();
            bigIntDestroy(&ac);
            bigIntDestroy(&bc);
            bigIntDestroy(&r);
            bigIntDestroy(&before);
        }
    }
    bigIntDestroy(&a);
    bigIntDestroy(&b);
    bigIntDestroy(&expected);
    CHECK(live_allocations == 0);
}
/* Fail each allocation in turn, including constructors, copying, scratch
 * growth, conversion and final packing. Repeat with either aliased input. */
static int allocation_case(int operation, int alias, int signs, int failure) {
    BigInt a = BIGINT_INIT, b = BIGINT_INIT, r = BIGINT_INIT;
    BigInt sa = BIGINT_INIT, sb = BIGINT_INIT, sr = BIGINT_INIT;
    BigFloat x = BIGFLOAT_INIT, y = BIGFLOAT_INIT, z = BIGFLOAT_INIT;
    BigFloat sx = BIGFLOAT_INIT, sy = BIGFLOAT_INIT, sz = BIGFLOAT_INIT;
    fill(&a, 8, 0);
    fill(&b, 7, 3);
    fill(&r, 8, 0);
    a.sign = signs & 1 ? -1 : 1;
    b.sign = signs & 2 ? -1 : 1;
    r.sign = -1;
    require(bigIntCopy(&sa, &a));
    require(bigIntCopy(&sb, &b));
    require(bigIntCopy(&sr, &r));
    require(bigIntCopy(&x.mantissa, &a));
    require(bigIntCopy(&y.mantissa, &b));
    x.mantissa.sign = y.mantissa.sign = 1;
    x.exp = -220;
    y.exp = -219;
    x.precision = y.precision = z.precision = 4;
    require(bigFloatFromUint32(&z, 123));
    require(bigFloatCopy(&sx, &x));
    require(bigFloatCopy(&sy, &y));
    require(bigFloatCopy(&sz, &z));
    BigInt *out = alias == 1 ? &a : alias == 2 ? &b : &r;
    BigFloat *fo = alias == 1 ? &x : alias == 2 ? &y : &z;
    char *text = NULL;
    int live = live_allocations;
    reset_calls();
    fail_allocation_call = failure;
    int rc = 0;
    switch(operation) {
    case 0:
        rc = bigIntReserve(out, out->capacity + 1);
        break;
    case 1:
        rc = bigIntCopy(out, &a);
        break;
    case 2:
        rc = bigIntAddUInt_32(out, 1);
        break;
    case 3:
        rc = bigIntMulUInt_32(out, UINT32_MAX);
        break;
    case 4:
        rc = bigIntShiftLeft(out, 65);
        break;
    case 5:
        rc = bigIntSub(out, &a, &b);
        break;
    case 6:
        rc = bigIntFromString(out, "-123456789012345678901234567890123456789012345678901234567890");
        break;
    case 7:
        rc = bigIntFactorial(out, 200);
        break;
    case 8:
        rc = bigIntMul(out, &a, &b);
        break;
    case 9:
        rc = bigIntMulFFT(out, &a, &b);
        break;
    case 10:
        rc = bigIntToString(&a, &text);
        break;
    case 11:
        rc = bigFloatCopy(fo, &x);
        break;
    case 12:
        rc = bigFloatFromUint32(fo, 42);
        break;
    case 13:
        rc = bigFloatSetPrecision(fo, 2);
        break;
    case 14:
        rc = bigFloatNormalize(fo);
        break;
    case 15:
        rc = bigFloatShiftLeft(fo, 65);
        break;
    case 16:
        rc = bigFloatShiftRight(fo, 3);
        break;
    case 17:
        rc = bigFloatAdd(fo, &x, &y);
        break;
    case 18:
        rc = bigFloatSub(fo, &x, &y);
        break;
    case 19:
        rc = bigFloatMul(fo, &x, &y);
        break;
    case 20:
        rc = bigFloatDiv(fo, &x, &y, 4);
        break;
    case 21:
        rc = bigFloatReciprocal(fo, &x, 4);
        break;
    case 22:
        rc = bigFloatSqrt(fo, &x, 4);
        break;
    case 23:
        rc = bigFloatToString(&x, 40, &text);
        break;
    case 24:
        rc = bigIntAdd(out, &a, &b);
        break;
    }
    int calls = allocation_calls;
    reset_calls();
    if(failure) {
        if(rc != -1) {
            fprintf(stderr, "OOM case op=%d alias=%d allocation=%d rc=%d\n", operation, alias,
                    failure, rc);
        }
        CHECK(rc == -1);
        CHECK(equal(&a, &sa) && equal(&b, &sb) && equal(&r, &sr));
        CHECK(float_equal(&x, &sx) && float_equal(&y, &sy) && float_equal(&z, &sz));
        CHECK(text == NULL && live_allocations == live);
    } else {
        CHECK(rc == 0);
    }
    free(text);
    bigIntDestroy(&a);
    bigIntDestroy(&b);
    bigIntDestroy(&r);
    bigIntDestroy(&sa);
    bigIntDestroy(&sb);
    bigIntDestroy(&sr);
    bigFloatDestroy(&x);
    bigFloatDestroy(&y);
    bigFloatDestroy(&z);
    bigFloatDestroy(&sx);
    bigFloatDestroy(&sy);
    bigFloatDestroy(&sz);
    CHECK(live_allocations == 0);
    return(calls);
}
static void allocation_failures(void) {
    for(int operation = 0; operation < 25; ++operation) {
        for(int alias = 0; alias < 3; ++alias) {
            for(int signs = 0; signs < (operation < 11 || operation == 24 ? 4 : 1); ++signs) {
                int calls = allocation_case(operation, alias, signs, 0);
                for(int failure = 1; failure <= calls; ++failure) {
                    (void)allocation_case(operation, alias, signs, failure);
                }
            }
        }
    }
    /* Allocation-free initialization/zero/destruction and first scalar set. */
    BigInt a;
    reset_calls();
    fail_allocation_call = 1;
    bigIntInit(&a);
    bigIntZero(&a);
    CHECK(int_32ToBigInt(&a, 42) == -1 && !a.size && !a.limbs);
    bigIntDestroy(&a);
    bigIntDestroy(&a);
    reset_calls();
    fail_allocation_call = 1;
    CHECK(bigIntFromInt64(&a, INT64_MIN) == -1 && !a.size && a.sign == 1 && !a.limbs);
    bigIntDestroy(&a);
    CHECK(live_allocations == 0);
    reset_calls();
}
int main(void) {
    full_products();
    transform_failures();
    allocation_failures();
    printf("%u multiplication/ownership/failure checks, %u failures\n", checks, failures);
    return(failures ? EXIT_FAILURE : EXIT_SUCCESS);
}
