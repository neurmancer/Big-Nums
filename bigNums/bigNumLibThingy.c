/*
        Sup? this will have less commentary since this is more like a project side-quest for Tupper's self-referantial formula bullet-point
        Idea is simple make C stop bitching about big nums

        Important shit to know: 
        I'll be using Little-endian to store shit to make carry propagation natural
        Integer products use FFT with exact coefficient verification


        Hii...I've returned back (next day and implemented big floats cuz I need them for Ramanujan fuckery)
        This one is a side-quest so I think I documented it good enough
*/

#include <string.h>     //For memset
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include "bignums.h"
#include "complexFFT.h"

// Each 32-bit limb needs at most 10 decimal digits, plus one terminator.
#define DECIMAL_BUFFER_SIZE (MAX_LIMBS * 10 + 1)


static int clz32(uint32_t x)
{
    if (x == 0){ return 32; }

#if defined(__GNUC__) || defined(__clang__)
    return(__builtin_clz(x));
#else
    int n = 0;

    while ((x & 0x80000000u) == 0)
    {
        n++;
        x <<= 1;
    }

    return(n);
#endif
}


void bigIntZero(BigInt *a) {
    memset(a->limbs, 0, sizeof(a->limbs));
    a->size = 1;            // value 0 represented as 1 limb
}

void int_32ToBigInt(BigInt *a, uint32_t val) {
    bigIntZero(a);
    a->limbs[0] = val;
    a->size = 1;
}


void printBigInt(const BigInt *a)
{
    if (a->size == 1 && a->limbs[0] == 0) {
        printf("0");
        return;
    }

    BigInt tmp = *a;

    char digits[DECIMAL_BUFFER_SIZE];
    int pos = sizeof(digits) - 1;
    digits[pos] = '\0';

    while (!(tmp.size == 1 && tmp.limbs[0] == 0)) {
        uint32_t rem = bigIntDivUInt32(&tmp, 10);
        digits[--pos] = (char)('0' + rem);
    }
    printf("%s", &digits[pos]);
}

int bigIntAddUInt_32(BigInt *a, uint32_t b) {
    uint64_t carry = b;
    for (int i = 0; carry > 0 && i < a->size; i++) {
        carry += a->limbs[i];
        a->limbs[i] = (uint32_t)carry;
        carry >>= 32;
    }
    // Append the carry without reading inactive (possibly stale) limbs.
    if (carry) {
        if (a->size < MAX_LIMBS) {
            a->limbs[a->size] = (uint32_t)carry;
            a->size++;
        }
        else {
            return(INT_MAX);     //Overflow check
        }
    }
    return(0);
}


// a *= b, b is a 32-bit integer
int bigIntMulUInt_32(BigInt *a, uint32_t b) {
    if (b == 0) {
        bigIntZero(a);
        return 0;
    }
    uint64_t carry = 0;
    for (int i = 0; i < a->size; i++) {
        carry += (uint64_t)a->limbs[i] * b;
        a->limbs[i] = (uint32_t)carry;
        carry >>= 32;
    }
    while (carry > 0) {
        if (a->size < MAX_LIMBS) {        
            a->limbs[a->size++] = (uint32_t)carry;
            carry >>= 32;
        }
        else {
            return(INT_MAX); //Overflow check
        }
    }
    return(0);
}


// a = a / divisor; return remainder


/**
 * Compute n! for 0 ≤ n ≤ ... (capacity limited by MAX_LIMBS).
 * result = n! (n factorial).
 * Returns 0 on success, INT_MAX if the result needs more than MAX_LIMBS limbs.
 */
int bigIntFactorial(BigInt *result, uint32_t n) {
    // 0! = 1! = 1
    if (n <= 1) {
        int_32ToBigInt(result, 1);
        return(0);
    }

    int_32ToBigInt(result, 1);
    for (uint32_t i = 2; i <= n; i++) {
        int rc = bigIntMulUInt_32(result, i);
        if (rc != 0) {                     // overflow
            return(INT_MAX);
        }
    }
    return(0);
}

int bigIntFromString(BigInt *a, const char *dec_str) {
    bigIntZero(a);
    while (*dec_str) {
        if (*dec_str < '0' || *dec_str > '9'){ return (-1); }

        int rc = bigIntMulUInt_32(a, 10);
        if (rc){ return (rc); }

        rc = bigIntAddUInt_32(a, (uint32_t)(*dec_str - '0'));
        if (rc){ return(rc); }

        dec_str++;
    }
    return(0);
}


