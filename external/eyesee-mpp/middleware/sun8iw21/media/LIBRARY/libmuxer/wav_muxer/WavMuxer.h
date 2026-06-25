#ifndef _WAVMUXER_H_
#define _WAVMUXER_H_

/**
  This structure is designed only for:
    0x0001	WAVE_FORMAT_PCM	    PCM
    0x0006	WAVE_FORMAT_ALAW	8-bit ITU-T G.711 A-law
    0x0007	WAVE_FORMAT_MULAW	8-bit ITU-T G.711 µ-law

  don't consider:
    0x0003	WAVE_FORMAT_IEEE_FLOAT	IEEE float
    0xFFFE	WAVE_FORMAT_EXTENSIBLE	Determined by SubFormat
*/
typedef struct __attribute__((packed))
{
    int nRiffId;
    int nRiffSz;
    int nWaveId;
    int nFmtId;
    int nFmtSz;
    short nFormatCode;
    short nChnNum;
    int nSampleRate;
    int nByteRate;
    short nBlockAlign;
    short nBitsPerSample;
    //int data_id;
    //int data_sz;
}WavRiffHeader; //for PCM

typedef struct __attribute__((packed))
{
    WavRiffHeader mBasicHeader;
    short nExtensionSize;
    int nFactId;
    int nFactSz;
    int nSampleNum; //number of samples per channel.
}WavRiffExtHeader; //for g711a/u.

#endif  /* _WAVMUXER_H_ */

