/* Binary floating point with explicit precision and dynamic exact scratch. */
#include "bignums_internal.h"

int bn_precision(size_t limbs) { return(!limbs ? -2 : limbs > BN_MAX_LIMBS ? INT_MAX : 0); }

void bigFloatInit(BigFloat *x) {
    bigIntInit(&x->mantissa);

    x->exp = 0;
    x->sign = 1;
    x->precision = BIGFLOAT_DEFAULT_PRECISION;
}

void bigFloatDestroy(BigFloat *x) {
    bigIntDestroy(&x->mantissa);
    bigFloatInit(x);
}

void bigFloatZero(BigFloat *x) {
    bigIntZero(&x->mantissa);

    x->exp = 0;
    x->sign = 1;
}

void bigFloatSwap(BigFloat *a, BigFloat *b) {
    BigFloat tmp = *a;
    *a = *b;
    *b = tmp;
}

int bigFloatCopy(BigFloat *dst, const BigFloat *src) {
    if(dst == src) { return(0); }

    int rc = bigIntCopy(&dst->mantissa, &src->mantissa);

    if(!rc) {
        dst->exp = src->exp;
        dst->sign = src->sign;
        dst->precision = src->precision;
    }

    return(rc);
}
/* Scratch m may change on failure 
The owning public output changes only once
 * all allocation, rounding and exponent checks have succeeded. */


 int bn_float_pack(BigFloat *out, BigInt *m, int64_t exp, int sign, size_t limbs) {

    int rc = bn_precision(limbs);

    if(rc) { return(rc); }

    size_t bits = bn_bits(m), precision = 32 * limbs;

    if(!bits) {
        bigFloatZero(out);
        out->precision = limbs;
        return(0);
    }

    if(bits > precision) {
        (void)bn_right(m, bits - precision);
        exp += (int64_t)(bits - precision);
        bits = precision;
    }

    unsigned shift = (unsigned)((32 - bits % 32) % 32);

    rc = bn_left(m, shift);

    if(rc) { return(rc); }

    exp -= shift;

    /* Remove multiple low zero limbs bundled near the lower exponent bound */
    if(exp < INT32_MIN) {
        size_t zeros = 0;
        
        while(zeros + 1 < m->size && !m->limbs[zeros]) { ++zeros; }
        
        uint64_t needed = ((uint64_t)(INT32_MIN - exp) + 31) / 32;
        
        size_t words = needed < zeros ? (size_t)needed : zeros;
        
        (void)bn_right(m, 32 * words);
        
        exp += (int64_t)(32 * words);
    }
    
    if(exp > INT32_MAX) {
        uint64_t words = ((uint64_t)(exp - INT32_MAX) + 31) / 32;
        if(words > limbs - m->size) { return(INT_MAX); }
        
        rc = bn_left(m, words * 32);
        if(rc) { return(rc); }
        
        exp -= (int64_t)(words * 32);
    }
    
    if(exp < INT32_MIN || exp > INT32_MAX) { return(INT_MAX); }
    
    BigFloat packed = BIGFLOAT_INIT;
    
    bigIntSwap(&packed.mantissa, m);
    
    packed.exp = (int32_t)exp;
    packed.sign = sign;
    packed.precision = limbs;
    
    bigFloatSwap(out, &packed);
    bigFloatDestroy(&packed);
    
    return(0);
}

int bigFloatFromUint32(BigFloat *x, uint32_t v) {
    BigInt m = BIGINT_INIT;

    int rc = int_32ToBigInt(&m, v);
    if(!rc) { rc = bn_float_pack(x, &m, 0, 1, x->precision); }
    
    bigIntDestroy(&m);
    
    return(rc);
}

int bigFloatSetPrecision(BigFloat *x, size_t limbs) {
    int rc = bn_precision(limbs);
    if(rc) { return(rc); }

    BigInt m = BIGINT_INIT;

    rc = bigIntCopy(&m, &x->mantissa);
    if(!rc) { rc = bn_float_pack(x, &m, x->exp, x->sign, limbs); }

    bigIntDestroy(&m);
    return(rc);
}

int bigFloatTruncate(BigFloat *x, size_t limbs) { return(bigFloatSetPrecision(x, limbs)); }

int bigFloatNormalize(BigFloat *x) { return(bigFloatSetPrecision(x, x->precision)); }

