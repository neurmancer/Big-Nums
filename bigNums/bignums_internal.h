#ifndef BIGNUMS_INTERNAL_H
#define BIGNUMS_INTERNAL_H
#include "bignums.h"
#include <limits.h>

/* Address/index bound with headroom for signed float scaling arithmetic. */
#define BN_MAX_LIMBS ((size_t)(SIZE_MAX / 32 < INT64_MAX / 128 ? SIZE_MAX / 32 : INT64_MAX / 128))

void bn_trim(BigInt *a);
size_t bn_bits(const BigInt *a);
int bn_left(BigInt *a, uint64_t bits);
int bn_right(BigInt *a, uint64_t bits); /* Discarded-nonzero flag */
/* Magnitude kernels ignore operand signs, bn_sub requires |a| >= |b|. */
int bn_add(BigInt *a, const BigInt *b);
void bn_sub(BigInt *a, const BigInt *b);
int bn_mul_schoolbook(BigInt *out, const BigInt *a, const BigInt *b);
int bn_div(BigInt *out, const BigInt *a, const BigInt *b);
int bn_sqrt(BigInt *out, const BigInt *a);
int bn_precision(size_t limbs);
int bn_float_pack(BigFloat *out, BigInt *m, int64_t exp, int sign, size_t limbs);
#endif
