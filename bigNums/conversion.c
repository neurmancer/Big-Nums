#include "bignums_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int bigIntToString(const BigInt *a, char **text) {

    if(!text) { return(-2); }
    if(a->size > (SIZE_MAX - 2) / 10) { return(INT_MAX); }
    //I'll format that into my usual style eventually...
    size_t capacity = a->size ? 10 * a->size + 2 : 2;
    char *digits = malloc(capacity);

    if(!digits) { return(-1); }

    BigInt tmp = BIGINT_INIT;

    int rc = bigIntCopy(&tmp, a);

    if(rc) {
        free(digits);
        return(rc);
    }

    tmp.sign = 1; /* Decimal digits come from the absolute value. */

    size_t pos = capacity - 1;

    digits[pos] = '\0';

    do {
        uint32_t rem = (uint32_t)bigIntDivUInt32(&tmp, UINT32_C(1000000000));
        unsigned count = 0;
        do {
            digits[--pos] = (char)('0' + rem % 10);
            rem /= 10;
            ++count;
        } while(rem || (tmp.size && count < 9));
    } while(tmp.size);
    if(a->sign < 0 && a->size) { digits[--pos] = '-'; }
    memmove(digits, digits + pos, capacity - pos);
    bigIntDestroy(&tmp);
    *text = digits;
    return(0);
}

int printBigInt(const BigInt *a) {
    char *text = NULL;
    int rc = bigIntToString(a, &text);

    if(!rc && fputs(text, stdout) == EOF) { rc = -3; }
    free(text);

    return(rc);
}

int bigFloatToString(const BigFloat *x, size_t places, char **text) {
    if(!text) { return(-2); }
    if(places > SIZE_MAX - 4 || places > (uint64_t)INT64_MAX / 4) { return(INT_MAX); }

    BigInt scaled = BIGINT_INIT;
    char *digits = NULL, *formatted = NULL;
    int rc = bigIntCopy(&scaled, &x->mantissa);

    if(rc) { return(rc); }
    /* 5^places uses less than 3*places bits. Reserve once for that phase. */

    if(scaled.size) {
        size_t growth = places / 32 * 3 + (places % 32 * 3 + 31) / 32 + 1;
        if(growth > BN_MAX_LIMBS - scaled.size) {
            rc = INT_MAX;
            goto done;
        }
        rc = bigIntReserve(&scaled, scaled.size + growth);

        if(rc) { goto done; }

        for(size_t i = 0; i < places; ++i) {
            rc = bigIntMulUInt_32(&scaled, 5);
            if(rc) { goto done; }
        }
    }

    int64_t shift = (int64_t)x->exp + (int64_t)places;

    if(shift >= 0) {
        rc = bn_left(&scaled, (uint64_t)shift);
    } 
    
    else {
        int round_up = bigIntGetBit(&scaled, (size_t)(-shift - 1));
        (void)bn_right(&scaled, (uint64_t)-shift);
        if(round_up) { rc = bigIntAddUInt_32(&scaled, 1); }
    }
    
    if(rc) { goto done; }
    
    rc = bigIntToString(&scaled, &digits);
    
    if(rc) { goto done; }
    
    size_t len = strlen(digits), count = len > places ? len : places + 1;
    size_t negative = x->sign < 0 && x->mantissa.size;
    
    if(count > SIZE_MAX - 2 - negative) {
        rc = INT_MAX;
        goto done;
    }
    
    formatted = malloc(count + 2 + negative);
    
    if(!formatted) {
        rc = -1;
        goto done;
    }
    
    if(negative) { formatted[0] = '-'; }
    
    size_t integer = count - places, padding = count - len;
    
    for(size_t i = 0; i < count; ++i) {
        formatted[negative + i + (i >= integer)] = i < padding ? '0' : digits[i - padding];
    }
    
    formatted[negative + integer] = '.';
    formatted[negative + count + 1] = '\0';
    
    *text = formatted;
    formatted = NULL;


done:
    bigIntDestroy(&scaled);
    free(digits);
    free(formatted);
    return(rc);
}

int printBigFloat(const BigFloat *x, size_t places) {
    char *text = NULL;
    int rc = bigFloatToString(x, places, &text);

    if(!rc && fputs(text, stdout) == EOF) { rc = -3; }

    free(text);

    return(rc);
}
