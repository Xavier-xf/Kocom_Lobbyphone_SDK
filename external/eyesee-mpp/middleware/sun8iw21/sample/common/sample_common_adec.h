
#ifndef _SAMPLE_COMMON_ADEC_H_
#define _SAMPLE_COMMON_ADEC_H_

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

int ParseWavHeader(FILE *pWavFile, int *pChnNum, int *pSampleRate, int *pBitsPerSample);

#ifdef __cplusplus
}
#endif

#endif  /* _SAMPLE_COMMON_ADEC_H_ */

