#include <stdio.h>
#include "bignums.h"

/*
        Sup? This is a demo for the each function in my new Shiny API 
        and nope I won't call this an arbitarey preciousn library 'cuz I can't spell it without double checking
        it is big nums anyways rest of the demo will be 'Corpo-like'.  
        
        Have fun!

*/


/* ---------- print helpers (just for demo, not part of the API) ---------- */

/* Print a BigInt as a hex string, most-significant limb first */
void bigIntPrintHex(const BigInt *a) {
    printf("0x");
    for (int i = a->size - 1; i >= 0; i--) {
        printf("%08x", a->limbs[i]);
    }
    printf("  (size=%d)\n", a->size);
}

/* Print a BigFloat as sign + mantissa hex + exponent */
void bigFloatPrint(const BigFloat *x) {
    if (x->sign < 0) printf("-");
    else             printf("+");
    printf("0x");
    for (int i = x->mantissa.size - 1; i >= 0; i--)
        printf("%08x", x->mantissa.limbs[i]);
    printf(" * 2^(%d)  (size=%d)\n", x->exp, x->mantissa.size);
}


/* ==================================================================
   MAIN — one demo block per function group
   ================================================================== */

int main(void) {
    BigInt a, b, res, copy;
    BigFloat fa, fb, fres;
    uint32_t rem;
    int rc;

    printf("============================================\n");
    printf("  BigNums Library — Teaching Test Suite\n");
    printf("============================================\n\n");

    /* ---------------------------------------------------------------
       1. BigInt — creating numbers
       --------------------------------------------------------------- */
    printf("--- BigInt: bigIntZero, int_32ToBigInt, bigIntFromString ---\n\n");

    // bigIntZero — start with a clean zero
    bigIntZero(&a);
    printf("bigIntZero(&a)        -> "); bigIntPrintHex(&a);

    // int_32ToBigInt — wrap a plain 32-bit integer
    int_32ToBigInt(&a, 42);
    printf("int_32ToBigInt(&a,42) -> "); bigIntPrintHex(&a);

    // bigIntFromString — parse decimal text
    bigIntFromString(&a, "12345678901234567890");
    printf("fromString(\"1234...\") -> "); bigIntPrintHex(&a);

    // bad string returns -1
    rc = bigIntFromString(&a, "12x34");
    printf("fromString(\"12x34\") returned %d (expected -1)\n\n", rc);


    /* ---------------------------------------------------------------
       2. BigInt — add / mul / div / mod by a 32-bit scalar
       --------------------------------------------------------------- */
    printf("--- BigInt: scalar add, mul, div, mod ---\n\n");

    bigIntFromString(&a, "1000000000000");
    printf("a = "); bigIntPrintHex(&a);

    // bigIntAddUInt_32 — add a small integer in-place
    bigIntAddUInt_32(&a, 1);
    printf("a + 1 = "); bigIntPrintHex(&a);

    // bigIntMulUInt_32 — multiply in-place by a small integer
    bigIntMulUInt_32(&a, 12345);
    printf("a * 12345 = "); bigIntPrintHex(&a);

    // bigIntDivUInt32 — divide in-place, get remainder as return value
    copy = a;
    rem = bigIntDivUInt32(&a, 1000);
    printf("a / 1000 = "); bigIntPrintHex(&a);
    printf("remainder = %u\n", rem);

    // bigIntModUInt32 — mod without destroying the original
    rem = bigIntModUInt32(&copy, 1000);
    printf("original mod 1000 = %u\n\n", rem);


    /* ---------------------------------------------------------------
       3. BigInt — subtraction
       --------------------------------------------------------------- */
    printf("--- BigInt: bigIntSub ---\n\n");

    bigIntFromString(&a, "1000");
    bigIntFromString(&b, "999");
    printf("a = "); bigIntPrintHex(&a);
    printf("b = "); bigIntPrintHex(&b);

    // bigIntSub — result = a - b  (requires a >= b)
    rc = bigIntSub(&res, &a, &b);
    printf("a - b = "); bigIntPrintHex(&res);
    printf("returned %d (0 = success)\n", rc);

    // in-place subtraction: res can be the same pointer as a
    bigIntSub(&a, &a, &b);   // a = a - b  (safe!)
    printf("after in-place a -= b: "); bigIntPrintHex(&a);

    // subtracting larger from smaller returns -1
    rc = bigIntSub(&res, &b, &a);
    printf("b - a returned %d (expected -1)\n\n", rc);


    /* ---------------------------------------------------------------
       4. BigInt — comparison and bit access
       --------------------------------------------------------------- */
    printf("--- BigInt: bigIntCmp, bigIntGetBit ---\n\n");

    bigIntFromString(&a, "999");
    bigIntFromString(&b, "1000");
    printf("cmp(999, 1000) = %d\n", bigIntCmp(&a, &b));
    printf("cmp(1000, 999) = %d\n", bigIntCmp(&b, &a));
    printf("cmp(1000, 1000) = %d\n", bigIntCmp(&b, &b));

    // bigIntGetBit — peek at individual bits (LSB is bit 0)
    bigIntFromString(&a, "128");   // binary 10000000
    printf("\na = 128\n");
    printf("bit 0 = %d (expected 0)\n", bigIntGetBit(&a, 0));
    printf("bit 7 = %d (expected 1)\n", bigIntGetBit(&a, 7));
    printf("bit 8 = %d (expected 0, out of range)\n\n", bigIntGetBit(&a, 8));


    /* ---------------------------------------------------------------
       5. BigInt — bit shifts
       --------------------------------------------------------------- */
    printf("--- BigInt: bigIntShiftLeft, bigIntShiftRight ---\n\n");

    bigIntFromString(&a, "1");
    printf("a = 1\n");

    bigIntShiftLeft(&a, 128);
    printf("a <<= 128 -> "); bigIntPrintHex(&a);

    bigIntShiftRight(&a, 128);
    printf("a >>= 128 -> "); bigIntPrintHex(&a);

    // negative shift delegates to the opposite direction
    bigIntShiftLeft(&a, -3);   // same as right-shift 3
    printf("a <<= -3 (same as >> 3) -> "); bigIntPrintHex(&a);
    printf("\n");


    /* ---------------------------------------------------------------
       6. BigInt — FFT multiplication (the big gun)
       --------------------------------------------------------------- */
    printf("--- BigInt: bigIntMulFFT ---\n\n");

    // two decent-sized numbers
    bigIntFromString(&a, "12345678901234567890");
    bigIntFromString(&b, "98765432109876543210");
    
    printf("a = "); bigIntPrintHex(&a);
    printf("b = "); bigIntPrintHex(&b);

    rc = bigIntMulFFT(&res, &a, &b);
    printf("a * b = "); bigIntPrintHex(&res);
    printf("returned %d\n\n", rc);

    // edge case: multiply by zero
    bigIntFromString(&a, "0");
    bigIntFromString(&b, "99999999999999999999");
    bigIntMulFFT(&res, &a, &b);
    
    printf("0 * big = "); bigIntPrintHex(&res);

    // 2^16 * 2^16  (tests the 16-bit digit split)
    bigIntFromString(&a, "65536");
    bigIntFromString(&b, "65536");
    bigIntMulFFT(&res, &a, &b);
    
    printf("65536 * 65536 = "); bigIntPrintHex(&res);

    // (2^32 - 1)^2  (full 32-bit limb stress)
    bigIntFromString(&a, "4294967295");
    bigIntFromString(&b, "4294967295");
    bigIntMulFFT(&res, &a, &b);
    
    printf("(2^32-1)^2 = "); bigIntPrintHex(&res);
    printf("\n");


    /* ================================================================
       BIGFLOAT — same vibe, now with fractions and signs
       ================================================================ */

    printf("========== BigFloat Zone ==========\n\n");


    /* ---------------------------------------------------------------
       7. BigFloat — constructors and copy
       --------------------------------------------------------------- */
    printf("--- BigFloat: bigFloatZero, bigFloatFromUint32, bigFloatCopy ---\n\n");

    bigFloatZero(&fa);
    printf("bigFloatZero -> "); bigFloatPrint(&fa);

    bigFloatFromUint32(&fa, 123456789);
    printf("fromUint32(123456789) -> "); bigFloatPrint(&fa);

    bigFloatCopy(&fb, &fa);
    printf("copy -> "); bigFloatPrint(&fb);
    printf("\n");


    /* ---------------------------------------------------------------
       8. BigFloat — shifts
       --------------------------------------------------------------- */
    printf("--- BigFloat: bigFloatShiftLeft, bigFloatShiftRight ---\n\n");

    bigFloatFromUint32(&fa, 1);
    printf("1 -> "); bigFloatPrint(&fa);

    bigFloatShiftLeft(&fa, 64);
    printf("<< 64 -> "); bigFloatPrint(&fa);

    bigFloatShiftRight(&fa, 64);
    printf(">> 64 -> "); bigFloatPrint(&fa);
    printf("\n");


    /* ---------------------------------------------------------------
       9. BigFloat — multiplication
       --------------------------------------------------------------- */
    printf("--- BigFloat: bigFloatMul ---\n\n");

    bigFloatFromUint32(&fa, 123456789);
    bigFloatFromUint32(&fb, 987654321);
    printf("fa = "); bigFloatPrint(&fa);
    printf("fb = "); bigFloatPrint(&fb);

    rc = bigFloatMul(&fres, &fa, &fb);
    printf("fa * fb = "); bigFloatPrint(&fres);
    printf("returned %d\n\n", rc);


    /* ---------------------------------------------------------------
       10. BigFloat — addition and subtraction
       --------------------------------------------------------------- */
    printf("--- BigFloat: bigFloatAdd, bigFloatSub ---\n\n");

    bigFloatFromUint32(&fa, 1000);
    bigFloatFromUint32(&fb, 250);

    printf("fa = 1000, fb = 250\n");

    bigFloatAdd(&fres, &fa, &fb);
    printf("fa + fb = "); bigFloatPrint(&fres);

    bigFloatSub(&fres, &fa, &fb);
    printf("fa - fb = "); bigFloatPrint(&fres);

    // very different magnitudes — alignment is handled automatically
    bigFloatFromUint32(&fa, 1);
    bigFloatShiftLeft(&fa, 128);          // fa = 2^128
    bigFloatFromUint32(&fb, 1);
    printf("\nfa = 2^128, fb = 1\n");

    bigFloatAdd(&fres, &fa, &fb);
    printf("fa + fb = "); bigFloatPrint(&fres);

    bigFloatSub(&fres, &fa, &fb);
    printf("fa - fb = "); bigFloatPrint(&fres);
    printf("\n");


    /* ---------------------------------------------------------------
       11. BigFloat — reciprocal and division
       --------------------------------------------------------------- */
    printf("--- BigFloat: bigFloatReciprocal, bigFloatDiv ---\n\n");

    // reciprocal: 1 / 7
    bigFloatFromUint32(&fa, 7);
    printf("1 / 7 (4 limbs)  = ");
    bigFloatReciprocal(&fres, &fa, 4);
    bigFloatPrint(&fres);

    // division: 22 / 7
    bigFloatFromUint32(&fa, 22);
    bigFloatFromUint32(&fb, 7);
    printf("22 / 7 (4 limbs) = ");
    bigFloatDiv(&fres, &fa, &fb, 4);
    bigFloatPrint(&fres);

    // division by zero returns INT_MAX
    bigFloatZero(&fb);
    rc = bigFloatDiv(&fres, &fa, &fb, 4);
    printf("div by zero returned %d (expected INT_MAX)\n\n", rc);


    /* ---------------------------------------------------------------
       12. BigFloat — square root
       --------------------------------------------------------------- */
    printf("--- BigFloat: bigFloatSqrt ---\n\n");

    // sqrt(2)
    bigFloatFromUint32(&fa, 2);
    printf("sqrt(2) (6 limbs) = ");
    bigFloatSqrt(&fres, &fa, 6);
    bigFloatPrint(&fres);

    // sqrt(144) = 12  (but may show tiny noise due to Newton's method)
    bigFloatFromUint32(&fa, 144);
    printf("sqrt(144) (4 limbs) = ");
    bigFloatSqrt(&fres, &fa, 4);
    bigFloatPrint(&fres);
    printf("  (^ may show tiny AM-GM noise in last bits — known quirk)\n");

    // sqrt of negative returns -1
    fa.sign = -1;
    rc = bigFloatSqrt(&fres, &fa, 4);
    printf("sqrt(negative) returned %d (expected -1)\n\n", rc);


    /* ---------------------------------------------------------------
       13. BigFloat — absolute comparison
       --------------------------------------------------------------- */
    printf("--- BigFloat: bigFloatCmpAbs ---\n\n");

    bigFloatFromUint32(&fa, 100);
    bigFloatFromUint32(&fb, 50);
    printf("cmpAbs(100, 50) = %d (expected 1)\n", bigFloatCmpAbs(&fa, &fb));

    bigFloatFromUint32(&fa, 50);
    bigFloatFromUint32(&fb, 100);
    printf("cmpAbs(50, 100) = %d (expected -1)\n", bigFloatCmpAbs(&fa, &fb));

    bigFloatFromUint32(&fa, 77);
    bigFloatFromUint32(&fb, 77);
    printf("cmpAbs(77, 77)  = %d (expected 0)\n", bigFloatCmpAbs(&fa, &fb));

    // signs don't matter for cmpAbs
    fa.sign = -1;
    fb.sign = 1;
    printf("cmpAbs(-77, +77) = %d (still 0 — ignores sign)\n\n",
           bigFloatCmpAbs(&fa, &fb));


    /* ---------------------------------------------------------------
       14. BigFloat — normalization (usually called internally)
       --------------------------------------------------------------- */
    printf("--- BigFloat: bigFloatNormalize ---\n\n");

    // create a denormalized float by manually messing with it
    bigFloatFromUint32(&fa, 1);
    bigIntShiftLeft(&fa.mantissa, 96);   // shove bits way left
    printf("before normalize: "); bigFloatPrint(&fa);

    bigFloatNormalize(&fa);
    printf("after normalize:  "); bigFloatPrint(&fa);
    printf("\n");


    printf("============================================\n");
    printf("  All teaching examples complete.\n");
    printf("  Read the source comments above each block\n");
    printf("  to understand what each function does.\n");
    printf("============================================\n");

    return(0);
}