/* Exact integer multiplication backends. */
#include "bignums_internal.h"
#include "complexFFT.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MUL_NTT_PRIME UINT32_C(998244353)

static int signed_schoolbook(BigInt *out, const BigInt *a, const BigInt *b) {
    int sign = a->sign * b->sign; /* Capture before replacing an aliased input. */
    int rc = bn_mul_schoolbook(out, a, b);
    if(!rc && out->size) { out->sign = sign; }
    return(rc);
}

static uint32_t mul_mod_power(uint32_t base, uint32_t exponent) {
    uint32_t result = 1;
    for(; exponent; exponent >>= 1) {
        if(exponent & 1) { result = (uint32_t)((uint64_t)result * base % MUL_NTT_PRIME); }
        base = (uint32_t)((uint64_t)base * base % MUL_NTT_PRIME);
    }
    return(result);
}

/* Private radix-2 transform, with n a power of two no greater than 2^23.
 * All residues stay below the prime; products fit uint64_t and sums uint32_t. */
static void mul_ntt(uint32_t *values, int n, int inverse) {
    for(int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        while(j & bit) {
            j ^= bit;
            bit >>= 1;
        }
        j ^= bit;
        if(i < j) {
            uint32_t swap = values[i];
            values[i] = values[j];
            values[j] = swap;
        }
    }
    for(int width = 2; width <= n; width *= 2) {
        uint32_t exponent = (MUL_NTT_PRIME - 1) / (uint32_t)width;
        if(inverse) { exponent = MUL_NTT_PRIME - 1 - exponent; }
        uint32_t step = mul_mod_power(3, exponent);
        for(int start = 0; start < n; start += width) {
            uint32_t phase = 1;
            for(int j = 0; j < width / 2; ++j) {
                uint32_t even = values[start + j];
                uint32_t odd =
                    (uint32_t)((uint64_t)phase * values[start + j + width / 2] % MUL_NTT_PRIME);
                uint32_t sum = even + odd;
                values[start + j] = sum >= MUL_NTT_PRIME ? sum - MUL_NTT_PRIME : sum;
                values[start + j + width / 2] =
                    even >= odd ? even - odd : even + MUL_NTT_PRIME - odd;
                phase = (uint32_t)((uint64_t)phase * step % MUL_NTT_PRIME);
            }
        }
    }
    if(inverse) {
        uint32_t scale = mul_mod_power((uint32_t)n, MUL_NTT_PRIME - 2);
        for(int i = 0; i < n; ++i) {
            values[i] = (uint32_t)((uint64_t)values[i] * scale % MUL_NTT_PRIME);
        }
    }
}

/* FFT convolution with exact coefficient certification. Recover using the NTT
 * coefficients if floating-point reconstruction fails. Both paths have
 * O(n log n) cost inside the supported transform range. */
int bigIntMulFFT(BigInt *result, const BigInt *a, const BigInt *b) {
    if(!a->size || !b->size) {
        bigIntZero(result);
        return(0);
    }
    if(a->size > BN_MAX_LIMBS - b->size) { return(INT_MAX); }
    size_t smaller = a->size < b->size ? a->size : b->size;
    /* The modulus must exceed every coefficient, and n must divide p-1.
     * Check before narrowing sizes or doubling transform lengths. */
    if(smaller > (MUL_NTT_PRIME - 1) / (4u * 255u * 255u) || a->size + b->size > (1u << 21)) {
        return(signed_schoolbook(result, a, b));
    }
    int digits_a = (int)(4 * a->size), digits_b = (int)(4 * b->size);
    int length = digits_a + digits_b - 1, n = 1;
    while(n < length) {
        n *= 2;
    }
    complexNum *fa = calloc((size_t)n, sizeof(*fa));
    complexNum *fb = calloc((size_t)n, sizeof(*fb));
    uint32_t *na = calloc((size_t)n, sizeof(*na));
    uint32_t *nb = calloc((size_t)n, sizeof(*nb));
    BigInt packed = BIGINT_INIT;
    int status = -1;
    if(!fa || !fb || !na || !nb) { goto done; }
    for(int i = 0; i < digits_a; ++i) {
        uint32_t digit = (a->limbs[i / 4] >> (8 * (i % 4))) & 255u;
        fa[i].re = digit;
        na[i] = digit;
    }
    for(int i = 0; i < digits_b; ++i) {
        uint32_t digit = (b->limbs[i / 4] >> (8 * (i % 4))) & 255u;
        fb[i].re = digit;
        nb[i] = digit;
    }
    if(fft(fa, n, 0) != 0 || fft(fb, n, 0) != 0) { goto done; }
    for(int i = 0; i < n; ++i) {
        double real = fa[i].re * fb[i].re - fa[i].im * fb[i].im;
        double imaginary = fa[i].re * fb[i].im + fa[i].im * fb[i].re;
        fa[i].re = real;
        fa[i].im = imaginary;
    }
    if(fft(fa, n, 1) != 0) { goto done; }

    mul_ntt(na, n, 0);
    mul_ntt(nb, n, 0);
    for(int i = 0; i < n; ++i) {
        na[i] = (uint32_t)((uint64_t)na[i] * nb[i] % MUL_NTT_PRIME);
    }
    mul_ntt(na, n, 1);
    int verified = 1;
    for(int i = 0; i < length; ++i) {
        /* Compare in double before any integer cast. This rejects NaN,
         * infinity, and out-of-range values without undefined conversion. */
        fa[i].re = floor(fa[i].re + 0.5);
        if(!isfinite(fa[i].re) || fa[i].re != (double)na[i]) { verified = 0; }
    }
    status = bigIntReserve(&packed, a->size + b->size);
    if(status) { goto done; }
    memset(packed.limbs, 0, (a->size + b->size) * sizeof(*packed.limbs));
    uint64_t carry = 0;
    for(int i = 0; i <= length; ++i) {
        uint32_t coefficient = i == length ? 0 : verified ? (uint32_t)fa[i].re : na[i];
        carry += coefficient;
        uint32_t digit = (uint32_t)(carry & 255u);
        carry >>= 8;
        packed.limbs[i / 4] |= digit << (8 * (i % 4));
        if(digit != 0) { packed.size = (size_t)i / 4 + 1; }
    }
    /* The product of digits_a and digits_b base-256 integers needs at most
     * digits_a+digits_b digits, so the extra iteration consumes all carry. */
    packed.sign = packed.size ? a->sign * b->sign : 1;
    bigIntSwap(result, &packed);
    status = 0;
done:
    bigIntDestroy(&packed);
    free(fa);
    free(fb);
    free(na);
    free(nb);
    return(status);
}

int bigIntMul(BigInt *result, const BigInt *a, const BigInt *b) {
    if(a->size < 32 || b->size < 32) { return(signed_schoolbook(result, a, b)); }
    return(bigIntMulFFT(result, a, b));
}
