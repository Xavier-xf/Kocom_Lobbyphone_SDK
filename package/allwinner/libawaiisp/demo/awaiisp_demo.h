#ifndef _AWAIISP_DEMO_H_
#define _AWAIISP_DEMO_H_

#ifdef __cplusplus
       extern "C" {
#endif

#define AWAIISP_COMMON_NUM_MAX  (4)
#define MAX_FILE_PATH_SIZE  256
#define DEMO_AIISP_FILL_LEN 64
#define DEMO_AIISP_HEAD_LEN 1080

/* print level */
#define awaiisp_common_err_print(fmt, arg...)      printf("[AWAIISP_COMMON_ERR]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define awaiisp_common_warn_print(fmt, arg...)     printf("[AWAIISP_COMMON_WRN]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define awaiisp_common_dbg_print(fmt, arg...)      printf("[AWAIISP_COMMON_DBG]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define awaiisp_common_ver_print(fmt, arg...)      //printf("[AWAIISP_COMMON_VER]%s:%u: " fmt "\n", __FUNCTION__, __LINE__, ##arg)

typedef struct demo_vin_isp_tdm_event_status {
    unsigned char dev_id;
    void *iommu_buf;
    unsigned int buf_size;
    unsigned char buf_id;
    unsigned int head_len;
    unsigned int fill_len;
} demo_vin_isp_tdm_event_status; /* same as vin_isp_tdm_event_status */

typedef struct DemoFrame
{
    demo_vin_isp_tdm_event_status status;
    void *pvirbuffer;
    int mBufferId;
    int datasize;
    struct list_head mList;
}DemoFrame;

typedef struct DemoCmdLineParam
{
    char mConfigFilePath[MAX_FILE_PATH_SIZE];
}DemoCmdLineParam;

typedef struct DemoConfig
{
    char mDemoAiIspLutNbgFilePath[MAX_FILE_PATH_SIZE];
    char mDemoAiIspNbgFilePath[MAX_FILE_PATH_SIZE];
    int mDemoAiIspModelVersion;
    int mDemoAiIspWidth;
    int mDemoAiIspHeight;
    int mDemoAiIspTdmRxBufNum;
    int mDemoAiIspMode;
    int mDemoAiIspReserve0;
    int mDemoAiIspReserve1;
    int mDemoAiIspReserve2;
    int mDemoAiIspFrameRate;
    int mDemoAiIspSaveAfterFrame;
    char mDemoAiIspInputFilePath[MAX_FILE_PATH_SIZE];
    char mDemoAiIspOutputFilePath[MAX_FILE_PATH_SIZE];
}DemoConfig;

typedef struct demo_awaiisp_common_context {
    DemoCmdLineParam mCmdLinePara;
    DemoConfig mConfigPara;

    int isp_id;
    FILE* infile;
    FILE* outfile;
    int buf_size;
    pthread_t TestThreadId;
    int sendframecount;
    int recvframecount;

    struct SunxiMemOpsS *pMemops;
    struct list_head        mDemoFrameIdleList;
    struct list_head        mDemoFrameUsedList;
    pthread_mutex_t         mDemoFrameListMutex;
    void (*tdm_buffer_process_callback)(void *func);
} demo_awaiisp_common_context;

#ifdef __cplusplus
       }
#endif

#endif

