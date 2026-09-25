#ifndef BIGNUM_H
#define BIGNUM_H

#include <stddef.h>
#include <stdint.h>

/* ABI 3: initialize every object (including outputs) and destroy it afterwards.
 * Structure assignment is NOT a value copy. Use Copy or Swap instead.
 * Limbs store the magnitude. Zero has size == 0 and sign == +1. */
typedef struct {
    uint32_t *limbs;
    size_t size;
    size_t capacity;
    int sign; /* +1 or -1; canonical zero is positive. */
} BigInt;

#define BIGINT_INIT {NULL, 0, 0, 1}
#define BIGFLOAT_DEFAULT_PRECISION ((size_t)1024)

typedef struct {
    BigInt mantissa; /* Nonnegative magnitude: mantissa.sign == +1. */
    int32_t exp;     /* Value = sign * mantissa * 2^exp. */
    int sign;
    size_t precision; /* Significant 32-bit limbs, not capacity. */
} BigFloat;

#define BIGFLOAT_INIT {BIGINT_INIT, 0, 1, BIGFLOAT_DEFAULT_PRECISION}

/* Status-returning operations: 0 success, -1 allocation/transform failure, INT_MAX
 * unrepresentable size/exponent, -2 invalid input/domain, -3 stream failure.
 * Numeric outputs remain unchanged on error. Comparisons return -1/0/+1.
 * Scalar division/modulo return signed remainders, or INT64_MIN for divisor 0. */
void bigIntInit(BigInt *a);
void bigIntDestroy(BigInt *a);
void bigIntZero(BigInt *a); /* Retains capacity; never allocates. */
int bigIntReserve(BigInt *a, size_t limbs);
int bigIntCopy(BigInt *dst, const BigInt *src);
void bigIntSwap(BigInt *a, BigInt *b);
int int_32ToBigInt(BigInt *a, uint32_t val);
int bigIntFromInt64(BigInt *a, int64_t val);
void bigIntNegate(BigInt *a); /* In place, without allocation. */
int bigIntFromString(BigInt *a, const char *text);
int bigIntToString(const BigInt *a, char **text); /* Free successful output. */
int printBigInt(const BigInt *a);
/* Quotients truncate toward zero; remainder has the dividend's sign. */
int64_t bigIntModUInt32(const BigInt *a, uint32_t divisor);
int64_t bigIntDivUInt32(BigInt *a, uint32_t divisor);
int bigIntAddUInt_32(BigInt *a, uint32_t b);
int bigIntMulUInt_32(BigInt *a, uint32_t b);
int bigIntGetBit(const BigInt *a, size_t bit_index); /* Magnitude bit. */
int bigIntCmp(const BigInt *a, const BigInt *b);
int bigIntCmpAbs(const BigInt *a, const BigInt *b);
int bigIntAdd(BigInt *result, const BigInt *a, const BigInt *b);
int bigIntSub(BigInt *result, const BigInt *a, const BigInt *b);
/* Shifts operate on magnitude; right shifts truncate toward zero. */
int bigIntShiftLeft(BigInt *a, int64_t bits);
int bigIntShiftRight(BigInt *a, int64_t bits);
int bigIntFactorial(BigInt *result, uint32_t n);
int bigIntMul(BigInt *result, const BigInt *a, const BigInt *b);
/* Verified FFT where its bounds hold; exact schoolbook elsewhere. */
int bigIntMulFFT(BigInt *result, const BigInt *a, const BigInt *b);

void bigFloatInit(BigFloat *x);
void bigFloatDestroy(BigFloat *x);
void bigFloatZero(BigFloat *x); /* Retains capacity and precision. */
void bigFloatSwap(BigFloat *a, BigFloat *b);
int bigFloatCopy(BigFloat *dst, const BigFloat *src); /* Copies precision too. */
int bigFloatFromUint32(BigFloat *x, uint32_t v);
int bigFloatSetPrecision(BigFloat *x, size_t limbs); /* >= 1; rounds if needed. */
int bigFloatTruncate(BigFloat *x, size_t limbs);     /* Same as SetPrecision. */
int bigFloatNormalize(BigFloat *x);
int bigFloatShiftLeft(BigFloat *x, int64_t bits);
int bigFloatShiftRight(BigFloat *x, int64_t bits);
int bigFloatCmpAbs(const BigFloat *a, const BigFloat *b);
/* Add/Sub/Mul use the destination's precision, including aliased outputs. */
int bigFloatMul(BigFloat *result, const BigFloat *a, const BigFloat *b);
int bigFloatAdd(BigFloat *result, const BigFloat *a, const BigFloat *b);
int bigFloatSub(BigFloat *result, const BigFloat *a, const BigFloat *b);
/* These use the explicit precision and store it in the destination. */
int bigFloatReciprocal(BigFloat *result, const BigFloat *x, size_t limbs);
int bigFloatSqrt(BigFloat *result, const BigFloat *x, size_t limbs);
int bigFloatDiv(BigFloat *result, const BigFloat *a, const BigFloat *b, size_t limbs);
int bigFloatToString(const BigFloat *x, size_t places, char **text);
int printBigFloat(const BigFloat *x, size_t places);

#endif
