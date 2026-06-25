#ifndef __VNN_UTILS_H__
#define __VNN_UTILS_H__

#include "vip_lite.h"
#include "vip_lite_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define MAX_OBJECT_DET_NUM 200
#define BILLION                                 1000000000

typedef struct _object_box
{
    int   label;
    float prob;
    int   x1;
    int   y1;
    int   x2;
    int   y2;
} Object_box;

typedef struct _detect_result
{
    int num;
    Object_box obj_box[MAX_OBJECT_DET_NUM];
}Detect_result;

typedef struct _vip_network_items {
    /* argv information. */
    char           *nbg_name;
    int             input_count;
    char          **input_names;
    int             output_count;
    char          **output_names;

    /* VIP lite buffer objects. */
    vip_network     network;
    vip_buffer     *input_buffers;
    vip_buffer     *output_buffers;
} vip_network_items;

typedef union{
    unsigned int u;
    float f;
}_fp32_t;

#define _CHECK_PTR( ptr, lbl )      do {\
    if( NULL == ptr ) {\
        printf("Error: %s: %s at %d\n", __FILE__, __FUNCTION__, __LINE__);\
        goto lbl;\
    }\
} while(0)

#define _CHECK_STATUS( stat, lbl )  do {\
    if( VIP_SUCCESS != stat ) {\
        printf("Error: %s: %s at %d\n", __FILE__, __FUNCTION__, __LINE__);\
        goto lbl;\
    }\
} while(0)

int net_hd_pstprocess(float **p_dst, Detect_result *p_out, float conf_thres, float nms_thres, int src_w, int src_h);

vip_int32_t  uint8_to_fp32(float* fp_pst, vip_uint8_t *data_src,  vip_int32_t buff_size, vip_int32_t zeroPoint, vip_float_t scale);
vip_status_e vnn_CreateInOutBufPrepareNetwork(
    vip_network_items *network_items);
vip_status_e vnn_ReleaseNeuralNetwork(
    vip_network_items *network_items);
int vnn_netrun(vip_network_items *network_items);
int vnn_output2float(float **output_float, void **outdata_list, int *outdatas_size,
                    int *outdatas_format,int *zeroPoints,
                    float *scales, int out_count);
vip_status_e destroy_network(
    vip_network_items *network_items);
void destroy_network_items(
    vip_network_items *network_items);
float fp16_to_fp32(const short in);


int get_inputdata_msg(vip_network_items *network_items, void **inputdata_list, int *inputdata_size, int *input_count);
int get_outputdata_msg (vip_network_items *network_items, void **outdata_list, int *outdatas_size, int *outdatas_format,
                     int *zeroPoints,  float *scales, int *out_count );
#ifdef __cplusplus
}
#endif

#endif