/* Exact multiplication oracle and failure injection. Link with --wrap for
 * fft/calloc/free; no test hooks or fault switches enter the shared library. */
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bignums.h"
#include "complexFFT.h"

static unsigned checks, failures;
#define CHECK(expr) do { \
    ++checks; \
    if (!(expr)) { \
        ++failures; \
        fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #expr); \
    } \
} while (0)

static int fft_calls, allocation_calls, live_allocations, largest_transform;
static int fail_fft_call, fail_allocation_call, corrupt_inverse;

int __real_fft(complexNum *x, int n, int inverse);
void *__real_calloc(size_t count, size_t size);
void __real_free(void *pointer);

int __wrap_fft(complexNum *x, int n, int inverse) {
    ++fft_calls;
    if (n > largest_transform) largest_transform = n;
    if (fft_calls == fail_fft_call) return -1;
    int status = __real_fft(x, n, inverse);
    if (status == 0 && inverse) {
        /* Corrupt both low and high coefficients, including values whose
         * conversion to an integer would be undefined without validation. */
        switch (corrupt_inverse) {
            case 1: x[0].re += 1; break;
            case 2: x[n / 2].re += 1; break;
            case 3: x[0].re = NAN; break;
            case 4: x[0].re = INFINITY; break;
            case 5: x[0].re = -INFINITY; break;
            case 6: x[0].re = 1e300; break;
            case 7: x[0].re = -1; break;
        }
    }
    return status;
}

void *__wrap_calloc(size_t count, size_t size) {
    ++allocation_calls;
    if (allocation_calls == fail_allocation_call) return NULL;
    void *pointer = __real_calloc(count, size);
    if (pointer) ++live_allocations;
    return pointer;
}

void __wrap_free(void *pointer) {
    if (pointer) --live_allocations;
    __real_free(pointer);
}

static void reset_calls(void) {
    CHECK(live_allocations == 0);
    fft_calls = allocation_calls = 0;
    fail_fft_call = fail_allocation_call = corrupt_inverse = 0;
}

/* Independent base-65536 diagonal convolution. Accumulate all contributions
 * before carrying; coefficients fit uint64_t even at maximum capacity. */
static int reference_product(BigInt *out, const BigInt *a, const BigInt *b) {
    uint64_t coefficients[4 * MAX_LIMBS] = {0};
    int da = 2 * a->size, db = 2 * b->size;
    for (int i = 0; i < da; ++i) {
        uint32_t av = (a->limbs[i / 2] >> (16 * (i % 2))) & 65535u;
        for (int j = 0; j < db; ++j) {
            uint32_t bv = (b->limbs[j / 2] >> (16 * (j % 2))) & 65535u;
            coefficients[i + j] += (uint64_t)av * bv;
        }
    }
    memset(out, 0, sizeof(*out));
    out->size = 1;
    uint64_t carry = 0;
    int overflow = 0;
    for (int i = 0; i < da + db; ++i) {
        uint64_t v = coefficients[i] + carry;
        uint32_t digit = (uint32_t)(v & 65535u);
        carry = v >> 16;
        if (i / 2 >= MAX_LIMBS) overflow |= digit != 0;
        else {
            out->limbs[i / 2] |= digit << (16 * (i % 2));
            if (digit) out->size = i / 2 + 1;
        }
    }
    CHECK(carry == 0);
    return overflow ? INT_MAX : 0;
}

static int equal(const BigInt *a, const BigInt *b) {
    return a->size == b->size &&
        memcmp(a->limbs, b->limbs, (size_t)a->size * sizeof(*a->limbs)) == 0;
}

static void product_case(const BigInt *a, const BigInt *b) {
    BigInt expected;
    int expected_status = reference_product(&expected, a, b);
    for (int alias = 0; alias < 3; ++alias) {
        BigInt acopy = *a, bcopy = *b, result;
        memset(&result, 0xa5, sizeof(result));
        BigInt *out = alias == 1 ? &acopy : alias == 2 ? &bcopy : &result;
        BigInt before = *out;
        reset_calls();
        int status = bigIntMulFFT(out, &acopy, &bcopy);
        CHECK(status == expected_status);
        if (expected_status == 0) CHECK(equal(out, &expected));
        else CHECK(memcmp(out, &before, sizeof(before)) == 0);
        CHECK(fft_calls == 3);
        CHECK(live_allocations == 0);
        if (alias != 1) CHECK(memcmp(&acopy, a, sizeof(acopy)) == 0);
        if (alias != 2) CHECK(memcmp(&bcopy, b, sizeof(bcopy)) == 0);
    }
}