uint32_t bigIntModUInt32(const BigInt *a, uint32_t divisor) {
    uint64_t remainder = 0;
    if (divisor == 0) { return(INT_MAX); }
    
    for (int i = a->size - 1; i >= 0; i--) {
        remainder = (remainder << 32) | a->limbs[i];
        remainder %= divisor;
    }
    return((uint32_t)remainder);
}

uint32_t bigIntDivUInt32(BigInt *a, uint32_t divisor) {
    uint64_t remainder = 0;
    if (divisor == 0) { return(INT_MAX); }
    for (int i = a->size - 1; i >= 0; i--) {
        remainder = (remainder << 32) | a->limbs[i];
        a->limbs[i] = (uint32_t)(remainder / divisor);
        remainder %= divisor;
    }
    // trim leading zeros (I mean...please do)
    while (a->size > 1 && a->limbs[a->size-1] == 0)
        a->size--;
    return (uint32_t)remainder;
}


int bigIntGetBit(const BigInt *a, int bit_index) {
    if (bit_index < 0) return 0;
    int limb = bit_index / 32;
    int bit  = bit_index % 32;
    if (limb >= a->size) return 0;
    return (a->limbs[limb] >> bit) & 1;
}

/* Compare a and b. Returns -1 if a < b, 0 if a == b, 1 if a > b. don't expect better API guidline than that */
int bigIntCmp(const BigInt *a, const BigInt *b) {
    if (a->size != b->size)
        return((a->size > b->size) ? 1 : -1);

    for (int i = a->size - 1; i >= 0; i--) {
        if (a->limbs[i] != b->limbs[i])
            return (a->limbs[i] > b->limbs[i]) ? 1 : -1;
    }
    return(0);  // equal
}


/*
 * result = a - b
 * Returns 0 on success.
 * Returns -1 if b > a (subtraction would be negative).
 * Either input may alias result.
 */

int bigIntSub(BigInt *result, const BigInt *a, const BigInt *b) {
    // Only allow a >= b
    if (bigIntCmp(a, b) < 0)
        return(-1);

    BigInt difference;
    bigIntZero(&difference);
    int maxSize = a->size;
    uint64_t borrow = 0;
    for (int i = 0; i < maxSize; i++) {
        uint64_t bv = (i < b->size ? (uint64_t)b->limbs[i] : 0) + borrow;
        uint64_t av = a->limbs[i];
        difference.limbs[i] = (uint32_t)(av - bv);
        borrow = av < bv;
    }
    difference.size = maxSize;
    while (difference.size > 1 && difference.limbs[difference.size - 1] == 0)
        difference.size--;
    *result = difference;
    return(0);
}



/* Shift the BigInt left by 'bits' bits (0 <= bits < 32?) Actually any amount.
 * Returns 0, or INT_MAX if result would exceed MAX_LIMBS. */
int bigIntShiftLeft(BigInt *a, int bits) {
    if (bits == 0) return 0;
    if (bits == INT_MIN) { bigIntZero(a); return 0; }
    if (bits < 0) return bigIntShiftRight(a, -bits);
    if (a->size == 1 && a->limbs[0] == 0) return 0;

    int limb_shift = bits / 32;
    int bit_shift  = bits % 32;

    // Reserve a carry limb only when bits spill out of the highest limb.
    int carry_limb = bit_shift != 0 &&
                     (a->limbs[a->size - 1] >> (32 - bit_shift)) != 0;
    if (a->size + limb_shift + carry_limb > MAX_LIMBS)
        return INT_MAX;

    // Make room by moving limbs up
    if (limb_shift > 0) {
        memmove(a->limbs + limb_shift, a->limbs, (size_t)a->size * sizeof(uint32_t));
        memset(a->limbs, 0, (size_t)limb_shift * sizeof(uint32_t));
    }

    // Bit shift
    if (bit_shift > 0) {
        uint32_t carry = 0;
        for (int i = limb_shift; i < a->size + limb_shift; i++) {
            uint64_t val = ((uint64_t)a->limbs[i] << bit_shift) | carry;
            a->limbs[i] = (uint32_t)val;
            carry = (uint32_t)(val >> 32);
        }
        if (carry) {
            a->limbs[a->size + limb_shift] = carry;
            a->size += limb_shift + 1;
        } else {
            a->size += limb_shift;
        }
    } else {
        a->size += limb_shift;
    }

    while (a->size > 1 && a->limbs[a->size-1] == 0)
        a->size--;
    return(0);
}

/* Shift the BigInt right by 'bits' bits. Truncates toward zero.
 * Returns 0. */
