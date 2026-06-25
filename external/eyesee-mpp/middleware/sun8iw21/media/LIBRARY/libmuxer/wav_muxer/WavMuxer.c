// #include <CDX_LogNDebug.h>
#define LOG_TAG "WavMuxer"
#include <utils/plat_log.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <encoder_type.h>
#include <record_writer.h>

#include "WavMuxer.h"
#include <FsWriter.h>

#include <aencoder.h>

/**
  Wave File Format
Wave files have a master RIFF chunk which includes a WAVE identifier followed by sub-chunks. The data is stored in little-endian byte order.

| Field       | Length | Contents                                                   |
| ----------- | ------ | ---------------------------------------------------------- |
| ckID        | 4      | Chunk ID: RIFF                                             |
| cksize      | 4      | Chunk size: 4+n                                            |
| WAVEID      | 4      | WAVE ID: WAVE                                              |
| WAVE chunks | n      | Wave chunks containing format information and sampled data |

fmt Chunk
The fmt specifies the format of the data. There are 3 variants of the Format chunk for sampled data. These differ in the extensions to the basic fmt chunk.

| Field               | Length | Contents                             |
| ------------------- | ------ | ------------------------------------ |
| ckID                | 4      | Chunk ID: fmt                        |
| cksize              | 4      | Chunk size: 16, 18 or 40             |
| wFormatTag          | 2      | Format code                          |
| nChannels           | 2      | Number of interleaved channels       |
| nSamplesPerSec      | 4      | Sampling rate (blocks per second)    |
| nAvgBytesPerSec     | 4      | Data rate                            |
| nBlockAlign         | 2      | Data block size (bytes)              |
| wBitsPerSample      | 2      | Bits per sample                      |
| cbSize              | 2      | Size of the extension (0 or 22)      |
| SubFormat           | 16     | GUID, including the data format code |

The standard format codes for waveform data are given below. The references above give more format codes for compressed data, a good fraction of which are now obsolete.

| Format Code | PreProcessor Symbol    | Data                    |
| ----------- | ---------------------- | ----------------------- |
| 0x0001      | WAVE_FORMAT_PCM        | PCM                     |
| 0x0003      | WAVE_FORMAT_IEEE_FLOAT | IEEE float              |
| 0x0006      | WAVE_FORMAT_ALAW       | 8-bit ITU-T G.711 A-law |
| 0x0007      | WAVE_FORMAT_MULAW      | 8-bit ITU-T G.711 µ-law |
| 0xFFFE      | WAVE_FORMAT_EXTENSIBLE | Determined by SubFormat |

PCM Format
The first part of the Format chunk is used to describe PCM data.

* For PCM data, the Format chunk in the header declares the number of bits/sample in each sample (wBitsPerSample). 
  The original documentation (Revision 1) specified that the number of bits per sample is to be rounded up to the next multiple of 8 bits. 
  This rounded-up value is the container size. This information is redundant in that the container size (in bytes) for each sample
  can also be determined from the block size divided by the number of channels (nBlockAlign / nChannels).
    * This redundancy has been appropriated to define new formats. For instance, Cool Edit uses a format 
      which declares a sample size of 24 bits together with a container size of 4 bytes (32 bits) determined from the block size and number of channels.
      With this combination, the data is actually stored as 32-bit IEEE floats. 
      The normalization (full scale 223) is however different from the standard float format.
* PCM data is two's-complement except for resolutions of 1-8 bits, which are represented as offset binary.

Non-PCM Formats
An extended Format chunk is used for non-PCM data. The cbSize field gives the size of the extension.

    * For all formats other than PCM, the Format chunk must have an extended portion.
      The extension can be of zero length, but the size field (with value 0) must be present.
    * For float data, full scale is 1. The bits/sample would normally be 32 or 64.
    * For the log-PCM formats (µ-law and A-law), the Rev. 3 documentation indicates that the bits/sample field (wBitsPerSample) should be set to 8 bits.
    * The non-PCM formats must have a fact chunk.

fact Chunk
All (compressed) non-PCM formats must have a fact chunk (Rev. 3 documentation). The chunk contains at least one value, the number of samples in the file.

    | Field          | Length | Contents                        |
    | -------------- | ------ | ------------------------------- |
    | ckID           | 4      | Chunk ID: fact                  |
    | cksize         | 4      | Chunk size: minimum 4           |
    | dwSampleLength | 4      | Number of samples (per channel) |

    * The Rev. 3 documentation states that the Fact chunk is required for all new new WAVE formats, but is not required for the standard WAVE_FORMAT_PCM file. 
      One presumes that files with IEEE float data (introduced after the Rev. 3 documention) need a fact chunk.
    * The number of samples field is redundant for sampled data, since the Data chunk indicates the length of the data.
      The number of samples can be determined from the length of the data and the container size as determined from the Format chunk.
    * There is an ambiguity as to the meaning of number of samples for multichannel data. The implication in the Rev. 3 documentation is that
      it should be interpreted to be number of samples per channel. The statement in the Rev. 3 documentation is:
        The nSamplesPerSec field from the wave format header is used in conjunction with the dwSampleLength field to determine the length of the data in seconds.
        With no mention of the number of channels in this computation, this implies that dwSampleLength is the number of samples per channel.
    * There is a question as to whether the fact chunk should be used for (including those with PCM) WAVE_FORMAT_EXTENSIBLE files.
      One example of a WAVE_FORMAT_EXTENSIBLE with PCM data from Microsoft, does not have a fact chunk.
*/
enum WavFormatCode
{
    WAVE_FORMAT_PCM = 0x1,
    WAVE_FORMAT_ALAW = 0x6,
    WAVE_FORMAT_MULAW = 0x7,
    WAVE_FORMAT_EXTENSIBLE = 0xFFFE,
};

