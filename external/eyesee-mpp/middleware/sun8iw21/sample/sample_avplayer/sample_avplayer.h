
#ifndef _SAMPLE_AVPLAYER_H_
#define _SAMPLE_AVPLAYER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <pthread.h>

//#include "mm_comm_sys.h"
//#include "mpi_sys.h"

//#include "DemuxCompStream.h"
//#include "mm_comm_demux.h"
//#include "mpi_demux.h"

//#include "mm_comm_vdec.h"
//#include "mpi_vdec.h"

//#include "mm_comm_vo.h"
//#include "mpi_vo.h"

#include "tsemaphore.h"

#define MAX_FILE_PATH_LEN  (128)


//typedef enum
//{
//    STATE_PREPARED = 0,
//    STATE_PAUSE,
//    STATE_PLAY,
//    STATE_STOP,
//}STATE_E;

//enum CLOCK_COMP_PORT_INDEX{
//    CLOCK_PORT_AUDIO = 0, //to audio render
//    CLOCK_PORT_VIDEO = 1, //to video render, CLOCK_PORT_INDEX_VIDEO
//    CLOCK_PORT_DEMUX = 2, //to demux
//    CLOCK_PORT_VDEC  = 3, //to vdec
//};

typedef struct SampleAVPlayerCmdLineParam
{
    char strConfigFilePath[MAX_FILE_PATH_LEN];
} SampleAVPlayerCmdLineParam;

typedef struct SampleAVPlayerConfig
{
    char strSrcFile[MAX_FILE_PATH_LEN];
    PIXEL_FORMAT_E eVdecPixelFormat;
    int nMaxVdecOutputWidth; //0:not limited
    int nMaxVdecOutputHeight;
    ROTATE_E eVdecRotation;
    int nVdecExtraFrameNum;
    VO_LAYER nLayerId;
    int nDisplayX;
    int nDisplayY;
    int nDisplayWidth;
    int nDisplayHeight;
    int nVoChnFrameRate; //unit:fps
    bool bForbidAudio;
    int nAudioVolume; //scope: [0, 100]
    int nVideoStreamIndex; //from 0.
    int nAudioStreamIndex; //from 0.
    int nSeekTime; //unit:ms
    float fVps; //scope: [0.5, 4]
    int nLoopCnt; //0:not loop, >0: loop play count, -1: loop infinitely
    int nVeFreq;  // 0:default, unit: MHz
    int nTestDuration; //unit:s
} SampleAVPlayerConfig;


typedef struct SampleAVPlayerContext
{
    SampleAVPlayerCmdLineParam stCmdLinePara;
    SampleAVPlayerConfig stConfigPara;

    int nSrcFd;
    MPP_SYS_CONF_S stSysConf;

    cdx_sem_t stSemExit;

    DEMUX_CHN nDmxChn;
    DEMUX_CHN_ATTR_S stDmxChnAttr;
    DEMUX_MEDIA_INFO_S stDemuxMediaInfo;

    VDEC_CHN nVdecChn;
    VDEC_CHN_ATTR_S stVdecChnAttr;
    BOOL bForceFramePackage;

    ADEC_CHN nAdecChn;
    ADEC_CHN_ATTR_S stAdecChnAttr;

    VO_DEV nVoDev;
    VO_PUB_ATTR_S stVoPubAttr;
    VO_LAYER nVoLayer;
    VO_VIDEO_LAYER_ATTR_S stVoLayerAttr;
    VO_CHN nVoChn;
    VO_LAYER nUILayer;

    AUDIO_DEV nAODev;
    AO_CHN nAOChn;
    PCM_CARD_TYPE_E ePcmCardType;
    AIO_ATTR_S stAOAttr;

    CLOCK_CHN nClockChn;
    CLOCK_CHN_ATTR_S stClockChnAttr;

    bool bVONotifyEof;
    bool bAONotifyEof;
    pthread_mutex_t EofLock;
    
    bool bVideoFlag;
    bool bAudioFlag;
    int nLoopNum;

    bool bOverFlag;
} SampleAVPlayerContext;

#endif  /* _SAMPLE_AVPLAYER_H_ */