int bigIntShiftRight(BigInt *a, int bits) {
    if (bits == 0) return 0;
    if (bits == INT_MIN)
        return (a->size == 1 && a->limbs[0] == 0) ? 0 : INT_MAX;
    if (bits < 0) return bigIntShiftLeft(a, -bits);

    int limb_shift = bits / 32;
    int bit_shift  = bits % 32;

    if (limb_shift >= a->size) {
        bigIntZero(a);
        return(0);
    }

    // Move limbs down
    if (limb_shift > 0) {
        memmove(a->limbs, a->limbs + limb_shift,
                (size_t)(a->size - limb_shift) * sizeof(uint32_t));
        a->size -= limb_shift;
    }

    // Bit shift
    if (bit_shift > 0) {
        uint32_t carry = 0;
        for (int i = a->size - 1; i >= 0; i--) {
            uint64_t val = ((uint64_t)carry << 32) | a->limbs[i];
            a->limbs[i] = (uint32_t)(val >> bit_shift);
            carry = (uint32_t)(val & ((1ULL << bit_shift) - 1));
        }
        if (a->limbs[a->size-1] == 0 && a->size > 1)
            a->size--;
    }
    return(0);
}




/* Private exact intermediates. Products, scaled division and square root need
 * at most 2*FLOAT_BITS bits. Decimal output needs at most FLOAT_BITS plus
 * ceil(10*MAX_LIMBS*log2(10)) bits, less than 3*FLOAT_BITS. Two spare limbs
 * cover carries and normalization. Public values use at most MAX_LIMBS limbs. */
#define WIDE_LIMBS (3 * MAX_LIMBS + 2)
#define FLOAT_BITS (32 * MAX_LIMBS)
#define MAX_DECIMAL_PLACES (10 * MAX_LIMBS)

typedef struct {
    uint32_t limbs[WIDE_LIMBS];
    int size;
} WideInt;

static void wide_zero(WideInt *a) {
    memset(a, 0, sizeof(*a));
    a->size = 1;
}

static void wide_trim(WideInt *a) {
    while (a->size > 1 && a->limbs[a->size - 1] == 0) --a->size;
}

static void wide_from_int(WideInt *a, const BigInt *b) {
    wide_zero(a);
    memcpy(a->limbs, b->limbs, (size_t)b->size * sizeof(*b->limbs));
    a->size = b->size;
    wide_trim(a);
}

static int wide_bits(const WideInt *a) {
    return 32 * a->size - clz32(a->limbs[a->size - 1]);
}

static int wide_bit(const WideInt *a, int64_t bit) {
    if (bit < 0 || bit >= (int64_t)a->size * 32) return 0;
    return (int)((a->limbs[bit / 32] >> (bit % 32)) & 1u);
}

static int wide_cmp(const WideInt *a, const WideInt *b) {
    if (a->size != b->size) return a->size > b->size ? 1 : -1;
    for (int i = a->size - 1; i >= 0; --i)
        if (a->limbs[i] != b->limbs[i]) return a->limbs[i] > b->limbs[i] ? 1 : -1;
    return 0;
}

static int wide_left(WideInt *a, int64_t bits) {
    int used = wide_bits(a);
    if (used == 0 || bits == 0) return 0;
    if (bits < 0 || bits > 32 * WIDE_LIMBS - used) return INT_MAX;
    int words = (int)(bits / 32), shift = (int)(bits % 32);
    if (words) {
        memmove(a->limbs + words, a->limbs, (size_t)a->size * sizeof(*a->limbs));
        memset(a->limbs, 0, (size_t)words * sizeof(*a->limbs));
        a->size += words;
    }
    uint64_t carry = 0;
    for (int i = words; shift && i < a->size; ++i) {
        uint64_t v = ((uint64_t)a->limbs[i] << shift) | carry;
        a->limbs[i] = (uint32_t)v;
        carry = v >> 32;
    }
    if (carry) a->limbs[a->size++] = (uint32_t)carry;
    return 0;
}

/* Returns whether any discarded bit was nonzero. */
static int wide_right(WideInt *a, int64_t bits) {
    if (bits <= 0) return 0;
    if (bits >= wide_bits(a)) {
        int lost = wide_bits(a) != 0;
        wide_zero(a);
        return lost;
    }
    int words = (int)(bits / 32), shift = (int)(bits % 32), lost = 0;
    for (int i = 0; i < words; ++i) lost |= a->limbs[i] != 0;
    if (shift) lost |= (a->limbs[words] & (UINT32_MAX >> (32 - shift))) != 0;
    int new_size = a->size - words;
    for (int i = 0; i < new_size; ++i) {
        uint64_t v = a->limbs[i + words];
        if (shift && i + words + 1 < a->size)
            v |= (uint64_t)a->limbs[i + words + 1] << 32;
        a->limbs[i] = (uint32_t)(v >> shift);
    }
    memset(a->limbs + new_size, 0, (size_t)(a->size - new_size) * sizeof(*a->limbs));
    a->size = new_size;
    wide_trim(a);
    return lost;
}