static uint32_t random_word(void) {
    static uint32_t state = UINT32_C(0xf17c032b);
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static void fill(BigInt *a, int size, int pattern) {
    /* Inactive storage deliberately contains garbage. */
    memset(a, 0xa5, sizeof(*a));
    a->size = size;
    for (int i = 0; i < size; ++i) {
        switch (pattern) {
            case 0: a->limbs[i] = UINT32_MAX; break;
            case 1: a->limbs[i] = i % 2 ? UINT32_C(0xaaaaaaaa) : UINT32_C(0x55555555); break;
            case 2: a->limbs[i] = 0; break;
            default: a->limbs[i] = random_word(); break;
        }
    }
    a->limbs[size - 1] |= UINT32_C(0x80000000);
}

static void full_products(void) {
    const int sizes[][2] = {
        {1, 1}, {2, 3}, {15, 17}, {31, 32}, {63, 65}, {127, 129},
        {255, 257}, {MAX_LIMBS / 2, MAX_LIMBS / 2},
        {MAX_LIMBS / 2, MAX_LIMBS / 2 + 1},
        {MAX_LIMBS - 1, 1}, {MAX_LIMBS, 1}, {MAX_LIMBS, MAX_LIMBS}
    };
    BigInt a, b;
    for (unsigned i = 0; i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        for (int pattern = 0; pattern < 4; ++pattern) {
            fill(&a, sizes[i][0], pattern);
            fill(&b, sizes[i][1], pattern);
            product_case(&a, &b);
        }
    }
    for (int i = 0; i < 20; ++i) {
        int size = 1 + (int)(random_word() % (MAX_LIMBS - 1));
        fill(&a, size, 3);
        fill(&b, MAX_LIMBS - size, 3);
        product_case(&a, &b);
    }
    /* An 8192-point transform whose product still fits the public capacity. */
    fill(&a, MAX_LIMBS, 0);
    memset(&b, 0xa5, sizeof(b));
    b.size = 1;
    b.limbs[0] = 1;
    product_case(&a, &b);
    CHECK(largest_transform == 8 * MAX_LIMBS);

    BigInt expected;
    fill(&a, MAX_LIMBS / 2, 3);
    CHECK(reference_product(&expected, &a, &a) == 0);
    reset_calls();
    CHECK(bigIntMulFFT(&a, &a, &a) == 0);
    CHECK(equal(&a, &expected) && fft_calls == 3 && live_allocations == 0);

    /* Zero is the only product path that skips transforms and allocations. */
    memset(&b, 0, sizeof(b));
    b.size = 1;
    reset_calls();
    CHECK(bigIntMulFFT(&a, &b, &a) == 0);
    CHECK(a.size == 1 && a.limbs[0] == 0);
    CHECK(fft_calls == 0 && allocation_calls == 0 && live_allocations == 0);
}

static void injected_failures(void) {
    BigInt a, b, expected;
    fill(&a, MAX_LIMBS / 2, 3);
    fill(&b, MAX_LIMBS / 2, 3);
    CHECK(reference_product(&expected, &a, &b) == 0);
    for (int corruption = 1; corruption <= 7; ++corruption) {
        for (int alias = 0; alias < 3; ++alias) {
            BigInt acopy = a, bcopy = b, result;
            BigInt *out = alias == 1 ? &acopy : alias == 2 ? &bcopy : &result;
            reset_calls();
            corrupt_inverse = corruption;
            CHECK(bigIntMulFFT(out, &acopy, &bcopy) == 0);
            CHECK(equal(out, &expected));
            CHECK(fft_calls == 3 && live_allocations == 0);
        }
    }
    for (int failure = 1; failure <= 7; ++failure) {
        for (int alias = 0; alias < 3; ++alias) {
            BigInt acopy = a, bcopy = b, result;
            memset(&result, 0xa5, sizeof(result));
            BigInt *out = alias == 1 ? &acopy : alias == 2 ? &bcopy : &result;
            BigInt before = *out;
            reset_calls();
            if (failure <= 4) fail_allocation_call = failure;
            else fail_fft_call = failure - 4;
            CHECK(bigIntMulFFT(out, &acopy, &bcopy) == -1);
            CHECK(memcmp(out, &before, sizeof(before)) == 0);
            CHECK(memcmp(&acopy, &a, sizeof(a)) == 0);
            CHECK(memcmp(&bcopy, &b, sizeof(b)) == 0);
            CHECK(live_allocations == 0);
        }
    }
    /* Corrupted FFT output cannot hide overflow or overwrite an aliased input. */
    fill(&a, MAX_LIMBS, 0);
    BigInt before = a;
    reset_calls();
    corrupt_inverse = 3;
    CHECK(bigIntMulFFT(&a, &a, &a) == INT_MAX);
    CHECK(memcmp(&a, &before, sizeof(a)) == 0 && live_allocations == 0);
}

int main(void) {
    full_products();
    injected_failures();
    printf("%u FFT multiplication checks, %u failures\n", checks, failures);
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
