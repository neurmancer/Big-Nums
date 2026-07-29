#ifndef BIGNUM_H          //They say this indef thingy is a 'guard'
#define BIGNUM_H

#include <stdint.h>       // for uint32_t, uint64_t

#define MAX_LIMBS 64

typedef struct {
    uint32_t limbs[MAX_LIMBS];
    int size;
} BigInt;

typedef struct {
    BigInt mantissa;
    int32_t exp;      // exponent in BITS (not words)
    int sign;
} BigFloat;

void bigIntZero(BigInt *a);

void printBigInt(const BigInt *a);

void int_32ToBigInt(BigInt *a, uint32_t val);

uint32_t bigIntModUInt32(const BigInt *a, uint32_t divisor);

uint32_t bigIntDivUInt32(BigInt *a, uint32_t divisor);

int bigIntAddUInt_32(BigInt *a, uint32_t b);

int bigIntMulUInt_32(BigInt *a, uint32_t b);

int bigIntFromString(BigInt *a, const char *dec_str);

int bigIntGetBit(const BigInt *a, int bit_index);

int bigIntMulFFT(BigInt *result, const BigInt *a, const BigInt *b);

int bigIntCmp(const BigInt *a, const BigInt *b);

int bigIntSub(BigInt *result, const BigInt *a, const BigInt *b);

int bigIntShiftLeft(BigInt *a, int bits);

int bigIntShiftRight(BigInt *a, int bits);

//Float operations

void bigFloatZero(BigFloat *x);

void printBigFloat(const BigFloat *x, int decimal_places);

void bigFloatFromUint32(BigFloat *x, uint32_t v);

void bigFloatCopy(BigFloat *dst, const BigFloat *src);

void bigFloatTruncate(BigFloat *x, int target_limbs);

int bigFloatNormalize(BigFloat *x);

int bigFloatShiftLeft(BigFloat *x, int bits);

int bigFloatShiftRight(BigFloat *x, int bits);

int bigFloatCmpAbs(const BigFloat *a, const BigFloat *b);

int bigFloatMul(BigFloat *result, const BigFloat *a, const BigFloat *b);

int bigFloatAdd(BigFloat *result, const BigFloat *a, const BigFloat *b);

int bigFloatSub(BigFloat *result, const BigFloat *a, const BigFloat *b);

int bigFloatReciprocal(BigFloat *result, const BigFloat *x, int target_limbs);

int bigFloatSqrt(BigFloat *result, const BigFloat *x, int target_limbs);

int bigFloatDiv(BigFloat *result, const BigFloat *a, const BigFloat *b, int target_limbs);



static int clz32(uint32_t x);



#endif