static int wide_add_small(WideInt *a, uint32_t b) {
    uint64_t carry = b;
    for (int i = 0; carry && i < a->size; ++i) {
        carry += a->limbs[i];
        a->limbs[i] = (uint32_t)carry;
        carry >>= 32;
    }
    if (carry) {
        if (a->size == WIDE_LIMBS) return INT_MAX;
        a->limbs[a->size++] = (uint32_t)carry;
    }
    return 0;
}

static int wide_add(WideInt *a, const WideInt *b) {
    int size = a->size > b->size ? a->size : b->size;
    uint64_t carry = 0;
    for (int i = 0; i < size; ++i) {
        uint64_t av = i < a->size ? a->limbs[i] : 0;
        uint64_t bv = i < b->size ? b->limbs[i] : 0;
        uint64_t v = av + bv + carry;
        a->limbs[i] = (uint32_t)v;
        carry = v >> 32;
    }
    a->size = size;
    if (carry) {
        if (a->size == WIDE_LIMBS) return INT_MAX;
        a->limbs[a->size++] = (uint32_t)carry;
    }
    return 0;
}

/* a >= b; all uses have bounded, valid private operands. */
static void wide_sub(WideInt *a, const WideInt *b) {
    uint64_t borrow = 0;
    for (int i = 0; i < a->size; ++i) {
        uint64_t av = a->limbs[i];
        uint64_t bv = (i < b->size ? (uint64_t)b->limbs[i] : 0) + borrow;
        a->limbs[i] = (uint32_t)(av - bv);
        borrow = av < bv;
    }
    wide_trim(a);
}

static int wide_mul_small(WideInt *a, uint32_t b) {
    uint64_t carry = 0;
    for (int i = 0; i < a->size; ++i) {
        uint64_t v = (uint64_t)a->limbs[i] * b + carry;
        a->limbs[i] = (uint32_t)v;
        carry = v >> 32;
    }
    if (carry) {
        if (a->size == WIDE_LIMBS) return INT_MAX;
        a->limbs[a->size++] = (uint32_t)carry;
    }
    wide_trim(a);
    return 0;
}

static uint32_t wide_div_small(WideInt *a, uint32_t b) {
    uint64_t rem = 0;
    for (int i = a->size - 1; i >= 0; --i) {
        uint64_t v = (rem << 32) | a->limbs[i];
        a->limbs[i] = (uint32_t)(v / b);
        rem = v % b;
    }
    wide_trim(a);
    return (uint32_t)rem;
}

static void wide_mul(WideInt *out, const BigInt *a, const BigInt *b) {
    wide_zero(out);
    for (int i = 0; i < a->size; ++i) {
        uint64_t carry = 0;
        for (int j = 0; j < b->size; ++j) {
            uint64_t v = (uint64_t)a->limbs[i] * b->limbs[j] + out->limbs[i + j] + carry;
            out->limbs[i + j] = (uint32_t)v;
            carry = v >> 32;
        }
        out->limbs[i + b->size] = (uint32_t)carry;
    }
    out->size = a->size + b->size;
    wide_trim(out);
}

/* Base-256 convolution coefficients are at most 4*MAX_LIMBS*255^2.
 * At 1024 limbs this is 266342400 < 998244353. Consequently one NTT
 * determines each coefficient exactly, not merely a checksum of the product.
 * The prime is 119*2^23+1 and 3 is a primitive root. Keep both bounds explicit
 * so a future capacity change cannot silently invalidate certification. */
#define MUL_NTT_PRIME UINT32_C(998244353)
#if 4ULL * MAX_LIMBS * 255 * 255 >= 998244353ULL || \
    8ULL * MAX_LIMBS > (1ULL << 23)
#error "FFT multiplication needs a larger exact coefficient verification range"
#endif

static uint32_t mul_mod_power(uint32_t base, uint32_t exponent) {
    uint32_t result = 1;
    for (; exponent; exponent >>= 1) {
        if (exponent & 1)
            result = (uint32_t)((uint64_t)result * base % MUL_NTT_PRIME);
        base = (uint32_t)((uint64_t)base * base % MUL_NTT_PRIME);
    }
    return result;
}

/* Private radix-2 transform, with n a power of two no greater than 2^23.
 * All residues stay below the prime; products fit uint64_t and sums uint32_t. */
