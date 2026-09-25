/* REFACTOR...*/
#include "bignums_internal.h"
#include <stdlib.h>
#include <string.h>

void bigIntInit(BigInt *a) {
    a->limbs = NULL;
    a->size = a->capacity = 0;
    a->sign = 1;
}

void bigIntDestroy(BigInt *a) {
    free(a->limbs);
    bigIntInit(a);
}

void bigIntZero(BigInt *a) {
    a->size = 0;
    a->sign = 1;
}

void bigIntNegate(BigInt *a) {
    if(a->size) { a->sign = -a->sign; }
}

void bigIntSwap(BigInt *a, BigInt *b) {
    BigInt tmp = *a;
    *a = *b;
    *b = tmp; /* NOT COPY OWNERSHIP EXCHANGE. */
}

int bigIntReserve(BigInt *a, size_t limbs) {
    if(limbs > BN_MAX_LIMBS) { return(INT_MAX); }
    if(limbs <= a->capacity) { return(0); }

    size_t capacity = a->capacity ? a->capacity : 4;

    if(capacity > BN_MAX_LIMBS) { capacity = BN_MAX_LIMBS; }

    while(capacity < limbs) {
        if(capacity > BN_MAX_LIMBS / 2) {
            capacity = limbs;
            break;
        }
        capacity *= 2;
    }

    uint32_t *memory = realloc(a->limbs, capacity * sizeof(*memory));

    if(!memory) { return(-1); }

    a->limbs = memory;
    a->capacity = capacity;

    return(0);
}

int bigIntCopy(BigInt *dst, const BigInt *src) {
    if(dst == src) { return(0); }

    int rc = bigIntReserve(dst, src->size);

    if(rc) { return(rc); }
    if(src->size) { memcpy(dst->limbs, src->limbs, src->size * sizeof(*src->limbs)); }

    dst->size = src->size;
    dst->sign = src->sign;

    return(0);
}

void bn_trim(BigInt *a) {
    while(a->size && !a->limbs[a->size - 1]) {
        --a->size;
    }

    if(!a->size) { a->sign = 1; }
}

size_t bn_bits(const BigInt *a) {
    if(!a->size) { return(0); }

    uint32_t top = a->limbs[a->size - 1];
    unsigned bits = 0;

    while(top) {
        ++bits;
        top >>= 1;
    }

    return(32 * (a->size - 1) + bits);
}

int int_32ToBigInt(BigInt *a, uint32_t val) {
    if(!val) {
        bigIntZero(a);
        return(0);
    }

    int rc = bigIntReserve(a, 1);

    if(rc) { return(rc); }
    a->limbs[0] = val;
    a->size = 1;
    a->sign = 1;

    return(0);
}

int bigIntFromInt64(BigInt *a, int64_t val) {
    if(!val) {
        bigIntZero(a);
        return(0);
    }

    uint64_t magnitude = val < 0 ? (uint64_t)(-(val + 1)) + 1 : (uint64_t)val;

    size_t size = magnitude > UINT32_MAX ? 2 : 1;
    int rc = bigIntReserve(a, size);
    if(rc) { return(rc); }

    a->limbs[0] = (uint32_t)magnitude;
    if(size == 2) { a->limbs[1] = (uint32_t)(magnitude >> 32); }

    a->size = size;
    a->sign = val < 0 ? -1 : 1;

    return(0);
}

int bigIntAddUInt_32(BigInt *a, uint32_t b) {
    if(!b) { return(0); }
    if(a->sign < 0) {
        if(a->size == 1 && a->limbs[0] <= b) {
            a->limbs[0] = b - a->limbs[0];
            a->sign = 1;
        } 
        
        else {
            uint64_t borrow = b;
            for(size_t i = 0; borrow && i < a->size; ++i) {
                uint64_t v = a->limbs[i];
                a->limbs[i] = (uint32_t)(v - borrow);
                borrow = v < borrow;
            }
        }
        
        bn_trim(a);
        return(0);
    }

    if(a->size == BN_MAX_LIMBS) { return(INT_MAX); }
    
    int rc = bigIntReserve(a, a->size + 1);
    if(rc) { return(rc); }
    
    uint64_t carry = b;
    for(size_t i = 0; carry && i < a->size; ++i) {
        carry += a->limbs[i];
        a->limbs[i] = (uint32_t)carry;
        carry >>= 32;
    }
    
    if(carry) { a->limbs[a->size++] = (uint32_t)carry; }
    return(0);
}

