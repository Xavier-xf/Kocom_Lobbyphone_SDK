#ifndef _SAMPLE_VIDEORESIZER_H_
#define _SAMPLE_VIDEORESIZER_H_

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>

#include "mpi_sys.h"
#include "mpi_demux.h"
#include "mpi_vdec.h"
#include "mpi_venc.h"

#define MAX_FILE_PATH_LEN  (128)

typedef struct SampleVideoResizerCmdLineParam
{
    char strConfigFilePath[MAX_FILE_PATH_LEN];
}SampleVideoResizerCmdLineParam;

typedef struct SampleVideoResizerConfig
{
    char SrcFile[MAX_FILE_PATH_LEN];
    char DstFile[MAX_FILE_PATH_LEN];
    PAYLOAD_TYPE_E eEncodeType;
    int nDstWidth;
    int nDstHeight;
    ROTATE_E eRotation;
    float fBitrate; //unit:Mbps
    int nKeyFrameInterval;
}SampleVideoResizerConfig;

typedef struct SampleVideoResizerContext
{
    SampleVideoResizerCmdLineParam stCmdLinePara;
    SampleVideoResizerConfig stConfigPara;

    FILE *pDstFile;
    MPP_SYS_CONF_S stSysConf;

    DEMUX_CHN nDmxChn;
    DEMUX_CHN_ATTR_S stDmxChnAttr;
    DEMUX_MEDIA_INFO_S stDemuxMediaInfo;
    bool bDmxOverFlag;

    VDEC_CHN nVdecChn;
    VDEC_CHN_ATTR_S stVdecChnAttr;
    bool bVdecOverFlag;

    VENC_CHN nVencChn;
    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_RC_PARAM_S stVencRcParam;
    VencHeaderData stSpsPpsInfo;

    bool bOverFlag;
}SampleVideoResizerContext;

SampleVideoResizerContext* createSampleVideoResizerContext();
int freeSampleVideoResizerContext(SampleVideoResizerContext *pContext);

#endif  /* _SAMPLE_VIDEORESIZER_H_ */