static void mul_ntt(uint32_t *values, int n, int inverse) {
    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
        if (i < j) {
            uint32_t swap = values[i];
            values[i] = values[j];
            values[j] = swap;
        }
    }
    for (int width = 2; width <= n; width *= 2) {
        uint32_t exponent = (MUL_NTT_PRIME - 1) / (uint32_t)width;
        if (inverse) exponent = MUL_NTT_PRIME - 1 - exponent;
        uint32_t step = mul_mod_power(3, exponent);
        for (int start = 0; start < n; start += width) {
            uint32_t phase = 1;
            for (int j = 0; j < width / 2; ++j) {
                uint32_t even = values[start + j];
                uint32_t odd = (uint32_t)((uint64_t)phase *
                    values[start + j + width / 2] % MUL_NTT_PRIME);
                uint32_t sum = even + odd;
                values[start + j] = sum >= MUL_NTT_PRIME ? sum - MUL_NTT_PRIME : sum;
                values[start + j + width / 2] =
                    even >= odd ? even - odd : even + MUL_NTT_PRIME - odd;
                phase = (uint32_t)((uint64_t)phase * step % MUL_NTT_PRIME);
            }
        }
    }
    if (inverse) {
        uint32_t scale = mul_mod_power((uint32_t)n, MUL_NTT_PRIME - 2);
        for (int i = 0; i < n; ++i)
            values[i] = (uint32_t)((uint64_t)values[i] * scale % MUL_NTT_PRIME);
    }
}

/* FFT convolution with exact coefficient certification. Recover using the NTT
 * coefficients if floating-point reconstruction fails. Both paths have
 * O(n log n) cost; there is no quadratic multiplication fallback. */
int bigIntMulFFT(BigInt *result, const BigInt *a, const BigInt *b) {
    if ((a->size == 1 && a->limbs[0] == 0) ||
        (b->size == 1 && b->limbs[0] == 0)) {
        bigIntZero(result);
        return 0;
    }
    int digits_a = 4 * a->size, digits_b = 4 * b->size;
    int length = digits_a + digits_b - 1, n = 1;
    while (n < length) n *= 2;
    complexNum *fa = calloc((size_t)n, sizeof(*fa));
    complexNum *fb = calloc((size_t)n, sizeof(*fb));
    uint32_t *na = calloc((size_t)n, sizeof(*na));
    uint32_t *nb = calloc((size_t)n, sizeof(*nb));
    int status = -1;
    if (!fa || !fb || !na || !nb) goto done;
    for (int i = 0; i < digits_a; ++i) {
        uint32_t digit = (a->limbs[i / 4] >> (8 * (i % 4))) & 255u;
        fa[i].re = digit;
        na[i] = digit;
    }
    for (int i = 0; i < digits_b; ++i) {
        uint32_t digit = (b->limbs[i / 4] >> (8 * (i % 4))) & 255u;
        fb[i].re = digit;
        nb[i] = digit;
    }
    if (fft(fa, n, 0) != 0 || fft(fb, n, 0) != 0) goto done;
    for (int i = 0; i < n; ++i) {
        double real = fa[i].re * fb[i].re - fa[i].im * fb[i].im;
        double imaginary = fa[i].re * fb[i].im + fa[i].im * fb[i].re;
        fa[i].re = real;
        fa[i].im = imaginary;
    }
    if (fft(fa, n, 1) != 0) goto done;

    mul_ntt(na, n, 0);
    mul_ntt(nb, n, 0);
    for (int i = 0; i < n; ++i)
        na[i] = (uint32_t)((uint64_t)na[i] * nb[i] % MUL_NTT_PRIME);
    mul_ntt(na, n, 1);
    int verified = 1;
    for (int i = 0; i < length; ++i) {
        /* Compare in double before any integer cast. This rejects NaN,
         * infinity, and out-of-range values without undefined conversion. */
        fa[i].re = floor(fa[i].re + 0.5);
        if (!isfinite(fa[i].re) || fa[i].re != (double)na[i]) verified = 0;
    }
    BigInt packed;
    bigIntZero(&packed);
    uint64_t carry = 0;
    for (int i = 0; i <= length; ++i) {
        uint32_t coefficient = i == length ? 0 :
            verified ? (uint32_t)fa[i].re : na[i];
        carry += coefficient;
        uint32_t digit = (uint32_t)(carry & 255u);
        carry >>= 8;
        if (i / 4 >= MAX_LIMBS) {
            if (digit != 0) { status = INT_MAX; goto done; }
        } else {
            packed.limbs[i / 4] |= digit << (8 * (i % 4));
            if (digit != 0) packed.size = i / 4 + 1;
        }
    }
    /* The product of digits_a and digits_b base-256 integers needs at most
     * digits_a+digits_b digits, so the extra iteration consumes all carry. */
    *result = packed;
    status = 0;
done:
    free(fa);
    free(fb);
    free(na);
    free(nb);
    return status;
}