int bigIntMulUInt_32(BigInt *a, uint32_t b) {
    if(!b) {
        bigIntZero(a);
        return(0);
    }
    if(!a->size || b == 1) { return(0); }
    if(a->size == BN_MAX_LIMBS) { return(INT_MAX); }

    int rc = bigIntReserve(a, a->size + 1);
    if(rc) { return(rc); }

    uint64_t carry = 0;
    for(size_t i = 0; i < a->size; ++i) {
        uint64_t v = (uint64_t)a->limbs[i] * b + carry;
        a->limbs[i] = (uint32_t)v;
        carry = v >> 32;
    }

    if(carry) { a->limbs[a->size++] = (uint32_t)carry; }
    return(0);

}
int64_t bigIntModUInt32(const BigInt *a, uint32_t divisor) {
    if(!divisor) { return(INT64_MIN); }

    uint64_t rem = 0;
    for(size_t i = a->size; i-- > 0;) {
        rem = ((rem << 32) | a->limbs[i]) % divisor;
    }

    return(a->sign * (int64_t)rem);
}

int64_t bigIntDivUInt32(BigInt *a, uint32_t divisor) {
    if(!divisor) { return(INT64_MIN); }
    int sign = a->sign;
    uint64_t rem = 0;

    for(size_t i = a->size; i-- > 0;) {
        uint64_t v = (rem << 32) | a->limbs[i];
        a->limbs[i] = (uint32_t)(v / divisor);
        rem = v % divisor;
    }

    bn_trim(a);
    return(sign * (int64_t)rem);
}

int bigIntFromString(BigInt *a, const char *text) {
    BigInt tmp = BIGINT_INIT;
    int rc = 0;
    if(!text) { return(-2); }

    int sign = 1;

    if(*text == '+' || *text == '-') {
        sign = *text++ == '-' ? -1 : 1;
        if(!*text) { return(-2); }
    }

    for(; *text; ++text) {
        if(*text < '0' || *text > '9') {
            rc = -2;
            break;
        }
        rc = bigIntMulUInt_32(&tmp, 10);
        if(!rc) { rc = bigIntAddUInt_32(&tmp, (uint32_t)(*text - '0')); }
        if(rc) { break; }
    }

    if(!rc) {
        tmp.sign = tmp.size ? sign : 1;
        bigIntSwap(a, &tmp);
    }

    bigIntDestroy(&tmp);
    return(rc);
}

int bigIntFactorial(BigInt *result, uint32_t n) {

    BigInt tmp = BIGINT_INIT;

    int rc = int_32ToBigInt(&tmp, 1);
    /* A 64-bit counter also terminates for n == UINT32_MAX. */
    for(uint64_t i = 2; !rc && i <= n; ++i) {
        rc = bigIntMulUInt_32(&tmp, (uint32_t)i);
    }

    if(!rc) { bigIntSwap(result, &tmp); }

    bigIntDestroy(&tmp);
    return(rc);
}

int bigIntGetBit(const BigInt *a, size_t bit) {
    return(bit / 32 < a->size ? (int)((a->limbs[bit / 32] >> (bit % 32)) & 1u) : 0);
}

int bigIntCmpAbs(const BigInt *a, const BigInt *b) {
    if(a->size != b->size) { return(a->size > b->size ? 1 : -1); }

    for(size_t i = a->size; i-- > 0;) {
        if(a->limbs[i] != b->limbs[i]) { return(a->limbs[i] > b->limbs[i] ? 1 : -1); }
    }

    return(0);
}

int bigIntCmp(const BigInt *a, const BigInt *b) {
    if(a->sign != b->sign) { return(a->sign > b->sign ? 1 : -1); }
    return(a->sign * bigIntCmpAbs(a, b));
}