typedef struct WavContext {
    int         sample_rate;
    int         channels;
    int mBitWidth;
    AUDIO_ENCODER_TYPE mEncodeType; //AUDIO_ENCODER_PCM_TYPE is PCM Format, AUDIO_ENCODER_G711A_TYPE/AUDIO_ENCODER_G711U_TYPE is compressed formats(None-PCM Formats)
    void        *priv_data;
    struct cdx_stream_info  *pb;
    unsigned int     mFallocateLen;
    FsWriter*   mpFsWriter;
    FsCacheMemInfo mCacheMemInfo;
    FSWRITEMODE mFsWriteMode;
    int     mFsSimpleCacheSize;
    unsigned int     mbSdcardDisappear;  //1:sdcard disappear, 0:sdcard is normal.

    WavRiffExtHeader mWavExtHeader;
    int mHeaderSize;
    int mPcmSize; //bytes
} WavContext;

static int GenerateWavRIFFHeader(WavContext *s, int nSampleRate, int nChannelNum, int nBitWidth,
                                AUDIO_ENCODER_TYPE eEncodeType, int nPcmBytes)
{
    int nHeaderSize = 0;
    if(AUDIO_ENCODER_PCM_TYPE == eEncodeType)
    {
        nHeaderSize = sizeof(WavRiffHeader);
    }
    else if(AUDIO_ENCODER_G711A_TYPE == eEncodeType|| AUDIO_ENCODER_G711U_TYPE == eEncodeType)
    {
        nHeaderSize = sizeof(WavRiffExtHeader);
    }
    else
    {
        aloge("fatal error! unsupport audio encode type:%d", eEncodeType);
        nHeaderSize = sizeof(WavRiffHeader);
    }
    s->mHeaderSize = nHeaderSize;
    WavRiffExtHeader *pExtHeader = &s->mWavExtHeader;
    memcpy(&pExtHeader->mBasicHeader.nRiffId, "RIFF", 4);
    pExtHeader->mBasicHeader.nRiffSz = (nPcmBytes + 8) + (nHeaderSize - 8);
    memcpy(&pExtHeader->mBasicHeader.nWaveId, "WAVE", 4);
    memcpy(&pExtHeader->mBasicHeader.nFmtId, "fmt ", 4);
    if(AUDIO_ENCODER_PCM_TYPE == eEncodeType)
    {
        pExtHeader->mBasicHeader.nFmtSz = 16;
    }
    else if(AUDIO_ENCODER_G711A_TYPE == eEncodeType || AUDIO_ENCODER_G711U_TYPE == eEncodeType)
    {
        pExtHeader->mBasicHeader.nFmtSz = 18;
    }
    else
    {
        aloge("fatal error! unsupport encode type for wav:%d", eEncodeType);
        pExtHeader->mBasicHeader.nFmtSz = 16;
    }
    if(AUDIO_ENCODER_PCM_TYPE == eEncodeType)
    {
        pExtHeader->mBasicHeader.nFormatCode = WAVE_FORMAT_PCM; // s16le
    }
    else if(AUDIO_ENCODER_G711A_TYPE == eEncodeType)
    {
        pExtHeader->mBasicHeader.nFormatCode = WAVE_FORMAT_ALAW;
    }
    else if(AUDIO_ENCODER_G711U_TYPE == eEncodeType)
    {
        pExtHeader->mBasicHeader.nFormatCode = WAVE_FORMAT_MULAW;
    }
    else
    {
        aloge("fatal error! unsupport encode type for wav:%d", eEncodeType);
        pExtHeader->mBasicHeader.nFormatCode = WAVE_FORMAT_PCM;
    }
    pExtHeader->mBasicHeader.nChnNum = nChannelNum;
    pExtHeader->mBasicHeader.nSampleRate = nSampleRate;

    if(nBitWidth == 24) // the data captured from driver is 32bits when set 24 bitdepth
    {
        pExtHeader->mBasicHeader.nByteRate = nSampleRate * nChannelNum * 32/8;
        pExtHeader->mBasicHeader.nBlockAlign = nChannelNum * 32/8;
        pExtHeader->mBasicHeader.nBitsPerSample = 32;
    }
    else
    {
        pExtHeader->mBasicHeader.nByteRate = nSampleRate * nChannelNum * nBitWidth/8;
        pExtHeader->mBasicHeader.nBlockAlign = nChannelNum * nBitWidth/8;
        pExtHeader->mBasicHeader.nBitsPerSample = nBitWidth;
    }
    if(AUDIO_ENCODER_G711A_TYPE == eEncodeType || AUDIO_ENCODER_G711U_TYPE == eEncodeType)
    {
        pExtHeader->mBasicHeader.nByteRate/=2;
        pExtHeader->mBasicHeader.nBlockAlign/=2;
        pExtHeader->mBasicHeader.nBitsPerSample/=2;
    }

    if(AUDIO_ENCODER_G711A_TYPE == eEncodeType || AUDIO_ENCODER_G711U_TYPE == eEncodeType) //need extend header
    {
        pExtHeader->nExtensionSize = 0;
        memcpy(&pExtHeader->nFactId, "fact", 4);
        pExtHeader->nFactSz = 4;
        pExtHeader->nSampleNum = nPcmBytes/(pExtHeader->mBasicHeader.nBitsPerSample*pExtHeader->mBasicHeader.nChnNum/8) ;
    }
    //memcpy(&header.data_id, "data", 4);
    //header.data_sz = ctx->mPcmSize;
    return 0;
}