/* Binary long division. The remainder needs at most divisor_bits + 1 bits;
 * callers keep dividends/divisors below two public mantissas in size. */
static void wide_div(WideInt *q, const WideInt *a, const WideInt *b) {
    WideInt rem;
    wide_zero(&rem);
    wide_zero(q);
    for (int bit = wide_bits(a) - 1; bit >= 0; --bit) {
        (void)wide_left(&rem, 1);
        rem.limbs[0] |= (uint32_t)wide_bit(a, bit);
        if (wide_cmp(&rem, b) >= 0) {
            wide_sub(&rem, b);
            q->limbs[bit / 32] |= UINT32_C(1) << (bit % 32);
            if (q->size < bit / 32 + 1) q->size = bit / 32 + 1;
        }
    }
}

/* Restoring square root, consuming two radicand bits at each step. */
static void wide_sqrt(WideInt *root, const WideInt *a) {
    WideInt rem, trial;
    wide_zero(root);
    wide_zero(&rem);
    for (int pair = (wide_bits(a) + 1) / 2 - 1; pair >= 0; --pair) {
        (void)wide_left(&rem, 2);
        rem.limbs[0] |= (uint32_t)(2 * wide_bit(a, 2 * pair + 1) + wide_bit(a, 2 * pair));
        (void)wide_left(root, 1);
        trial = *root;
        (void)wide_left(&trial, 1);
        (void)wide_add_small(&trial, 1);
        if (wide_cmp(&rem, &trial) >= 0) {
            wide_sub(&rem, &trial);
            (void)wide_add_small(root, 1);
        }
    }
}

static int clamp_precision(int limbs) {
    return limbs < 1 ? 1 : limbs > MAX_LIMBS ? MAX_LIMBS : limbs;
}

/* Round toward zero, normalize, and check the final exponent before publishing.
 * Whole zero limbs may be moved across the exponent boundary without losing
 * information, so an intermediate exponent overflow need not reject a value. */
static int float_pack(BigFloat *out, WideInt *m, int64_t exp, int sign, int limbs) {
    int bits = wide_bits(m);
    if (bits == 0) { bigFloatZero(out); return 0; }
    int precision = 32 * limbs;
    if (bits > precision) {
        (void)wide_right(m, bits - precision);
        exp += bits - precision;
        bits = precision;
    }
    int shift = (32 - bits % 32) % 32;
    (void)wide_left(m, shift);
    exp -= shift;
    while (exp < INT32_MIN && m->size > 1 && m->limbs[0] == 0) {
        (void)wide_right(m, 32);
        exp += 32;
    }
    if (exp > INT32_MAX) {
        int64_t words = (exp - INT32_MAX + 31) / 32;
        if (words > limbs - m->size) return INT_MAX;
        (void)wide_left(m, words * 32);
        exp -= words * 32;
    }
    if (exp < INT32_MIN || exp > INT32_MAX) return INT_MAX;
    BigFloat result;
    bigFloatZero(&result);
    memcpy(result.mantissa.limbs, m->limbs, (size_t)m->size * sizeof(*m->limbs));
    result.mantissa.size = m->size;
    result.exp = (int32_t)exp;
    result.sign = sign;
    *out = result;
    return 0;
}

void bigFloatZero(BigFloat *x) {
    bigIntZero(&x->mantissa);
    x->exp = 0;
    x->sign = 1;
}

void bigFloatFromUint32(BigFloat *x, uint32_t v) {
    bigFloatZero(x);
    x->mantissa.limbs[0] = v;
    (void)bigFloatNormalize(x);
}

void bigFloatCopy(BigFloat *dst, const BigFloat *src) {
    if (dst == src) return;
    memcpy(dst->mantissa.limbs, src->mantissa.limbs,
           (size_t)src->mantissa.size * sizeof(*src->mantissa.limbs));
    memset(dst->mantissa.limbs + src->mantissa.size, 0,
           (size_t)(MAX_LIMBS - src->mantissa.size) * sizeof(*src->mantissa.limbs));
    dst->mantissa.size = src->mantissa.size;
    dst->exp = src->exp;
    dst->sign = src->sign;
}

int bigFloatNormalize(BigFloat *x) {
    WideInt m;
    wide_from_int(&m, &x->mantissa);
    return float_pack(x, &m, x->exp, x->sign, MAX_LIMBS);
}