static int float_shift(BigFloat *x, int64_t bits, int left) {
    if(!bits) { return(0); }

    BigInt m = BIGINT_INIT;

    int rc = bigIntCopy(&m, &x->mantissa);

    if(!rc) { rc = left ? bigIntShiftLeft(&m, bits) : bigIntShiftRight(&m, bits); }
    if(!rc) { rc = bn_float_pack(x, &m, x->exp, x->sign, x->precision); }

    bigIntDestroy(&m);
    return(rc);
}

int bigFloatShiftLeft(BigFloat *x, int64_t bits) { return(float_shift(x, bits, 1)); }

int bigFloatShiftRight(BigFloat *x, int64_t bits) { return(float_shift(x, bits, 0)); }

int bigFloatCmpAbs(const BigFloat *a, const BigFloat *b) {
    size_t ab = bn_bits(&a->mantissa), bb = bn_bits(&b->mantissa);

    if(!ab || !bb) { return((ab != 0) - (bb != 0)); }

    int64_t atop = (int64_t)a->exp + (int64_t)ab;
    int64_t btop = (int64_t)b->exp + (int64_t)bb;

    if(atop != btop) { return(atop > btop ? 1 : -1); }

    size_t count = ab > bb ? ab : bb;

    for(size_t i = 0; i < count; ++i) {
        int av = i < ab ? bigIntGetBit(&a->mantissa, ab - 1 - i) : 0;
        int bv = i < bb ? bigIntGetBit(&b->mantissa, bb - 1 - i) : 0;
        if(av != bv) { return(av > bv ? 1 : -1); }
    }

    return(0);
}

int bigFloatMul(BigFloat *out, const BigFloat *a, const BigFloat *b) {

    BigInt product = BIGINT_INIT;

    int rc = bn_precision(out->precision);

    if(!rc) { rc = bigIntMul(&product, &a->mantissa, &b->mantissa); }

    if(!rc) {
        rc = bn_float_pack(out, &product, (int64_t)a->exp + b->exp, a->sign * b->sign,
                           out->precision);
    }

    bigIntDestroy(&product);

    return(rc);
}


static int float_add(BigFloat *out, const BigFloat *a, const BigFloat *b, int bsign) {
    BigInt am = BIGINT_INIT, bm = BIGINT_INIT, one = BIGINT_INIT;

    int rc = bn_precision(out->precision);
    if(rc) { return(rc); }

    rc = bigIntCopy(&am, &a->mantissa);
    if(!rc) { rc = bigIntCopy(&bm, &b->mantissa); }
    if(rc) { goto done; }

    size_t ab = bn_bits(&am), bb = bn_bits(&bm);

    if(!ab) {
        rc = bn_float_pack(out, &bm, b->exp, bsign, out->precision);
        goto done;
    }

    if(!bb) {
        rc = bn_float_pack(out, &am, a->exp, a->sign, out->precision);
        goto done;
    }

    int cmp = bigFloatCmpAbs(a, b);

    if(a->sign != bsign && !cmp) {
        bigFloatZero(out);
        goto done;
    }

    /* Inclde both ipnut widths as well as output precision This preserves
     * cancellation for inputs more precise than the destination while at
     * most one operand loses a tail across widely separated exponents...OwO */
    size_t work = out->precision * 32;
    if(work < ab) { work = ab; }
    if(work < bb) { work = bb; }
    
    int64_t topa = (int64_t)a->exp + (int64_t)ab;
    int64_t topb = (int64_t)b->exp + (int64_t)bb;
    int64_t exp = a->exp < b->exp ? a->exp : b->exp;
    int64_t floor_exp = (topa > topb ? topa : topb) - (int64_t)work - 2;
    
    if(exp < floor_exp) { exp = floor_exp; }
    
    int lost_a = 0, lost_b = 0;
    
    if(a->exp >= exp) {
        rc = bn_left(&am, (uint64_t)(a->exp - exp));
    } 
    
    else {
        lost_a = bn_right(&am, (uint64_t)(exp - a->exp));
    }
    
    if(rc) { goto done; }
    
    if(b->exp >= exp) {
        rc = bn_left(&bm, (uint64_t)(b->exp - exp));
    } 
    
    else {
        lost_b = bn_right(&bm, (uint64_t)(exp - b->exp));
    }
    
    if(rc) { goto done; }

    int sign = a->sign;
    
    if(a->sign == bsign) {
        rc = bn_add(&am, &bm);
    } 
    
    else {
        BigInt *larger = cmp > 0 ? &am : &bm;
        BigInt *smaller = cmp > 0 ? &bm : &am;
    
        int lost_smaller = cmp > 0 ? lost_b : lost_a;
    
        sign = cmp > 0 ? a->sign : bsign;
    
        bn_sub(larger, smaller);
    
        if(lost_smaller) {
            rc = int_32ToBigInt(&one, 1);
            if(rc) { goto done; }
            bn_sub(larger, &one);
        }
    
        if(larger != &am) { bigIntSwap(&am, &bm); }
    }
    
    if(!rc) { rc = bn_float_pack(out, &am, exp, sign, out->precision); }
done:
    bigIntDestroy(&am);
    bigIntDestroy(&bm);
    bigIntDestroy(&one);
    
    return(rc);
}