static int WavWriteExtraData(void *handle, unsigned char *vosData, unsigned int vosLen, unsigned int idx)
{
    return 0;
}

static int WavMuxerWriteHeader(void *handle)
{
    WavContext *s = (WavContext *)handle;
    char *pCache = NULL;
    unsigned int nCacheSize = 0;
    if(s->pb)
    {
        FSWRITEMODE mode = s->mFsWriteMode;
        if(FSWRITEMODE_CACHETHREAD == mode)
        {
            if (s->mCacheMemInfo.mCacheSize > 0 && s->mCacheMemInfo.mpCache != NULL)
            {
                mode = FSWRITEMODE_CACHETHREAD;
                pCache = s->mCacheMemInfo.mpCache;
                nCacheSize = s->mCacheMemInfo.mCacheSize;
            }
            else
            {
                aloge("fatal error! not set cacheMemory but set mode FSWRITEMODE_CACHETHREAD! use FSWRITEMODE_DIRECT.");
                mode = FSWRITEMODE_DIRECT;
            }
        }
        else if(FSWRITEMODE_SIMPLECACHE == mode)
        {
            pCache = NULL;
            nCacheSize = s->mFsSimpleCacheSize;
        }
        s->mpFsWriter = createFsWriter(mode, s->pb, pCache, nCacheSize, 0);
        if(NULL == s->mpFsWriter)
        {
            aloge("fatal error! create FsWriter() fail!");
            return -1;
        }
    }

    int nHeaderSize = 0;
    if(AUDIO_ENCODER_PCM_TYPE == s->mEncodeType)
    {
        nHeaderSize = sizeof(WavRiffHeader);
    }
    else if(AUDIO_ENCODER_G711A_TYPE == s->mEncodeType || AUDIO_ENCODER_G711U_TYPE == s->mEncodeType)
    {
        nHeaderSize = sizeof(WavRiffExtHeader);
    }
    else
    {
        aloge("fatal error! unsupport audio encode type:%d", s->mEncodeType);
        nHeaderSize = sizeof(WavRiffHeader);
    }
    s->mHeaderSize = nHeaderSize;
    s->mpFsWriter->fsSeek(s->mpFsWriter, nHeaderSize+8, SEEK_SET); //skip "data_id", "data_sz"
    return 0;
}

