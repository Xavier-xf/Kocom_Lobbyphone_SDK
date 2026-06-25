#ifndef _FFT_H_
#define _FFT_H_

#ifdef __USE_KISS_FFT

#include "kiss_fftr.h"
#define AEC_FFT_INIT		spx_fft_init
#define AEC_FFT_DESTROY		spx_fft_destroy
#define AEC_FFT				spx_fft
#define AEC_IFFT			spx_ifft


#else

#include "opt_fft.h"
#define AEC_FFT_INIT		opt_fft_create
#define AEC_FFT_DESTROY		opt_fft_destroy
#define AEC_FFT				opt_fft
#define AEC_IFFT			opt_ifft

#endif


#endif
