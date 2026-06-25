#ifndef _SAMPLE_UVC_H_
#define _SAMPLE_UVC_H_

#include <pthread.h>
#include <tsemaphore.h>
#include <plat_type.h>
#include <mm_comm_vo.h>
#include <mm_common.h>
#include <mm_comm_video.h>
#include <mm_comm_venc.h>

#include "uac/uac.h"
#include "utils/parser_uvc_configfs.h"

#define MAX_FILE_PATH_SIZE (256)

typedef struct SampleUVCFrame {
    void *pVirAddr;
    void *pPhyAddr;
    unsigned int iBufLen;
} SampleUVCFrame;

typedef struct SampleUVCDevice {
    int uvc_dev;
    VI_DEV vipp_dev;
    VI_CHN vipp_chn;
    ISP_DEV isp_dev;
    VENC_CHN ve_chn;
    BOOL enable_bulk_mode;
    BOOL is_streaming_flag;
    struct uvc_streaming_control uvc_streaming_probe;
    struct uvc_streaming_control uvc_streaming_commit;
    int control;
    int format;
    struct uvc_frame *frame;
    SampleUVCFrame *frame_buffer;
    int frame_buffer_num;
    BOOL capture_thread_exit_flag;
    BOOL capture_thread_running;
    pthread_t capture_thread_trd;

    BOOL aiisp_switch_thread_exit_flag;
    pthread_t aiisp_switch_thread_trd;

    int g2d_dev;
    VIDEO_FRAME_INFO_S g2d_proc_frame;

    struct list_head frame_idle_list; /* SampleUVCOutBuf */
    struct list_head frame_valid_list;
    struct list_head frame_used_list;
    pthread_mutex_t frame_list_lock;

    VI_DEV dual_stream_vipp_dev;
    VI_CHN dual_stream_vipp_chn;
    ISP_DEV dual_stream_isp_dev;
    VENC_CHN dual_stream_ve_chn;
    int dual_stream_tmp_buffer_len;
    unsigned char *dual_stream_tmp_buffer;
    BOOL dual_stream_capture_thread_exit_flag;
    BOOL dual_stream_capture_thread_running;
    pthread_t dual_stream_capture_thread_trd;

    void *privite_data;
} SampleUVCDevice;

typedef struct SampleUVCCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];

} SampleUVCCmdLineParam;

typedef struct SampleUVCOutBuf {
    unsigned char *pcData;
    unsigned int iDataBufSize;
    unsigned int iDataSize0;
    unsigned int iDataSize1;
    unsigned int iDataSize2;
    struct list_head mList;
} SampleUVCOutBuf;

typedef struct SampleUVCConfig
{
    int uvc_dev;
    VI_DEV vipp_dev;
    ISP_DEV isp_dev;
    int capture_width;
    int capture_height;
    int capture_framerate;
    PIXEL_FORMAT_E capture_format; //MM_PIXEL_FORMAT_YUV_PLANAR_420
    int uvc_bulk_mode;

    PAYLOAD_TYPE_E encode_type;
    int encode_width;
    int encode_height;
    int encode_bitrate;
    int encode_framerate;
    int encode_quality;

    BOOL enable_encode_online;
    BOOL enable_encpp;

    BOOL enable_dual_stream;
    int dual_stream_vipp_dev;
    int dual_stream_isp_dev;
    PIXEL_FORMAT_E dual_stream_capture_format;
    int dual_stream_capture_framerate;
    int dual_stream_width;
    int dual_stream_height;
    PAYLOAD_TYPE_E dual_stream_type;
    int dual_stream_framerate;
    int dual_stream_bitrate;

    // aiisp
    BOOL mAiIspEnable;
    int mAiIspNpuRefBufReduceEnable;
    int mAiIspSwitchReleaseResEnable;
    char mAiIspLutNbgFilePath[MAX_FILE_PATH_SIZE];
    char mAiIspNbgFilePath[MAX_FILE_PATH_SIZE];
    int mAiIspModelVersion;
    char mAiIspCfgBinPath[MAX_FILE_PATH_SIZE];
    int mAiIspWidth;
    int mAiIspHeight;
    int mAiIspTdmRxBufNum;
    int mAiIspMode;
    int mAiIspAutoSwitchEnable;
    int mAiIspSwitchInterval;
    int mAiIspSwitchCase;
    int mAiIspSwitchDropFrameNum;
    char mAiIspCfgBinPath2[MAX_FILE_PATH_SIZE];
    int mAiIspReserve0;
    int mAiIspReserve1;
    int mAiIspReserve2;

    struct uac_config uac_config;
}SampleUVCConfig;

typedef struct SampleUVCContext
{
    SampleUVCDevice uvc_device;
    SampleUVCConfig uvc_config;
    struct uvc_configfs_para *configfs_para;
    SampleUVCCmdLineParam mCmdLinePara;
    pthread_t uvc_task_trd;
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    int exit_flag;
} SampleUVCContext;

int initSampleUVCContext();
int destroySampleUVCContext();

#endif  /* _SAMPLE_UVC_H_ */
