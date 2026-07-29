#ifndef COMPLEX_FFT_H
#define COMPLEX_FFT_H

typedef struct {
    double re;
    double im;
} complexNum;

int fft(complexNum *x, int n, int inverse);
int fft_arbitrary(complexNum *x, int n, int inverse);
static int next_pow2(int n);

#endif

/*
    Here is the tea...
    
    To multiply two big integers using FFT:

    Represent each integer as a sequence of “digits” (your 32‑bit limbs).

    If number A has la limbs and B has lb limbs, the convolution length is la + lb - 1.

    Find the smallest power of two N >= la + lb - 1.

    Allocate two arrays of N complex numbers, copy the limbs as real parts (imaginary = 0), zero the rest.

    Run fft on both.

    Pointwise multiply the two transforms (complex multiplication).

    Inverse FFT the product.

    Round the real parts to the nearest integer (they should be nearly integers).

    Propagate carries (each limb is now much larger than 32 bits) to produce the final big‑int result.

*/