void bigFloatTruncate(BigFloat *x, int target_limbs) {
    target_limbs = clamp_precision(target_limbs);
    if (x->mantissa.size <= target_limbs) return;
    int shift = 32 * (x->mantissa.size - target_limbs);
    WideInt m;
    wide_from_int(&m, &x->mantissa);
    (void)wide_right(&m, shift);
    if (float_pack(x, &m, (int64_t)x->exp + shift, x->sign, target_limbs) != 0)
        errno = ERANGE;
}

int bigFloatShiftLeft(BigFloat *x, int bits) {
    if (bits == 0) return 0;
    BigFloat tmp;
    bigFloatCopy(&tmp, x);
    int rc = bigIntShiftLeft(&tmp.mantissa, bits);
    if (rc == 0) rc = bigFloatNormalize(&tmp);
    if (rc == 0) *x = tmp;
    return rc;
}

int bigFloatShiftRight(BigFloat *x, int bits) {
    if (bits == 0) return 0;
    BigFloat tmp;
    bigFloatCopy(&tmp, x);
    int rc = bigIntShiftRight(&tmp.mantissa, bits);
    if (rc == 0) rc = bigFloatNormalize(&tmp);
    if (rc == 0) *x = tmp;
    return rc;
}

/* Compare absolute values without overflowing or discarding aligned bits. */
int bigFloatCmpAbs(const BigFloat *a, const BigFloat *b) {
    int as = a->mantissa.size, bs = b->mantissa.size;
    while (as > 1 && a->mantissa.limbs[as - 1] == 0) --as;
    while (bs > 1 && b->mantissa.limbs[bs - 1] == 0) --bs;
    int ab = 32 * as - clz32(a->mantissa.limbs[as - 1]);
    int bb = 32 * bs - clz32(b->mantissa.limbs[bs - 1]);
    if (ab == 0 || bb == 0) return (ab > 0) - (bb > 0);
    int64_t atop = (int64_t)a->exp + ab, btop = (int64_t)b->exp + bb;
    if (atop != btop) return atop > btop ? 1 : -1;
    for (int ai = ab - 1, bi = bb - 1; ai >= 0 || bi >= 0; --ai, --bi) {
        int av = bigIntGetBit(&a->mantissa, ai);
        int bv = bigIntGetBit(&b->mantissa, bi);
        if (av != bv) return av > bv ? 1 : -1;
    }
    return 0;
}

int bigFloatMul(BigFloat *result, const BigFloat *a, const BigFloat *b) {
    WideInt product;
    wide_mul(&product, &a->mantissa, &b->mantissa);
    return float_pack(result, &product, (int64_t)a->exp + b->exp,
                      a->sign * b->sign, MAX_LIMBS);
}

int bigFloatAdd(BigFloat *result, const BigFloat *a, const BigFloat *b) {
    WideInt am, bm;
    wide_from_int(&am, &a->mantissa);
    wide_from_int(&bm, &b->mantissa);
    int ab = wide_bits(&am), bb = wide_bits(&bm);
    if (ab == 0) return float_pack(result, &bm, b->exp, b->sign, MAX_LIMBS);
    if (bb == 0) return float_pack(result, &am, a->exp, a->sign, MAX_LIMBS);
    int cmp = bigFloatCmpAbs(a, b);
    if (a->sign != b->sign && cmp == 0) { bigFloatZero(result); return 0; }

    /* Preserve all available precision and two guard bits. At least the operand
     * with the greater exponent is exact at this scale; at most one tail is
     * discarded. This also bounds work when exponents are billions apart. */
    int64_t topa = (int64_t)a->exp + ab, topb = (int64_t)b->exp + bb;
    int64_t exp = a->exp < b->exp ? a->exp : b->exp;
    int64_t floor_exp = (topa > topb ? topa : topb) - FLOAT_BITS - 2;
    if (exp < floor_exp) exp = floor_exp;
    int lost_a = 0, lost_b = 0;
    if (a->exp >= exp) (void)wide_left(&am, (int64_t)a->exp - exp);
    else lost_a = wide_right(&am, exp - a->exp);
    if (b->exp >= exp) (void)wide_left(&bm, (int64_t)b->exp - exp);
    else lost_b = wide_right(&bm, exp - b->exp);

    int sign = a->sign;
    if (a->sign == b->sign) {
        (void)wide_add(&am, &bm);
    } else {
        WideInt *larger = cmp > 0 ? &am : &bm;
        WideInt *smaller = cmp > 0 ? &bm : &am;
        int lost_smaller = cmp > 0 ? lost_b : lost_a;
        sign = cmp > 0 ? a->sign : b->sign;
        wide_sub(larger, smaller);
        if (lost_smaller) {
            /* floor(integer - positive fractional tail) = integer - 1. */
            WideInt one;
            wide_zero(&one);
            one.limbs[0] = 1;
            wide_sub(larger, &one);
        }
        if (larger != &am) am = *larger;
    }
    return float_pack(result, &am, exp, sign, MAX_LIMBS);
}