int bigFloatAdd(BigFloat *out, const BigFloat *a, const BigFloat *b) {
    return(float_add(out, a, b, b->sign));
}

int bigFloatSub(BigFloat *out, const BigFloat *a, const BigFloat *b) {
    return(float_add(out, a, b, -b->sign));
}

int bigFloatDiv(BigFloat *out, const BigFloat *a, const BigFloat *b, size_t limbs) {
    int rc = bn_precision(limbs);
    if(rc) { return(rc); }

    if(!b->mantissa.size) { return(-2); }
    if(!a->mantissa.size) {
        bigFloatZero(out);
        out->precision = limbs;
        return(0);
    }

    BigInt numerator = BIGINT_INIT, denominator = BIGINT_INIT;
    BigInt cmpa = BIGINT_INIT, cmpb = BIGINT_INIT, quotient = BIGINT_INIT;

    rc = bigIntCopy(&numerator, &a->mantissa);
    if(!rc) { rc = bigIntCopy(&denominator, &b->mantissa); }
    if(!rc) { rc = bigIntCopy(&cmpa, &numerator); }
    if(!rc) { rc = bigIntCopy(&cmpb, &denominator); }
    if(rc) { goto done; }

    int64_t magnitude = (int64_t)bn_bits(&numerator) - (int64_t)bn_bits(&denominator);
    rc = magnitude >= 0 ? bn_left(&cmpb, (uint64_t)magnitude) : bn_left(&cmpa, (uint64_t)-magnitude);
    
    if(rc) { goto done; }
    
    if(bigIntCmpAbs(&cmpa, &cmpb) < 0) { --magnitude; }
    
    int64_t shift = (int64_t)(32 * limbs) - 1 - magnitude;
    rc = shift >= 0 ? bn_left(&numerator, (uint64_t)shift) : bn_left(&denominator, (uint64_t)-shift);
    
    if(!rc) { rc = bn_div(&quotient, &numerator, &denominator); }
    if(!rc) { rc = bn_float_pack(out, &quotient, (int64_t)a->exp - b->exp - shift, a->sign * b->sign, limbs); }

done:
    bigIntDestroy(&numerator);
    bigIntDestroy(&denominator);
    bigIntDestroy(&cmpa);
    bigIntDestroy(&cmpb);
    bigIntDestroy(&quotient);

    return(rc);
}


int bigFloatReciprocal(BigFloat *out, const BigFloat *x, size_t limbs) {

    BigFloat one = BIGFLOAT_INIT;

    int rc = bigFloatFromUint32(&one, 1);

    if(!rc) { rc = bigFloatDiv(out, &one, x, limbs); }
    bigFloatDestroy(&one);
    return(rc);
}

int bigFloatSqrt(BigFloat *out, const BigFloat *x, size_t limbs) {

    int rc = bn_precision(limbs);
    if(rc) { return(rc); }

    if(x->sign < 0) { return(-2); }

    if(!x->mantissa.size) {
        bigFloatZero(out);
        out->precision = limbs;
        return(0);
    }
    
    BigInt radicand = BIGINT_INIT, root = BIGINT_INIT;
    
    rc = bigIntCopy(&radicand, &x->mantissa);
    
    if(rc) { goto done; }
    
    int64_t top = (int64_t)x->exp + (int64_t)bn_bits(&radicand) - 1;
    int64_t magnitude = top >= 0 ? top / 2 : -((-top + 1) / 2);
    int64_t exp = magnitude - ((int64_t)(32 * limbs) - 1);
    int64_t shift = (int64_t)x->exp - 2 * exp;
    
    if(shift >= 0) {
        rc = bn_left(&radicand, (uint64_t)shift);
    } 
    else {
        (void)bn_right(&radicand, (uint64_t)-shift);
    }
    
    if(!rc) { rc = bn_sqrt(&root, &radicand); }
    if(!rc) { rc = bn_float_pack(out, &root, exp, 1, limbs); }

done:
    bigIntDestroy(&radicand);
    bigIntDestroy(&root);   //sudo su
    return(rc);
}
