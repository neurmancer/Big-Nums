/* Build: make MAIN=demo.c TARGET=demo
 * See DOCUMENTATION.md for current numerical and formatting limitations. */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "bignums.h"

#define REQUIRE(call) do { \
    int status = (call); \
    if (status != 0) { \
        fprintf(stderr, "%s failed: %d\n", #call, status); \
        return EXIT_FAILURE; \
    } \
} while (0)

static void show_int(const char *label, const BigInt *x) {
    printf("%s", label);
    printBigInt(x); /* The library does not append a newline. */
    putchar('\n');
}

static void show_float(const char *label, const BigFloat *x) {
    printf("%s", label);
    printBigFloat(x, 6); /* Keep decimal output modest; see printer limitations. */
    putchar('\n');
}

int main(void) {
    BigInt a, b, result;
    BigFloat x, y, z;

    puts("BigNums: fixed-capacity integers and experimental binary floats");
    printf("Capacity: %d limbs (%d bits)\n\n", MAX_LIMBS, MAX_LIMBS * 32);
    bigIntZero(&a);
    show_int("Zero: ", &a);
    int_32ToBigInt(&a, 42);
    REQUIRE(bigIntMulUInt_32(&a, 10));
    REQUIRE(bigIntAddUInt_32(&a, 69));
    show_int("42 * 10 + 69 = ", &a);
    REQUIRE(bigIntFromString(&b, "12345678901234567890"));
    REQUIRE(bigIntMulFFT(&result, &a, &b));
    show_int("489 * 12345678901234567890 = ", &result);
    printf("Product modulo 10 = %" PRIu32 "\n", bigIntModUInt32(&result, 10));
    uint32_t remainder = bigIntDivUInt32(&result, 10);
    show_int("Product divided by 10 = ", &result);
    printf("Remainder = %" PRIu32 "\n", remainder);
    REQUIRE(bigIntSub(&result, &b, &a)); /* Use a separate output or alias a, never b. */
    show_int("12345678901234567890 - 489 = ", &result);
    printf("Compare large integer with 489: %d\n", bigIntCmp(&b, &a));
    int_32ToBigInt(&a, 1);
    REQUIRE(bigIntShiftLeft(&a, 40));
    printf("Bit 40 of 2^40 = %d\n", bigIntGetBit(&a, 40));
    REQUIRE(bigIntShiftRight(&a, 8));
    show_int("2^40 >> 8 = ", &a);
    REQUIRE(bigIntFactorial(&result, 50));
    show_int("50! = ", &result);

    puts("\nBinary floats (value = sign * mantissa * 2^exp)");
    bigFloatZero(&x);
    show_float("Zero: ", &x);
    bigFloatFromUint32(&x, 10);
    bigFloatFromUint32(&y, 2);
    REQUIRE(bigFloatAdd(&z, &x, &y));
    show_float("10 + 2 = ", &z);
    REQUIRE(bigFloatSub(&z, &y, &x));
    show_float("2 - 10 = ", &z);
    REQUIRE(bigFloatMul(&z, &x, &y));
    show_float("10 * 2 = ", &z);
    bigFloatCopy(&z, &x);
    REQUIRE(bigFloatShiftLeft(&z, 2));
    REQUIRE(bigFloatShiftRight(&z, 1));
    REQUIRE(bigFloatNormalize(&z));
    bigFloatTruncate(&z, 1);
    show_float("10 shifted left 2, then right 1 = ", &z);
    printf("Compare |10| with |2|: %d\n", bigFloatCmpAbs(&x, &y));
    REQUIRE(bigFloatReciprocal(&z, &y, 4));
    show_float("1 / 2 = ", &z);
    REQUIRE(bigFloatDiv(&z, &x, &y, 4));
    show_float("10 / 2 = ", &z);

    /* Iterative routines are experimental. Report errors without printing an
     * uninitialized result or claiming that target_limbs guarantees accuracy. */
    int status = bigFloatSqrt(&z, &y, 4);
    if (status == 0) show_float("sqrt(2), experimental approximation = ", &z);
    else printf("sqrt(2) could not be computed (status %d)\n", status);
    bigFloatZero(&y);
    printf("Reciprocal of zero returns %d\n", bigFloatReciprocal(&z, &y, 4));
    bigFloatFromUint32(&y, 1);
    y.sign = -1;
    printf("Square root of -1 returns %d\n", bigFloatSqrt(&z, &y, 4));
    return EXIT_SUCCESS;
}