int bigFloatSub(BigFloat *result, const BigFloat *a, const BigFloat *b) {
    BigFloat negative;
    bigFloatCopy(&negative, b);
    negative.sign = -negative.sign;
    return bigFloatAdd(result, a, &negative);
}

int bigFloatDiv(BigFloat *result, const BigFloat *a, const BigFloat *b, int target_limbs) {
    WideInt numerator, denominator, cmpa, cmpb, quotient;
    wide_from_int(&numerator, &a->mantissa);
    wide_from_int(&denominator, &b->mantissa);
    int ab = wide_bits(&numerator), bb = wide_bits(&denominator);
    if (bb == 0) return INT_MAX;
    if (ab == 0) { bigFloatZero(result); return 0; }
    target_limbs = clamp_precision(target_limbs);
    int magnitude = ab - bb;
    cmpa = numerator;
    cmpb = denominator;
    if (magnitude >= 0) (void)wide_left(&cmpb, magnitude);
    else (void)wide_left(&cmpa, -magnitude);
    if (wide_cmp(&cmpa, &cmpb) < 0) --magnitude;
    int shift = 32 * target_limbs - 1 - magnitude;
    if (shift >= 0) (void)wide_left(&numerator, shift);
    else (void)wide_left(&denominator, -shift);
    wide_div(&quotient, &numerator, &denominator);
    int64_t exp = (int64_t)a->exp - b->exp - shift;
    return float_pack(result, &quotient, exp, a->sign * b->sign, target_limbs);
}

int bigFloatReciprocal(BigFloat *result, const BigFloat *x, int target_limbs) {
    BigFloat one;
    bigFloatFromUint32(&one, 1);
    return bigFloatDiv(result, &one, x, target_limbs);
}

int bigFloatSqrt(BigFloat *result, const BigFloat *x, int target_limbs) {
    if (x->sign < 0) return -1;
    WideInt radicand, root;
    wide_from_int(&radicand, &x->mantissa);
    int bits = wide_bits(&radicand);
    if (bits == 0) { bigFloatZero(result); return 0; }
    target_limbs = clamp_precision(target_limbs);
    int64_t top = (int64_t)x->exp + bits - 1;
    int64_t magnitude = top >= 0 ? top / 2 : -((-top + 1) / 2);
    int64_t exp = magnitude - (32 * target_limbs - 1);
    int64_t shift = (int64_t)x->exp - 2 * exp;
    if (shift >= 0) (void)wide_left(&radicand, shift);
    else (void)wide_right(&radicand, -shift);
    wide_sqrt(&root, &radicand);
    return float_pack(result, &root, exp, 1, target_limbs);
}

void printBigFloat(const BigFloat *x, int decimal_places) {
    if (decimal_places < 0) decimal_places = 0;
    WideInt scaled;
    wide_from_int(&scaled, &x->mantissa);
    int bits = wide_bits(&scaled);
    if (decimal_places > MAX_DECIMAL_PLACES ||
        (bits && (int64_t)x->exp + bits > FLOAT_BITS)) {
        errno = ERANGE;
        return;
    }
    /* x * 10^places = mantissa * 5^places * 2^(exp + places).
     * Rounding the complete scaled integer carries across the decimal point. */
    for (int i = 0; i < decimal_places; ++i) {
        if (wide_mul_small(&scaled, 5) != 0) { errno = ERANGE; return; }
    }
    int64_t shift = (int64_t)x->exp + decimal_places;
    if (shift >= 0) {
        if (wide_left(&scaled, shift) != 0) { errno = ERANGE; return; }
    } else {
        int round_up = wide_bit(&scaled, -shift - 1);
        (void)wide_right(&scaled, -shift);
        if (round_up && wide_add_small(&scaled, 1) != 0) { errno = ERANGE; return; }
    }
    char digits[2 * DECIMAL_BUFFER_SIZE + 2];
    int count = 0;
    do {
        digits[count++] = (char)('0' + wide_div_small(&scaled, 10));
    } while (wide_bits(&scaled) != 0);
    while (count <= decimal_places) digits[count++] = '0';
    if (x->sign < 0 && bits != 0) putchar('-');
    for (int i = count - 1; i >= 0; --i) {
        putchar(digits[i]);
        if (i == decimal_places) putchar('.');
    }
}
