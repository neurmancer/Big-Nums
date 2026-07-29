#include <stdio.h>
#include "bignums.h"

/*
        Sup? this is a test clause for my fresh big num lib and it seemed like it fucking passed the TEST...
        This subfolder won't have commentary more than it needs to since...it was a hell even for the creator(me lulz)
        
        So what I've done so far:
            Implemented:
                0- bignum zero init
                1- bignum+int
                2- bignum * int
                3- int to bignum
                4- string to bignum
                5- bignum mod and division
                6- bignum get bit
                7- bignum mul with FFT 

            Then the next day I came back like a maniac and added:
                8-  BigFloat zero / from uint32 / normalize / copy
                9-  BigFloat shift left / right
                10- BigFloat abs compare
                11- BigFloat mul (FFT powered)
                12- BigFloat add / sub
                13- BigFloat reciprocal (Newton)
                14- BigFloat div
                15- BigFloat sqrt (Newton)

            Now we test the whole damn zoo.


            Known bugs:
                Fixed using a different approach:
                    bigFloatSqrt is loosing precision for some reason 
                    bigFloatRecip is loosing also precision if I am not mistaken

                Turnicated the precision loss in the said functions


            the resut of :sqrt(144) = +0xc00000003f7fffff0400000000000000 * 2^(-124)  (size=4) is 12.000000000000000000001 due to 
            AM-GM Inequality (Arithmetic Mean-Geometric Mean) due to my method (Newton's method of square root) so yeah that can be 'known weakness'
*/

/* helper to print a BigInt in hex (most-significant limb first) */
void bigIntPrintHex(const BigInt *a);

/* helper to print a BigFloat (sign + mantissa hex + exp) */
void bigFloatPrint(const BigFloat *x);