int bn_add(BigInt *a, const BigInt *b) {
    size_t size = a->size > b->size ? a->size : b->size;

    if(!size) { return(0); }
    if(size == BN_MAX_LIMBS) { return(INT_MAX); }

    int rc = bigIntReserve(a, size + 1);

    if(rc) { return(rc); }

    uint64_t carry = 0;
    for(size_t i = 0; i < size; ++i) {
        uint64_t av = i < a->size ? a->limbs[i] : 0;
        uint64_t bv = i < b->size ? b->limbs[i] : 0;
        uint64_t v = av + bv + carry;
        a->limbs[i] = (uint32_t)v;
        carry = v >> 32;
    }

    a->size = size;

    if(carry) { a->limbs[a->size++] = (uint32_t)carry; }

    return(0);
}

void bn_sub(BigInt *a, const BigInt *b) {
    uint64_t borrow = 0;

    for(size_t i = 0; i < a->size; ++i) {
        uint64_t av = a->limbs[i];
        uint64_t bv = (i < b->size ? (uint64_t)b->limbs[i] : 0) + borrow;
        a->limbs[i] = (uint32_t)(av - bv);
        borrow = av < bv;
    }

    bn_trim(a);
}

static int signed_add(BigInt *out, const BigInt *a, const BigInt *b, int bsign) {
    BigInt tmp = BIGINT_INIT;

    int rc, sign;
    if(a->sign == bsign) {
        sign = a->sign;
        rc = bigIntCopy(&tmp, a);
        if(!rc) { rc = bn_add(&tmp, b); }
    } 
    
    else {
        int cmp = bigIntCmpAbs(a, b);
    
        sign = cmp >= 0 ? a->sign : bsign;
        rc = bigIntCopy(&tmp, cmp >= 0 ? a : b);
    
        if(!rc) { bn_sub(&tmp, cmp >= 0 ? b : a); }
    }
    
    if(!rc) {
        tmp.sign = tmp.size ? sign : 1;
        bigIntSwap(out, &tmp);
    }
    
    bigIntDestroy(&tmp);
    return(rc);
}

int bigIntAdd(BigInt *out, const BigInt *a, const BigInt *b) {
    return(signed_add(out, a, b, b->sign));
}

int bigIntSub(BigInt *out, const BigInt *a, const BigInt *b) {
    return(signed_add(out, a, b, -b->sign));
}

int bn_left(BigInt *a, uint64_t bits) {
    if(!bits || !a->size) { return(0); }

    uint64_t words64 = bits / 32;
    unsigned shift = (unsigned)(bits % 32);
    size_t carry_limb = shift && (a->limbs[a->size - 1] >> (32 - shift));

    if(words64 > BN_MAX_LIMBS - a->size || carry_limb > BN_MAX_LIMBS - a->size - (size_t)words64) {
        return(INT_MAX);
    }

    size_t words = (size_t)words64;
    int rc = bigIntReserve(a, a->size + words + carry_limb);

    if(rc) { return(rc); }
    if(words) {
        memmove(a->limbs + words, a->limbs, a->size * sizeof(*a->limbs));
        memset(a->limbs, 0, words * sizeof(*a->limbs));
    }

    uint64_t carry = 0;

    for(size_t i = words; shift && i < a->size + words; ++i) {
        uint64_t v = ((uint64_t)a->limbs[i] << shift) | carry;
        a->limbs[i] = (uint32_t)v;
        carry = v >> 32;
    }

    a->size += words;

    if(carry) { a->limbs[a->size++] = (uint32_t)carry; }

    return(0);
}

int bn_right(BigInt *a, uint64_t bits) {
    if(!bits || !a->size) { return(0); }
    if(bits >= bn_bits(a)) {
        bigIntZero(a);
        return(1);
    }

    size_t words = (size_t)(bits / 32);
    unsigned shift = (unsigned)(bits % 32);
    int lost = 0;

    for(size_t i = 0; i < words; ++i) {
        lost |= a->limbs[i] != 0;
    }

    if(shift) { lost |= (a->limbs[words] & (UINT32_MAX >> (32 - shift))) != 0; }

    size_t size = a->size - words;

    for(size_t i = 0; i < size; ++i) {
        uint64_t v = a->limbs[i + words];
        if(shift && i + words + 1 < a->size) { v |= (uint64_t)a->limbs[i + words + 1] << 32; }
        a->limbs[i] = (uint32_t)(v >> shift);
    }

    a->size = size;

    bn_trim(a);
    return(lost);
}

int bigIntShiftLeft(BigInt *a, int64_t bits) {
    if(bits < 0) {
        (void)bn_right(a, (uint64_t)(-(bits + 1)) + 1);
        return(0);
    }
    return(bn_left(a, (uint64_t)bits));
}

