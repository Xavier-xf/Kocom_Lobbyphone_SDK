#ifndef _OPT_FFT_H_
#define _OPT_FFT_H_

void *opt_fft_create(int size);
void opt_fft_destroy(void *table);
void opt_fft(void *table, short *in, short *out);
void opt_ifft(void *table, short *in, short *out);
void qns_ifft(void *table, short *in, short *out, int qin);

#endif