static int WavMuxerWriteTrailer(void *handle)
{
    WavContext *s = (WavContext *)handle;
    GenerateWavRIFFHeader(s, s->sample_rate, s->channels, s->mBitWidth, s->mEncodeType, s->mPcmSize);
    if (s->mpFsWriter && !s->mbSdcardDisappear)
    {
        s->mpFsWriter->fsSeek(s->mpFsWriter, 0, SEEK_SET);
        s->mpFsWriter->fsWrite(s->mpFsWriter, (char*)&s->mWavExtHeader, s->mHeaderSize);
        s->mpFsWriter->fsWrite(s->mpFsWriter, "data", 4);
        s->mpFsWriter->fsWrite(s->mpFsWriter, (char*)&s->mPcmSize, 4);
    }
    return 0;
}

static int WavMuxerWritePacket(void *handle, void *pkt)
{
    WavContext *s = (WavContext *)handle;
    CdxAVPacket *avpkt = (CdxAVPacket *)pkt;
    if (s->mpFsWriter && !s->mbSdcardDisappear)
    {
        s->mpFsWriter->fsWrite(s->mpFsWriter, avpkt->data0, avpkt->size0);
        s->mPcmSize += avpkt->size0;
        return 0;
    }
    return -1;
}

static int WavMuxerIoctrl(void *handle, unsigned int uCmd, unsigned int uParam, void *pParam2)
{
    WavContext *s = (WavContext *)handle;
    _media_file_inf_t *pMediaInf = NULL;

    switch (uCmd)
    {
    case SETTOTALTIME:
        break;
    case SETFALLOCATELEN:
        s->mFallocateLen = uParam;
        break;
    case SETCACHEFD:
    {
        //s->pb = (FILE *)uParam;
        CedarXDataSourceDesc datasourceDesc;
        memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
        datasourceDesc.source_url = (char*)pParam2;
        datasourceDesc.source_type = CEDARX_SOURCE_FILEPATH;
        datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
        s->pb = create_outstream_handle(&datasourceDesc);
        if(NULL == s->pb)
        {
            aloge("fatal error! create aac outstream fail.");
            return -1;
        }
        break;
    }
    case SETCACHEFD2:
    {
        CedarXDataSourceDesc datasourceDesc;
        memset(&datasourceDesc, 0, sizeof(CedarXDataSourceDesc));
        datasourceDesc.ext_fd_desc.fd = (int)uParam;
        datasourceDesc.source_type = CEDARX_SOURCE_FD;
        datasourceDesc.stream_type = CEDARX_STREAM_LOCALFILE;
        s->pb = create_outstream_handle(&datasourceDesc);
        if(NULL == s->pb)
        {
            aloge("fatal error! create aac outstream fail.");
            return -1;
        }
        if(s->mFallocateLen > 0)
        {
            if(s->pb->fallocate(s->pb, 0x01, 0, s->mFallocateLen) < 0)
            {
                aloge("fatal error! Failed to fallocate size %d, fd[%d](%s)", s->mFallocateLen, s->pb->fd_desc.fd, strerror(errno));
            }
        }
        break;
    }
    case SETOUTURL:
        aloge("DO not support set URL");
        break;
    case SETAVPARA:
        pMediaInf = (_media_file_inf_t *)pParam2;
        s->channels = pMediaInf->channels;
        s->sample_rate = pMediaInf->sample_rate;
        s->mBitWidth = pMediaInf->bits_per_sample;
        s->mEncodeType = (AUDIO_ENCODER_TYPE)pMediaInf->audio_encode_type;
        alogd("SETAVPARA: pMediaInf->sample_rate(%d), pMediaInf->channels(%d), aEncType:%d",
            pMediaInf->sample_rate, pMediaInf->channels, pMediaInf->audio_encode_type);
        break;
    case SETSDCARDSTATE:
        s->mbSdcardDisappear = !uParam;
        alogd("SETSDCARDSTATE, mbSdcardDisappear[%d]", s->mbSdcardDisappear);
        break;
    case SETCACHEMEM:
        s->mCacheMemInfo = *(FsCacheMemInfo*)pParam2;
        break;
    case SET_FS_WRITE_MODE:
        s->mFsWriteMode = (FSWRITEMODE)uParam;
        break;
    case SET_FS_SIMPLE_CACHE_SIZE:
        s->mFsSimpleCacheSize = (int)uParam;
        break;
    default:
        break;
    }

    return 0;
}

