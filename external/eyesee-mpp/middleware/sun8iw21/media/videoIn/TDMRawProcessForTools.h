#ifndef __TDMRAWPROCESSFORTOOLS_H__
#define __TDMRAWPROCESSFORTOOLS_H__

#include "tmessage.h"
#include "ComponentCommon.h"

#ifdef __cplusplus
       extern "C" {
#endif

#define TDM_RAW_PROCESS_FILE_PATH_MAX_LEN  (100)
#define MAX_LEN 301

typedef enum tdm_raw_process_type {
    TDM_RAW_DUMP_8BIT,
    TDM_RAW_DUMP_10BIT,
    TDM_RAW_DUMP_8BIT_FOR_TOOLS,
    TDM_RAW_DUMP_10BIT_FOR_TOOLS,
    TDM_RAW_PROCESS_LAST
} tdm_raw_process_type;

typedef struct tdm_raw_process_config_param {
    int mbus_code;
    int width;
    int height;
    char tdm_raw_file_path[TDM_RAW_PROCESS_FILE_PATH_MAX_LEN];
    char tdm_raw_flag_path[TDM_RAW_PROCESS_FILE_PATH_MAX_LEN];
    int frame_cnt_min;
    int frame_cnt_max;
} tdm_raw_process_config_param;

typedef struct RawInfo
{
    int isp_id;
    int width;
    int height;
    int fps;
    int wdr;
    char flagpath[MAX_LEN];
    char isp_cfg_path[MAX_LEN];
    char framepath[MAX_LEN];
    char fixedflagpath[13];
    tdm_raw_process_config_param tdm_raw_config_param;
    message_queue_t *pMsgQueue;
}RawInfo;

int tdm_raw_process_ft_open(int isp, tdm_raw_process_config_param *param);
int tdm_raw_process_ft_start(int isp, tdm_raw_process_config_param *param);
int tdm_raw_process_ft_stop(int isp);
int tdm_raw_process_ft_close(int isp);
static void *SaveTdmRawData(void *pThreadData);
static char *ReadFlagPath(char* flagpath);
pthread_t SendRawToApp(struct sensor_config *stConfig, int isp_id, message_queue_t* msg_que);

#ifdef __cplusplus
       }
#endif

#endif
