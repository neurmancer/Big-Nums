#ifndef COMPLEX_FFT_H
#define COMPLEX_FFT_H

typedef struct {
    double re;
    double im;
} complexNum;

int fft(complexNum *x, int n, int inverse);
int fft_arbitrary(complexNum *x, int n, int inverse);

#endif

/* Double-precision transforms, also used by bigIntMulFFT with base-256 digits
 * and exact modular coefficient verification. Standalone transforms remain
 * approximate. See DOCUMENTATION.md for conventions and limits. */
