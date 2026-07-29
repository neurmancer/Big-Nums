/*
        Sup? this will have less commentary since this is more like a project side-quest for Tupper's self-referantial formula bullet-point
        Idea is simple make C stop bitching about big nums

        Important shit to know: 
        I'll be using Little-endian to store shit to make carry propagation natural
        I'll be using my own FFT (an enhanced version that's able to handle non-power-of-two)


        Hii...I've returned back (next day and implemented big floats cuz I need them for Ramanujan fuckery)
        This one is a side-quest so I think I documented it good enough
*/

#include <string.h>     //For memset
#include <stdlib.h>     //For Dyanmic memory shit
#include <stdio.h>
#include "complexFFT.h"
#include "bignums.h"

#define MAX_LIMBS 64 // 64 * 32bits (int standard) = 2048 bits...hope that's gonna be enough
#ifndef INT_MAX
    #define INT_MAX (~0u>>1)
#endif


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


/* constant 0.5 in BigFloat format (mantissa=1, exp=-1) */
static const BigFloat half_const = {
    .mantissa = { .limbs = {1}, .size = 1 },
    .exp = -1,
    .sign = 1
};

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

    // Max decimal digits: 2048 bits ≈ 617 digits + '\0'
    char digits[620];
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
    for (int i = 0; carry > 0 && i < MAX_LIMBS; i++) {
        carry += a->limbs[i];
        a->limbs[i] = (uint32_t)carry;
        carry >>= 32;
    }
    // update size (largest index with non-zero limb)
    while (a->size > 1 && a->limbs[a->size-1] == 0)
        a->size--;
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
    int limb = bit_index / 32;
    int bit  = bit_index % 32;
    if (limb >= a->size) return 0;
    return (a->limbs[limb] >> bit) & 1;
}

/* smallest power of two >= n */
static int next_pow2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