int main(void) {
    BigInt a, b, res, copy;
    BigFloat fa, fb, fres, ftmp;
    uint32_t rem;
    int rc;

    /* ---------- 1. string conversion & add ---------- */
    printf("=== Test: string conversion & addition ===\n");
    bigIntFromString(&a, "123456789");
    bigIntFromString(&b, "987654321");
    printf("a = "); bigIntPrintHex(&a);
    printf("b = "); bigIntPrintHex(&b);

    bigIntAddUInt_32(&a, 1);
    printf("a + 1 = "); bigIntPrintHex(&a);

    /* ---------- 2. mul/div/mod by uint32 ---------- */
    printf("\n=== Test: mul/div/mod by 32-bit ===\n");
    bigIntFromString(&a, "1000000000000");   // 10^12
    printf("a = "); bigIntPrintHex(&a);

    bigIntMulUInt_32(&a, 12345);
    printf("a * 12345 = "); bigIntPrintHex(&a);

    copy = a;
    rem = bigIntDivUInt32(&a, 1000);
    printf("after div by 1000: "); bigIntPrintHex(&a);
    printf("remainder = %u\n", rem);

    rem = bigIntModUInt32(&copy, 1000);
    printf("mod 1000 of original = %u\n", rem);

    /* ---------- 3. get bit ---------- */
    printf("\n=== Test: get bit ===\n");
    bigIntFromString(&a, "128");  // binary: 10000000
    printf("a = "); bigIntPrintHex(&a);
    printf("bit 0 = %d\n", bigIntGetBit(&a, 0));
    printf("bit 7 = %d\n", bigIntGetBit(&a, 7));
    printf("bit 8 = %d\n", bigIntGetBit(&a, 8));

    /* ---------- 4. FFT multiplication ---------- */
    printf("\n=== Test: FFT multiplication ===\n");

    bigIntFromString(&a, "12345678901234567890");
    bigIntFromString(&b, "98765432109876543210");
    printf("a = "); bigIntPrintHex(&a);
    printf("b = "); bigIntPrintHex(&b);

    if (bigIntMulFFT(&res, &a, &b) == 0) {
        printf("a * b = "); bigIntPrintHex(&res);
    } else {
        printf("FFT multiplication overflowed!\n");
    }

    bigIntFromString(&a, "0");
    bigIntFromString(&b, "99999999999999999999");
    if (bigIntMulFFT(&res, &a, &b) == 0) {
        printf("0 * big = "); bigIntPrintHex(&res);
    }

    bigIntFromString(&a, "65536");
    bigIntFromString(&b, "65536");
    if (bigIntMulFFT(&res, &a, &b) == 0) {
        printf("2^16 * 2^16 = "); bigIntPrintHex(&res);
    }

    bigIntFromString(&a, "4294967295");
    bigIntFromString(&b, "4294967295");
    if (bigIntMulFFT(&res, &a, &b) == 0) {
        printf("(2^32-1)^2 = "); bigIntPrintHex(&res);
    }

    /* ================================================================
       BIGFLOAT TESTS
       ================================================================ */

    printf("\n\n========== BIGFLOAT ZONE ==========\n");

    /* ---------- 5. BigFloat zero / fromUint32 / copy / normalize ---------- */
    printf("\n=== Test: BigFloat basic constructors ===\n");

    bigFloatZero(&fa);
    printf("zero = "); bigFloatPrint(&fa);

    bigFloatFromUint32(&fa, 123456789);
    printf("from 123456789 = "); bigFloatPrint(&fa);

    bigFloatCopy(&fb, &fa);
    printf("copy = "); bigFloatPrint(&fb);

    /* ---------- 6. BigFloat shifts ---------- */
    printf("\n=== Test: BigFloat shifts ===\n");

    bigFloatFromUint32(&fa, 1);
    printf("1 = "); bigFloatPrint(&fa);

    bigFloatShiftLeft(&fa, 32);
    printf("1 << 32 = "); bigFloatPrint(&fa);

    bigFloatShiftLeft(&fa, 5);
    printf("then << 5 = "); bigFloatPrint(&fa);

    bigFloatShiftRight(&fa, 37);
    printf("then >> 37 (should be back near 1) = "); bigFloatPrint(&fa);

    /* ---------- 7. BigFloat mul ---------- */
    printf("\n=== Test: BigFloat multiplication ===\n");

    bigFloatFromUint32(&fa, 123456789);
    bigFloatFromUint32(&fb, 987654321);
    printf("fa = "); bigFloatPrint(&fa);
    printf("fb = "); bigFloatPrint(&fb);

    rc = bigFloatMul(&fres, &fa, &fb);
    if (rc == 0) {
        printf("fa * fb = "); bigFloatPrint(&fres);
    } else {
        printf("bigFloatMul failed with code %d\n", rc);
    }

    /* ---------- 8. BigFloat add / sub ---------- */
    printf("\n=== Test: BigFloat add / sub ===\n");

    bigFloatFromUint32(&fa, 1000);
    bigFloatFromUint32(&fb, 250);
    printf("1000 + 250 = ");
    if (bigFloatAdd(&fres, &fa, &fb) == 0)
        bigFloatPrint(&fres);

    printf("1000 - 250 = ");
    if (bigFloatSub(&fres, &fa, &fb) == 0)
        bigFloatPrint(&fres);

    // different exponents
    bigFloatFromUint32(&fa, 1);
    bigFloatShiftLeft(&fa, 64);          // 2^64
    bigFloatFromUint32(&fb, 1);
    printf("2^64 + 1 = ");
    if (bigFloatAdd(&fres, &fa, &fb) == 0)
        bigFloatPrint(&fres);

    /* ---------- 9. BigFloat reciprocal + div ---------- */
    printf("\n=== Test: BigFloat reciprocal & division ===\n");

    bigFloatFromUint32(&fa, 7);
    printf("1 / 7 ≈ ");
    if (bigFloatReciprocal(&fres, &fa, 4) == 0)
        bigFloatPrint(&fres);

    bigFloatFromUint32(&fa, 22);
    bigFloatFromUint32(&fb, 7);
    printf("22 / 7 ≈ ");
    if (bigFloatDiv(&fres, &fa, &fb, 4) == 0)
        bigFloatPrint(&fres);

    /* ---------- 10. BigFloat sqrt ---------- */
    printf("\n=== Test: BigFloat sqrt ===\n");

    bigFloatFromUint32(&fa, 2);
    printf("sqrt(2) ≈ ");
    if (bigFloatSqrt(&fres, &fa, 6) == 0)
        bigFloatPrint(&fres);

    bigFloatFromUint32(&fa, 144);
    printf("sqrt(144) = ");
    if (bigFloatSqrt(&fres, &fa, 4) == 0)
        bigFloatPrint(&fres);

    /* ---------- 11. compare abs ---------- */
    printf("\n=== Test: bigFloatCmpAbs ===\n");

    bigFloatFromUint32(&fa, 100);
    bigFloatFromUint32(&fb, 50);
    printf("|100| vs |50| → %d\n", bigFloatCmpAbs(&fa, &fb));

    bigFloatFromUint32(&fa, 50);
    bigFloatFromUint32(&fb, 100);
    printf("|50| vs |100| → %d\n", bigFloatCmpAbs(&fa, &fb));

    bigFloatFromUint32(&fa, 77);
    bigFloatFromUint32(&fb, 77);
    printf("|77| vs |77| → %d\n", bigFloatCmpAbs(&fa, &fb));

    printf("\n========== ALL TESTS DONE ==========\n");
    return(0);
}

void bigIntPrintHex(const BigInt *a) {
    printf("0x");
    for (int i = a->size - 1; i >= 0; i--) {
        printf("%08x", a->limbs[i]);
    }
    printf("  (size=%d)\n", a->size);
}

void bigFloatPrint(const BigFloat *x) {
    if (x->sign < 0) printf("-");
    else             printf("+");

    printf("0x");
    for (int i = x->mantissa.size - 1; i >= 0; i--)
        printf("%08x", x->mantissa.limbs[i]);
    printf(" * 2^(%d)  (size=%d)\n", x->exp, x->mantissa.size);
}