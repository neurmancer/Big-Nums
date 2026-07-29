#include "complexFFT.h"
#include <stdlib.h>
#include <math.h>


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void bit_reverse(complexNum *x, int n) {
    int j = 0;
    for (int i = 1; i < n; i++) {
        int bit = n >> 1;
        while (j >= bit) {
            j -= bit;
            bit >>= 1;
        }
        j += bit;
        if (i < j) {
            complexNum tmp = x[i];
            x[i] = x[j];
            x[j] = tmp;
        }
    }
}

int fft(complexNum *x, int n, int inverse) {
    bit_reverse(x, n);

    for (int len = 2; len <= n; len <<= 1) {
        double angle = -2.0 * M_PI / len * (inverse ? -1.0 : 1.0);
        complexNum wlen = {cos(angle), sin(angle)};

        for (int i = 0; i < n; i += len) {
            complexNum w = {1.0, 0.0};
            for (int j = 0; j < len/2; j++) {
                complexNum u = x[i + j];
                complexNum v = {
                    x[i + j + len/2].re * w.re - x[i + j + len/2].im * w.im,
                    x[i + j + len/2].re * w.im + x[i + j + len/2].im * w.re
                };

                x[i + j].re = u.re + v.re;
                x[i + j].im = u.im + v.im;
                x[i + j + len/2].re = u.re - v.re;
                x[i + j + len/2].im = u.im - v.im;

                double t_re = w.re * wlen.re - w.im * wlen.im;
                double t_im = w.re * wlen.im + w.im * wlen.re;
                w.re = t_re;
                w.im = t_im;
            }
        }
    }

    if (inverse) {
        double inv_n = 1.0 / n;
        for (int i = 0; i < n; i++) {
            x[i].re *= inv_n;
            x[i].im *= inv_n;
        }
    }
    return(0);
}



// Next power of two >= n
static int next_pow2(int n) {
    if (n <= 1) { return(1); }
    int p = 1;
    while (p < n) p <<= 1;
    return(p);
}

// Bluestein's algorithm – works for ANY n (even prime, odd, whatever)
int fft_arbitrary(complexNum *x, int n, int inverse) {
    if (n <= 0) { return(-1); }

    // Special case: already power of 2 → just call the fast one
    if ((n & (n - 1)) == 0) {
        return(fft(x, n, inverse));
    }

    // We need a convolution of length M >= 2n-1, M power of 2
    int M = next_pow2(2 * n - 1);

    // Allocate temporary buffers
    complexNum *a = (complexNum*)calloc(M, sizeof(complexNum));
    complexNum *b = (complexNum*)calloc(M, sizeof(complexNum));
    complexNum *chirp = (complexNum*)malloc(n * sizeof(complexNum));

    if (!a || !b || !chirp) {
        // OOM? just die gracefully or whatever, your call
        free(a); free(b); free(chirp);
        return(-1);
    }

    double sign = inverse ? 1.0 : -1.0;   

    for (int k = 0; k < n; k++) {
        double angle = sign * M_PI * (double)k * (double)k / (double)n;
        chirp[k].re = cos(angle);
        chirp[k].im = sin(angle);
    }

    
    for (int k = 0; k < n; k++) {
        a[k].re = x[k].re * chirp[k].re - x[k].im * chirp[k].im;
        a[k].im = x[k].re * chirp[k].im + x[k].im * chirp[k].re;
    }
    

    for (int k = 0; k < n; k++) {
        b[k].re =  chirp[k].re;   
        b[k].im = -chirp[k].im;  
    }
    
    for (int k = 1; k < n; k++) {
        b[M - k].re =  chirp[k].re;
        b[M - k].im = -chirp[k].im;
    }

    fft(a, M, 0);
    fft(b, M, 0);

    for (int i = 0; i < M; i++) {
        double re = a[i].re * b[i].re - a[i].im * b[i].im;
        double im = a[i].re * b[i].im + a[i].im * b[i].re;
        a[i].re = re;
        a[i].im = im;
    }

    fft(a, M, 1);

    for (int k = 0; k < n; k++) {
        double re = a[k].re * chirp[k].re - a[k].im * chirp[k].im;
        double im = a[k].re * chirp[k].im + a[k].im * chirp[k].re;
        x[k].re = re;
        x[k].im = im;
    }

    if (inverse) {
        double inv_n = 1.0 / n;
        for (int i = 0; i < n; i++) {
            x[i].re *= inv_n;
            x[i].im *= inv_n;
        }
    }

    free(a);
    free(b);
    free(chirp);

    return(0);
}