static void *WavMuxerOpen(int *ret)
{
    WavContext *s;
    alogd("WavMuxerOpen");
    *ret = 0;
    s = (WavContext *)malloc(sizeof(WavContext));
    if(!s)
    {
        *ret = -1;
        return NULL;
    }
    memset(s,0,sizeof(WavContext));
    return (void*)s;
}

static int WavMuxerClose(void *handle)
{
    WavContext *s = (WavContext *)handle;

    if(s->mpFsWriter)
    {
        destroyFsWriter(s->mpFsWriter);
        s->mpFsWriter = NULL;
    }
    if(s->pb)
    {
        destroy_outstream_handle(s->pb);
        s->pb = NULL;
    }
    if(s->priv_data)
    {
        free(s->priv_data);
        s->priv_data = NULL;
    }
    if (s)
    {
        free(s);
        s = NULL;
    }
    return 0;
}

CDX_RecordWriter record_writer_wav = {
    .info                = "recode write wav"   ,
    .MuxerOpen           = WavMuxerOpen         ,
    .MuxerClose          = WavMuxerClose        ,
    .MuxerWriteExtraData = WavWriteExtraData    ,
    .MuxerWriteHeader    = WavMuxerWriteHeader  ,
    .MuxerWriteTrailer   = WavMuxerWriteTrailer ,
    .MuxerWritePacket    = WavMuxerWritePacket  ,
    .MuxerIoctrl         = WavMuxerIoctrl       ,
};

