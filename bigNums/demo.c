/* Build: make MAIN=demo.c TARGET=demo
 * See DOCUMENTATION.md for current numerical and formatting limitations. */

/*
    This is the demo suite for usage of the lib just look how the function you want works and check the man page for more detail... 
*/


#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "bignums.h"

#define REQUIRE(call)                                                                              \
    do {                                                                                           \
        int status = (call);                                                                       \
        if(status != 0) {                                                                          \
            fprintf(stderr, "%s failed: %d\n", #call, status);                                     \
            exit_status = EXIT_FAILURE;                                                            \
            goto done;                                                                             \
        }                                                                                          \
    } while(0)

static int show_int(const char *label, const BigInt *x) {

    printf("%s", label);
    int rc = printBigInt(x);
    if(!rc && putchar('\n') == EOF) { rc = -3; }

    return(rc);
}

static int show_float(const char *label, const BigFloat *x) {

    printf("%s", label);

    int rc = printBigFloat(x, 6);

    if(!rc && putchar('\n') == EOF) { rc = -3; }
    return(rc);
}

int main(void) {

    int exit_status = EXIT_SUCCESS;
    
    BigInt a = BIGINT_INIT, b = BIGINT_INIT, result = BIGINT_INIT;
    BigFloat x = BIGFLOAT_INIT, y = BIGFLOAT_INIT, z = BIGFLOAT_INIT;

    puts("BigNums: dynamically allocated integers and binary floats");
    printf("Default float precision: %zu limbs\n\n", BIGFLOAT_DEFAULT_PRECISION);
    
    bigIntZero(&a);
    
    REQUIRE(show_int("Zero: ", &a));
    REQUIRE(int_32ToBigInt(&a, 42));
    REQUIRE(bigIntMulUInt_32(&a, 10));
    REQUIRE(bigIntAddUInt_32(&a, 69));
    REQUIRE(show_int("42 * 10 + 69 = ", &a));
    REQUIRE(bigIntFromString(&b, "12345678901234567890"));
    REQUIRE(bigIntMulFFT(&result, &a, &b));
    REQUIRE(show_int("489 * 12345678901234567890 = ", &result));
    
    printf("Product modulo 10 = %" PRId64 "\n", bigIntModUInt32(&result, 10));
    
    int64_t remainder = bigIntDivUInt32(&result, 10);
    
    REQUIRE(show_int("Product divided by 10 = ", &result));
    printf("Remainder = %" PRId64 "\n", remainder);
    
    REQUIRE(bigIntSub(&result, &b, &a)); /* Either input may also be the output. */
    REQUIRE(show_int("12345678901234567890 - 489 = ", &result));
    
    printf("Compare large integer with 489: %d\n", bigIntCmp(&b, &a));
    
    REQUIRE(int_32ToBigInt(&a, 1));
    REQUIRE(bigIntShiftLeft(&a, 40));
    printf("Bit 40 of 2^40 = %d\n", bigIntGetBit(&a, 40));
    
    REQUIRE(bigIntShiftRight(&a, 8));
    REQUIRE(show_int("2^40 >> 8 = ", &a));
    REQUIRE(bigIntFactorial(&result, 50));
    REQUIRE(show_int("50! = ", &result));

    REQUIRE(bigIntFromInt64(&a, -7));
    REQUIRE(bigIntFromString(&b, "+3"));
    REQUIRE(bigIntAdd(&result, &a, &b));
    REQUIRE(show_int("-7 + 3 = ", &result));
    REQUIRE(bigIntSub(&result, &a, &b));
    REQUIRE(show_int("-7 - 3 = ", &result));
    
    remainder = bigIntDivUInt32(&a, 3);
    
    REQUIRE(show_int("-7 / 3 (toward zero) = ", &a));
    printf("Signed remainder = %" PRId64 "\n", remainder);

    puts("\nBinary floats (value = sign * mantissa * 2^exp)");
    bigFloatZero(&x);
    
    REQUIRE(show_float("Zero: ", &x));
    REQUIRE(bigFloatFromUint32(&x, 10));
    REQUIRE(bigFloatFromUint32(&y, 2));
    REQUIRE(bigFloatAdd(&z, &x, &y));
    REQUIRE(show_float("10 + 2 = ", &z));
    REQUIRE(bigFloatSub(&z, &y, &x));
    REQUIRE(show_float("2 - 10 = ", &z));
    REQUIRE(bigFloatMul(&z, &x, &y));
    REQUIRE(show_float("10 * 2 = ", &z));
    REQUIRE(bigFloatCopy(&z, &x));
    REQUIRE(bigFloatShiftLeft(&z, 2));
    REQUIRE(bigFloatShiftRight(&z, 1));
    REQUIRE(bigFloatNormalize(&z));
    REQUIRE(bigFloatTruncate(&z, 1));
    REQUIRE(show_float("10 shifted left 2, then right 1 = ", &z));
    
    printf("Compare |10| with |2|: %d\n", bigFloatCmpAbs(&x, &y));
    
    REQUIRE(bigFloatReciprocal(&z, &y, 4));
    REQUIRE(show_float("1 / 2 = ", &z));
    REQUIRE(bigFloatDiv(&z, &x, &y, 4));
    REQUIRE(show_float("10 / 2 = ", &z));

    /* Check the status before printing a square-root approximation. */
    int status = bigFloatSqrt(&z, &y, 4);
    
    if(status == 0) {
        REQUIRE(show_float("sqrt(2), approximation = ", &z));
    } 
    
    else {
        printf("sqrt(2) could not be computed (status %d)\n", status);
    }
    
    bigFloatZero(&y);
    
    printf("Reciprocal of zero returns %d\n", bigFloatReciprocal(&z, &y, 4));
    REQUIRE(bigFloatFromUint32(&y, 1));
    
    y.sign = -1;
    
    printf("Square root of -1 returns %d\n", bigFloatSqrt(&z, &y, 4));
    
    if(ferror(stdout)) { exit_status = EXIT_FAILURE; }
done:
    bigIntDestroy(&a);
    bigIntDestroy(&b);
    bigIntDestroy(&result);
    bigFloatDestroy(&x);
    bigFloatDestroy(&y);
    bigFloatDestroy(&z);
    return(exit_status);
}
