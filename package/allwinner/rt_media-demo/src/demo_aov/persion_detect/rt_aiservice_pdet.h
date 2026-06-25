#ifndef __AISERVICE_PDET_H__
#define __AISERVICE_PDET_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "awnn.h"

#define MODULE_MAX_FILE_PATH_SIZE  (256)

typedef struct SamplePdetInputFrameInfo {
    int pix_format;
    unsigned int width;
    unsigned int height;
    void *vir_addr[3];
    unsigned int phy_addr[3];
} SamplePdetInputFrameInfo;

typedef struct SamplePdetInfo
{
    int pdet_input_w;
	int pdet_input_h;
	int pdet_input_c;
	float pdet_conf_thres;

    //AW_Det_Container *pdet_container;
    //AW_Det_Outputs *pdet_outputs;
    awnn_info_t *nbinfo;
    Awnn_Context_t *awnn_context;
    void *buf_vir_addr;
    unsigned int buf_len;
    Awnn_Result_t detect_result;

    char pdet_model_filename[MODULE_MAX_FILE_PATH_SIZE];
} SamplePdetInfo;

int pdet_init(SamplePdetInfo *pPdetInfo);
void pdet_deinit(SamplePdetInfo *pPdetInfo);
int pdet_run(SamplePdetInfo *pPdetInfo, SamplePdetInputFrameInfo *input_frame);

#ifdef __cplusplus
}
#endif

#endif // __AISERVICE_PDET_H__