int bigIntShiftRight(BigInt *a, int64_t bits) {
    if(bits < 0) { return(bn_left(a, (uint64_t)(-(bits + 1)) + 1)); }
    (void)bn_right(a, (uint64_t)bits);

    return(0);
}

int bn_mul_schoolbook(BigInt *out, const BigInt *a, const BigInt *b) {
    if(!a->size || !b->size) {
        bigIntZero(out);
        return(0);
    }

    if(a->size > BN_MAX_LIMBS - b->size) { return(INT_MAX); }

    size_t size = a->size + b->size;
    BigInt tmp = BIGINT_INIT;

    int rc = bigIntReserve(&tmp, size);
    if(!rc) {
        memset(tmp.limbs, 0, size * sizeof(*tmp.limbs));
        for(size_t i = 0; i < a->size; ++i) {
            uint64_t carry = 0;
            for(size_t j = 0; j < b->size; ++j) {
                uint64_t v = (uint64_t)a->limbs[i] * b->limbs[j] + tmp.limbs[i + j] + carry;
                tmp.limbs[i + j] = (uint32_t)v;
                carry = v >> 32;
            }
            tmp.limbs[i + b->size] = (uint32_t)carry;
        }
        tmp.size = size;
        bn_trim(&tmp);
        bigIntSwap(out, &tmp);
    }

    bigIntDestroy(&tmp);
    return(rc);
}
/* All scratch allocations precede the bit loop. Helpers reuse that capacity(allegedly). */

int bn_div(BigInt *out, const BigInt *a, const BigInt *b) {
    if(!b->size) { return(-2); }
    
    BigInt q = BIGINT_INIT, rem = BIGINT_INIT;
    
    int rc = bigIntReserve(&q, a->size);
    
    if(!rc) { rc = bigIntReserve(&rem, b->size + 2); }
    if(rc) { goto done; }
    if(a->size) { memset(q.limbs, 0, a->size * sizeof(*q.limbs)); }
    
    for(size_t bit = bn_bits(a); bit-- > 0;) {
        rc = bn_left(&rem, 1);
    
        if(!rc) { rc = bigIntAddUInt_32(&rem, (uint32_t)bigIntGetBit(a, bit)); }
        if(rc) { goto done; }
    
        if(bigIntCmpAbs(&rem, b) >= 0) {
            bn_sub(&rem, b);
            q.limbs[bit / 32] |= UINT32_C(1) << (bit % 32);
            if(q.size < bit / 32 + 1) { q.size = bit / 32 + 1; }
        }
    }
    
    bigIntSwap(out, &q);

done:
    bigIntDestroy(&q);
    bigIntDestroy(&rem);
    return(rc);
}

int bn_sqrt(BigInt *out, const BigInt *a) {
    BigInt root = BIGINT_INIT, rem = BIGINT_INIT, trial = BIGINT_INIT;

    size_t space = a->size / 2 + 3;
    int rc = bigIntReserve(&root, space);
    if(!rc) { rc = bigIntReserve(&rem, space); }
    if(!rc) { rc = bigIntReserve(&trial, space); }
    if(rc) { goto done; }

    for(size_t pair = (bn_bits(a) + 1) / 2; pair-- > 0;) {
        rc = bn_left(&rem, 2);
        if(!rc) {
            rc = bigIntAddUInt_32(
                &rem, (uint32_t)(2 * bigIntGetBit(a, 2 * pair + 1) + bigIntGetBit(a, 2 * pair)));
        }
        if(!rc) { rc = bn_left(&root, 1); }
        if(!rc) { rc = bigIntCopy(&trial, &root); }
        if(!rc) { rc = bn_left(&trial, 1); }
        if(!rc) { rc = bigIntAddUInt_32(&trial, 1); }
        if(rc) { goto done; }

        if(bigIntCmpAbs(&rem, &trial) >= 0) {
            bn_sub(&rem, &trial);
            rc = bigIntAddUInt_32(&root, 1);
            if(rc) { goto done; }
        }
    }

    bigIntSwap(out, &root);

done:
    bigIntDestroy(&root);
    bigIntDestroy(&rem);
    bigIntDestroy(&trial);
    return(rc);
}

/*Finally...*/