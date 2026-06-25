#ifndef __TDM_RAW_PROCESS_H__
#define __TDM_RAW_PROCESS_H__

#ifdef __cplusplus
       extern "C" {
#endif

#define TDM_RAW_PROCESS_FILE_PATH_MAX_LEN  (100)

typedef enum tdm_raw_process_type {
    TDM_RAW_DUMP_8BIT,
    TDM_RAW_DUMP_10BIT,
    TDM_RAW_DUMP_8BIT_FOR_TOOLS,
    TDM_RAW_DUMP_10BIT_FOR_TOOLS,
    TDM_RAW_SEND_8BIT,
    TDM_RAW_SEND_10BIT,
    TDM_RAW_PROCESS_LAST
} tdm_raw_process_type;

typedef struct tdm_raw_process_config_param {
    tdm_raw_process_type type;
    int width;
    int height;
    char tdm_raw_file_path[TDM_RAW_PROCESS_FILE_PATH_MAX_LEN];
    int frame_cnt_min;
    int frame_cnt_max;
} tdm_raw_process_config_param;

int tdm_raw_process_open(int isp, tdm_raw_process_config_param *param);
int tdm_raw_process_start(int isp);
int tdm_raw_process_stop(int isp);
int tdm_raw_process_close(int isp);

#ifdef __cplusplus
       }
#endif

#endif