/*
    FFT-based multiplication: result = a * b.
    Splits each 32-bit limb into two 16-bit digits to stay inside double precision.
    Returns 0 on success, -1 if the result would need > MAX_LIMBS limbs.
    This is my first API design duh...

    DEV NOTES: FUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUUCK My brain is getting squishy

*/
int bigIntMulFFT(BigInt *result, const BigInt *a, const BigInt *b) {
    int la = a->size;
    int lb = b->size;

    /* 1. Convert each limb into TWO 16-bit digits (low, high) */
    int digitsA = la * 2;
    int digitsB = lb * 2;
    int convLen = digitsA + digitsB - 1;     // linear convolution length
    int N = next_pow2(convLen);              // still pad to power-of-2 for speed
                                              // (fft_arbitrary will just call the fast path)

    /* 2. Allocate zero-padded complex arrays */
    complexNum *A = calloc(N, sizeof(complexNum));
    complexNum *B = calloc(N, sizeof(complexNum));
    if (!A || !B) {
        free(A); free(B);
        return(-1);
    }

    // Fill A (little-endian 16-bit digits)
    for (int i = 0; i < la; i++) {
        uint32_t limb = a->limbs[i];
        A[2*i].re     = (double)(limb & 0xFFFF);
        A[2*i + 1].re = (double)((limb >> 16) & 0xFFFF);
    }
    // Fill B
    for (int i = 0; i < lb; i++) {
        uint32_t limb = b->limbs[i];
        B[2*i].re     = (double)(limb & 0xFFFF);
        B[2*i + 1].re = (double)((limb >> 16) & 0xFFFF);
    }

    /* 3. Forward FFT */
    if (fft_arbitrary(A, N, 0) != 0 || fft_arbitrary(B, N, 0) != 0) {
        free(A); free(B);
        return(-1);
    }

    /* 4. Pointwise multiply */
    for (int i = 0; i < N; i++) {
        double re = A[i].re * B[i].re - A[i].im * B[i].im;
        double im = A[i].re * B[i].im + A[i].im * B[i].re;
        A[i].re = re;
        A[i].im = im;
    }

    /* 5. Inverse FFT */
    if (fft_arbitrary(A, N, 1) != 0) {
        free(A); free(B);
        return(-1);
    }

    /* 6. Temp buffer for rounded convolution + carry room */
    uint64_t *temp = calloc(convLen + 2, sizeof(uint64_t));
    if (!temp) {
        free(A); free(B);
        return(-1);
    }

    /* 7. Round (numbers are positive so +0.5 is fine) */
    for (int i = 0; i < convLen; i++) {
        // defensive: clamp tiny negative noise from floating-point error
        double v = A[i].re;
        if (v < 0.0) v = 0.0;
        temp[i] = (uint64_t)(v + 0.5);
    }

    /* 8. Carry propagation base 2^16 (one forward pass is enough) */
    for (int i = 0; i < convLen + 1; i++) {
        if (temp[i] >= 0x10000ULL) {
            temp[i + 1] += temp[i] >> 16;
            temp[i] &= 0xFFFFULL;
        }
    }

    // Find highest non-zero digit
    int lastDigit = convLen + 1;
    while (lastDigit > 0 && temp[lastDigit - 1] == 0)
        lastDigit--;

    /* Product is zero */
    if (lastDigit == 0) {
        bigIntZero(result);
        free(temp); free(A); free(B);
        return(0);
    }

    /* 9. Pack two 16-bit digits → one 32-bit limb */
    int outSize = (lastDigit + 1) / 2;
    if (outSize > MAX_LIMBS) {
        free(temp); free(A); free(B);
        return(INT_MAX);      // overflow
    }

    memset(result->limbs, 0, sizeof(result->limbs));
    for (int i = 0; i < lastDigit; i += 2) {
        uint32_t low  = (uint32_t)(temp[i] & 0xFFFF);
        uint32_t high = (i + 1 < lastDigit) ? (uint32_t)(temp[i + 1] & 0xFFFF) : 0;
        result->limbs[i / 2] = low | (high << 16);
    }

    result->size = outSize;
    while (result->size > 1 && result->limbs[result->size - 1] == 0)
        result->size--;

    free(temp);
    free(A);
    free(B);
    return(0);
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
 * Safe when result aliases a (in-place a = a - b), but NOT when result aliases b.
 */

int bigIntSub(BigInt *result, const BigInt *a, const BigInt *b) {
    // Only allow a >= b
    if (bigIntCmp(a, b) < 0)
        return(-1);

    // If result is not the same as a, copy a into result first
    if (result != a)
        memcpy(result->limbs, a->limbs, a->size * sizeof(uint32_t));

    int maxSize = a->size;
    uint64_t borrow = 0;

    // Subtract b's limbs
    for (int i = 0; i < b->size; i++) {
        uint64_t sub = (uint64_t)result->limbs[i] - (uint64_t)b->limbs[i] - borrow;
        result->limbs[i] = (uint32_t)sub;
        borrow = (sub >> 32) != 0;   // borrow if underflow occurred
    }

    // Propagate borrow through remaining limbs of a (if any)
    for (int i = b->size; borrow && i < maxSize; i++) {
        uint64_t sub = (uint64_t)result->limbs[i] - borrow;
        result->limbs[i] = (uint32_t)sub;
        borrow = (sub >> 32) != 0;
    }

    // Set size and trim leading zeros
    result->size = maxSize;
    while (result->size > 1 && result->limbs[result->size - 1] == 0)
        result->size--;

    return(0);
}



/* Shift the BigInt left by 'bits' bits (0 <= bits < 32?) Actually any amount.
 * Returns 0, or INT_MAX if result would exceed MAX_LIMBS. */
int bigIntShiftLeft(BigInt *a, int bits) {
    if (bits == 0) return 0;
    if (bits < 0) return bigIntShiftRight(a, -bits);   // unlikely but safe

    int limb_shift = bits / 32;
    int bit_shift  = bits % 32;

    // Check overflow
    if (a->size + limb_shift + 1 > MAX_LIMBS)
        return INT_MAX;

    // Make room by moving limbs up
    if (limb_shift > 0) {
        memmove(a->limbs + limb_shift, a->limbs, a->size * sizeof(uint32_t));
        memset(a->limbs, 0, limb_shift * sizeof(uint32_t));
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

    // Trim any trailing zero? Not needed.
    while (a->size > 1 && a->limbs[a->size-1] == 0)
        a->size--;
    return(0);
}

/* Shift the BigInt right by 'bits' bits. Truncates toward zero.
 * Returns 0. */
int bigIntShiftRight(BigInt *a, int bits) {
    if (bits == 0) return 0;
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
                (a->size - limb_shift) * sizeof(uint32_t));
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




/* ---------- BigFloat basic ---------- */
void bigFloatZero(BigFloat *x) {
    bigIntZero(&x->mantissa);
    x->exp  = 0;
    x->sign = 1;
}

void bigFloatFromUint32(BigFloat *x, uint32_t v) {
    if (v == 0) {
        bigFloatZero(x);
        return;
    }
    bigIntZero(&x->mantissa);
    x->mantissa.limbs[0] = v;
    x->mantissa.size     = 1;
    x->exp               = 0;
    x->sign              = 1;
    bigFloatNormalize(x);   
}

/* =====================================================================
   GUARD LIMB TRUNCATION
   ===================================================================== */

// Silently chops off the lowest limbs to hide iteration noise and enforce a strict precision
void bigFloatTruncate(BigFloat *x, int target_limbs) {
    if (target_limbs < 1) target_limbs = 1;
    
    if (x->mantissa.size > target_limbs) {
        int limbs_to_chop = x->mantissa.size - target_limbs;
        
        // Shift the mantissa right by exactly 'limbs_to_chop' whole limbs (32 bits each)
        // This physically deletes the un-converged noise at the bottom of the number!
        bigIntShiftRight(&x->mantissa, limbs_to_chop * 32);
        
        // Compensate the exponent because we effectively divided the mantissa integer by 2^(32 * chop)
        x->exp += (limbs_to_chop * 32);
        
        // Clean up any remaining leading zeros just in case
        bigFloatNormalize(x);
    }
}

void printBigFloat(const BigFloat *x, int decimal_places)
{
    if (decimal_places < 0) decimal_places = 0;

    if (x->mantissa.size == 1 && x->mantissa.limbs[0] == 0) {
        printf("0.");
        for (int i = 0; i < decimal_places; i++) printf("0");
        return;
    }

    if (x->sign < 0) printf("-");
    BigFloat pos;
    bigFloatCopy(&pos, x);
    pos.sign = 1;

    BigFloat int_float, frac;
    BigInt   int_part, frac_digits;
    BigFloat scale, scaled_frac;

    // Integer part
    int_part = pos.mantissa;
    if (pos.exp >= 0) {
        bigIntShiftLeft(&int_part, pos.exp);
    } else {
        bigIntShiftRight(&int_part, -pos.exp);
    }
    printBigInt(&int_part);
    printf(".");

    // Fractional part = pos - int_part (as BigFloat)
    int_float.mantissa = int_part;
    int_float.exp = 0;
    int_float.sign = 1;
    bigFloatNormalize(&int_float);
    bigFloatSub(&frac, &pos, &int_float);

    // Multiply by 10^decimal_places and round
    {
        BigInt ten_pow;
        bigIntZero(&ten_pow);
        ten_pow.limbs[0] = 1;
        for (int i = 0; i < decimal_places; i++)
            bigIntMulUInt_32(&ten_pow, 10);

        scale.mantissa = ten_pow;
        scale.exp = 0;
        scale.sign = 1;
        bigFloatNormalize(&scale);
    }
    bigFloatMul(&scaled_frac, &frac, &scale);
    bigFloatAdd(&scaled_frac, &scaled_frac, &half_const);

    // Extract digits
    frac_digits = scaled_frac.mantissa;
    if (scaled_frac.exp >= 0) {
        bigIntShiftLeft(&frac_digits, scaled_frac.exp);
    } else {
        bigIntShiftRight(&frac_digits, -scaled_frac.exp);
    }

    char buf[22];
    int bufPos = sizeof(buf) - 1;
    buf[bufPos] = '\0';

    if (frac_digits.size == 1 && frac_digits.limbs[0] == 0) {
        for (int i = 0; i < decimal_places; i++){ printf("0"); }
        return;
    }

    BigInt tmp = frac_digits;
    int digit_count = 0;
    while (!(tmp.size == 1 && tmp.limbs[0] == 0)) {
        uint32_t rem = bigIntDivUInt32(&tmp, 10);
        buf[--bufPos] = (char)('0' + rem);
        digit_count++;
    }

    int leading_zeros = decimal_places - digit_count;
    for (int i = 0; i < leading_zeros; i++){ printf("0"); }
    printf("%s", &buf[bufPos]);
}


/* Normalize: only remove leading zero limbs (no bit‑level shifting).*/

int bigFloatNormalize(BigFloat *x) {
    // trim leading zero limbs
    while (x->mantissa.size > 1 &&
           x->mantissa.limbs[x->mantissa.size-1] == 0){
        x->mantissa.size--;
    }
    
    if (x->mantissa.size == 1 && x->mantissa.limbs[0] == 0) {
        bigFloatZero(x);
        return(0);
    }

    uint32_t high = x->mantissa.limbs[x->mantissa.size-1];
    int shift = clz32(high);
    if (shift == 0){ return(0); }

    // shift mantissa left by 'shift' bits
    if (bigIntShiftLeft(&x->mantissa, shift) != 0) return INT_MAX;
    x->exp -= shift;
    
    return(0);
}

/* Shift the whole float left by 'bits' bits.
 * Actually shift the mantissa bits. */
int bigFloatShiftLeft(BigFloat *x, int bits) {
    if (bits == 0){ return(0); }
    int rc = bigIntShiftLeft(&x->mantissa, bits);
    
    if (rc != 0){ return(rc); }
    
    return(bigFloatNormalize(x));
}

int bigFloatShiftRight(BigFloat *x, int bits) {
    if (bits == 0){ return(0); }
    int rc = bigIntShiftRight(&x->mantissa, bits);
    if (rc != 0){ return(rc); }
    return(bigFloatNormalize(x));
}

/* Compare absolute values (ignore sign). */
int bigFloatCmpAbs(const BigFloat *a, const BigFloat *b) {
    int a_zero = (a->mantissa.size == 1 && a->mantissa.limbs[0] == 0);
    int b_zero = (b->mantissa.size == 1 && b->mantissa.limbs[0] == 0);
    
    if (a_zero && b_zero){ return(0); }
    
    if (a_zero){ return(-1); }
    
    if (b_zero){ return(-1); }

    if (a->exp != b->exp) { return((a->exp > b->exp) ? 1 : -1); }
    
    return(bigIntCmp(&a->mantissa, &b->mantissa));
}

void bigFloatCopy(BigFloat *dst, const BigFloat *src) {
    if (dst == src) return;
    memcpy(dst->mantissa.limbs, src->mantissa.limbs,
           src->mantissa.size * sizeof(uint32_t));
    if (src->mantissa.size < MAX_LIMBS)
        memset(dst->mantissa.limbs + src->mantissa.size, 0,
               (MAX_LIMBS - src->mantissa.size) * sizeof(uint32_t));
    dst->mantissa.size = src->mantissa.size;
    dst->exp           = src->exp;
    dst->sign          = src->sign;
}

/* ---------- BigFloat multiplication ---------- */
int bigFloatMul(BigFloat *result, const BigFloat *a, const BigFloat *b) {
    if ((a->mantissa.size == 1 && a->mantissa.limbs[0] == 0) ||
        (b->mantissa.size == 1 && b->mantissa.limbs[0] == 0)) {
        bigFloatZero(result);
        return(0);
    }

    int rc = bigIntMulFFT(&result->mantissa, &a->mantissa, &b->mantissa);
    if (rc != 0){ return(INT_MAX); }

    int64_t new_exp = (int64_t)a->exp + (int64_t)b->exp;
    if (new_exp > INT32_MAX || new_exp < INT32_MIN){ return(INT_MAX); }

    result->exp  = (int32_t)new_exp;
    result->sign = a->sign * b->sign;
    return(bigFloatNormalize(result));
}
//Make it work first...then format as you please 

/* ---------- BigFloat addition ---------- */
int bigFloatAdd(BigFloat *result, const BigFloat *a, const BigFloat *b) {
    if (a->mantissa.size == 1 && a->mantissa.limbs[0] == 0) {
        bigFloatCopy(result, b); return 0;
    }

    if (b->mantissa.size == 1 && b->mantissa.limbs[0] == 0) {
        bigFloatCopy(result, a); return 0;
    }

    if (a->sign == b->sign) {
        BigFloat tmp_a, tmp_b;
        bigFloatCopy(&tmp_a, a);
        bigFloatCopy(&tmp_b, b);

        // align by shifting the smaller exponent's mantissa right
        if (tmp_a.exp > tmp_b.exp) {
            int shift = tmp_a.exp - tmp_b.exp;
            if (bigIntShiftRight(&tmp_b.mantissa, shift) != 0) return -1;
            tmp_b.exp = tmp_a.exp;
        } else if (tmp_b.exp > tmp_a.exp) {
            int shift = tmp_b.exp - tmp_a.exp;
            if (bigIntShiftRight(&tmp_a.mantissa, shift) != 0) return -1;
            tmp_a.exp = tmp_b.exp;
        }

        BigInt sum; bigIntZero(&sum);
        uint64_t carry = 0;
        int max_size = (tmp_a.mantissa.size > tmp_b.mantissa.size) ?
                        tmp_a.mantissa.size : tmp_b.mantissa.size;
        for (int i = 0; i < max_size || carry; i++) {
            if (i >= MAX_LIMBS) return INT_MAX;
            uint64_t av = (i < tmp_a.mantissa.size) ? tmp_a.mantissa.limbs[i] : 0;
            uint64_t bv = (i < tmp_b.mantissa.size) ? tmp_b.mantissa.limbs[i] : 0;
            uint64_t s  = av + bv + carry;
            sum.limbs[i] = (uint32_t)s;
            carry = s >> 32;
            sum.size = i + 1;
        }
        result->mantissa = sum;
        result->exp  = tmp_a.exp;
        result->sign = a->sign;
        return(bigFloatNormalize(result));
    }

    int cmp = bigFloatCmpAbs(a, b);
    if (cmp == 0) { bigFloatZero(result); return 0; }
    const BigFloat *larger  = (cmp > 0) ? a : b;
    const BigFloat *smaller = (cmp > 0) ? b : a;

    BigFloat tmp_large, tmp_small;
    bigFloatCopy(&tmp_large, larger);
    bigFloatCopy(&tmp_small, smaller);

    if (tmp_large.exp > tmp_small.exp) {
        int shift = tmp_large.exp - tmp_small.exp;
        if (bigIntShiftRight(&tmp_small.mantissa, shift) != 0) return -1;
        tmp_small.exp = tmp_large.exp;
    } else if (tmp_small.exp > tmp_large.exp) {
        int shift = tmp_small.exp - tmp_large.exp;
        if (bigIntShiftRight(&tmp_large.mantissa, shift) != 0) return -1;
        tmp_large.exp = tmp_small.exp;
    }

    if (bigIntSub(&result->mantissa, &tmp_large.mantissa, &tmp_small.mantissa) != 0){ return(-1); }
    
    result->exp  = tmp_large.exp;
    result->sign = larger->sign;
    return(bigFloatNormalize(result));
}


/* result = a - b */
int bigFloatSub(BigFloat *result, const BigFloat *a, const BigFloat *b) {
    BigFloat neg_b;
    bigFloatCopy(&neg_b, b);
    neg_b.sign = -b->sign;
    return(bigFloatAdd(result, a, &neg_b));
}

/* ---------- Reciprocal, division, sqrt ---------- */
int bigFloatReciprocal(BigFloat *result, const BigFloat *x, int target_limbs)
{
    if (x->mantissa.size == 1 && x->mantissa.limbs[0] == 0) { return INT_MAX; }  // division by zero

    if (target_limbs < 1){ target_limbs = 1; }
    if (target_limbs > MAX_LIMBS){ target_limbs = MAX_LIMBS; }

    BigFloat y;
    bigFloatZero(&y);

    // --- NEW BULLETPROOF BITWISE SEED BITCH ---
    // Mathematically guarantees 1 <= x * y < 2
    uint32_t high = x->mantissa.limbs[x->mantissa.size - 1];
    int total_bits = 32 * (x->mantissa.size - 1) + (32 - clz32(high));

    y.mantissa.limbs[0] = 1;
    y.mantissa.size     = 1;
    y.exp               = -total_bits + 1 - x->exp;
    y.sign              = x->sign;

    // ---------- Newton: y = y * (2 - x*y) ----------

    BigFloat two, t1, t2, t3;
    bigFloatFromUint32(&two, 2);

    // 6 base iterations guarantees 32-bit precision from the seed, 
    // plus log2(target) iterations to scale up to massive sizes.
    
 int working_limbs = target_limbs + 2; 
    if (working_limbs > MAX_LIMBS) working_limbs = MAX_LIMBS;

    int required_iters = 6; 
    int temp = 1;
    while (temp < working_limbs) { // <- Use working_limbs here!
        required_iters++;
        temp *= 2;
    }

    for (int i = 0; i < required_iters; i++) {
        int rc = bigFloatMul(&t1, x, &y);
        if (rc != 0){ return(rc); }

        rc = bigFloatSub(&t2, &two, &t1);
        if (rc != 0){ return(rc); }

        rc = bigFloatMul(&t3, &y, &t2);
        if (rc != 0){ return(rc); }

        bigFloatCopy(&y, &t3);
    }
    
    bigFloatCopy(result, &y);
    
    // BOOM. CHOP THE NOISE OFF BEFORE ANYONE SEES IT.
    bigFloatTruncate(result, target_limbs); 

    return(bigFloatNormalize(result));
}

int bigFloatDiv(BigFloat *result, const BigFloat *a, const BigFloat *b, int target_limbs) {
    BigFloat recip;
    int rc = bigFloatReciprocal(&recip, b, target_limbs);
    if (rc != 0) return rc;
    return bigFloatMul(result, a, &recip);
}


int bigFloatSqrt(BigFloat *result, const BigFloat *x, int target_limbs)
{
    if (x->sign < 0){ return(-1); } // negative

    if (x->mantissa.size == 1 && x->mantissa.limbs[0] == 0) {
        bigFloatZero(result);
        return(0);
    }

    if (target_limbs < 1) target_limbs = 1;
    if (target_limbs > MAX_LIMBS) target_limbs = MAX_LIMBS;

    BigFloat y;
    bigFloatZero(&y);

    // --- NEW BULLETPROOF BITWISE SEED ---
    uint32_t high = x->mantissa.limbs[x->mantissa.size - 1];
    int total_bits = 32 * (x->mantissa.size - 1) + (32 - clz32(high));
    
    // Calculate the exact true exponent of the magnitude
    int true_exp = total_bits - 1 + x->exp;

    y.mantissa.limbs[0] = 1;
    y.mantissa.size = 1;
    // Floor division for negative odd numbers requires a tiny tweak
    y.exp = (true_exp >= 0) ? (true_exp / 2) : ((true_exp - 1) / 2);
    y.sign = 1;

// ---------- Newton: y = (y + x/y) / 2 ----------
BigFloat t1, t2;
    
    // WE ADD 2 GUARD LIMBS TO PUSH THE NOISE DOWN!
    int working_limbs = target_limbs + 2;
    if (working_limbs > MAX_LIMBS) working_limbs = MAX_LIMBS;

    int required_iters = 6;
    int temp = 1;
    while (temp < working_limbs) {
        required_iters++;
        temp *= 2;
    }

    for (int i = 0; i < required_iters; i++) {
        // Pass MAX_LIMBS to division so intermediate steps don't truncate early!
        if (bigFloatDiv(&t1, x, &y, MAX_LIMBS) != 0) return(-1);
        if (bigFloatAdd(&t2, &y, &t1) != 0) return(-1);

        t2.exp -= 1; 

        bigFloatCopy(&y, &t2);
    }
    
    bigFloatCopy(result, &y);
    
    // BOOM. CHOP THE NOISE OFF BEFORE ANYONE SEES IT (I am The Documentation Neuro hii...next commit will have my signatures (yeah I am clearing my old notes for myself)).
    bigFloatTruncate(result, target_limbs);
    
    return(bigFloatNormalize(result));
}
