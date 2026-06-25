#include "vnn_utils.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <vector>

#include "stdio.h"

static void hwc2chw(unsigned char *p_src, int h, int w, int c)
{
    int stride = h * w;
    int file_size = stride * 3;
    unsigned char *tmp = (unsigned char *)malloc(file_size * sizeof(char));
    for (int i = 0; i != stride; ++i) {
        for (int c = 0; c != 3; ++c) {
            tmp[c * stride + i] = p_src[i * 3 + c];
        }
    }
    memcpy(p_src, tmp, file_size);
    free(tmp);
}

static void rgb2bgr(unsigned char *p_src, int h, int w, int c)
{
    int channel_size = h * w;
    unsigned char *tmp = (unsigned char *)malloc(channel_size * sizeof(char));
    memcpy(tmp, p_src, channel_size);
    memcpy(p_src, p_src + channel_size * 2, channel_size);
    memcpy(p_src + channel_size * 2, tmp, channel_size);
    free(tmp);
}

int rgb888_draw_line(unsigned char *addr, int width, int height, int x1, int y1, int x2, int y2,
                     unsigned char color)
{
    int i, j, start;

    // top
    start = (y1 * width + x1) * 3;
    for (i = x1; i < x2; i++, start += 3) {
        *(addr + start + 0) = 0xFF;
        *(addr + start + 1) = 0x00;
        *(addr + start + 2) = 0x00;
    }
    // bottom
    start = (y2 * width + x1) * 3;
    for (i = x1; i < x2; i++, start += 3) {
        *(addr + start + 0) = 0xFF;
        *(addr + start + 1) = 0x00;
        *(addr + start + 2) = 0x00;
    }
    // left
    start = (y1 * width + x1) * 3;
    for (i = y1; i < y2; i++, start += width * 3) {
        *(addr + start + 0) = 0xFF;
        *(addr + start + 1) = 0x00;
        *(addr + start + 2) = 0x00;
    }
    // right
    start = (y1 * width + x2) * 3;
    for (i = y1; i < y2; i++, start += width * 3) {
        *(addr + start + 0) = 0xFF;
        *(addr + start + 1) = 0x00;
        *(addr + start + 2) = 0x00;
    }

    return 0;
}

typedef struct box_f {
    float x;
    float y;
    float w;
    float h;
} box;

typedef struct {
    float x;
    float y;
    float w;
    float h;
    int sort_class;
    float score;
    float class_conf;
} detection_pd;

static float sigmoid(float data) { return 1.0 / (1.0 + expf(-data)); }

static void interset(detection_pd &bbox1)
{
    box bbox2 = {0.0, 0.0, 1.0, 1.0};
    bbox1.x = (bbox1.x > bbox2.x) ? bbox1.x : bbox2.x;
    bbox1.y = (bbox1.y > bbox2.y) ? bbox1.y : bbox2.y;
    bbox1.w = (bbox1.w < bbox2.w) ? bbox1.w : bbox2.w;
    bbox1.h = (bbox1.h < bbox2.h) ? bbox1.h : bbox2.h;
}

static int find_max_index(float *data, int len)
{
    float max_data = 0;
    int index = 0;
    max_data = *data;
    data++;
    for (int i = 1; i < len; i++) {
        if ((*data) > max_data) {
            max_data = *data;
            index = i;
        }
        data++;
    }
    return index;
}

static void get_box(float *data, int h, int w, int c, std::vector<detection_pd> &detection_pd_box,
                    int number_class, float *biase, float conf_thres, int anchor_num)
{
    detection_pd det_tmp;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            for (int k = 0; k < anchor_num; k++) {
                det_tmp.score = sigmoid(data[k * (number_class + 5) + 4]);
                if (det_tmp.score > conf_thres) {
                    det_tmp.w = expf(data[k * (number_class + 5) + 2]) * biase[2 * k];
                    det_tmp.h = expf(data[k * (number_class + 5) + 3]) * biase[2 * k + 1];
                    det_tmp.x = (x + sigmoid(data[k * (number_class + 5)])) / w - det_tmp.w / 2;
                    det_tmp.y = (y + sigmoid(data[k * (number_class + 5) + 1])) / h - det_tmp.h / 2;
                    det_tmp.w += det_tmp.x;
                    det_tmp.h += det_tmp.y;
                    det_tmp.sort_class =
                        find_max_index(data + k * (number_class + 5) + 5, number_class);
                    det_tmp.class_conf =
                        sigmoid(*(data + k * (number_class + 5) + 5 + det_tmp.sort_class));
                    interset(det_tmp);
                    detection_pd_box.push_back(det_tmp);
                    detection_pd it = detection_pd_box[detection_pd_box.size() - 1];
                }
            }
            data = data + c;
        }
    }
}

static bool nms_cmp(detection_pd a, detection_pd b) { return (a.score < b.score); }

static float box_intersection(detection_pd a, detection_pd b)
{
    float inter_x1 = a.x > b.x ? a.x : b.x;
    float inter_y1 = a.y > b.y ? a.y : b.y;
    float inter_x2 = a.w < b.w ? a.w : b.w;
    float inter_y2 = a.h < b.h ? a.h : b.h;
    float w = inter_x2 - inter_x1 + 1;
    float h = inter_y2 - inter_y1 + 1;
    if (w < 0 || h < 0) return 0;
    float area = w * h;
    return area;
}

static float box_union(detection_pd a, detection_pd b)
{
    float i = box_intersection(a, b);
    float u = (a.w - a.x + 1) * (a.h - a.y + 1) + (b.w - b.x + 1) * (b.h - b.y + 1) - i;
    if (u < 0.001) return 0.001;
    return u;
}

static float box_iou(detection_pd a, detection_pd b)
{
    return box_intersection(a, b) / box_union(a, b);
}

static float get_intersect_area(detection_pd bbox1, detection_pd bbox2)
{
    return box_iou(bbox1, bbox2);
}

static void yolo_nms_multiclass(std::vector<detection_pd> &detection_pd_box, float thresh)
{
    int totol_box = detection_pd_box.size();
    for (int i = 0; i < totol_box; i++) {
        detection_pd_box[i].score = detection_pd_box[i].score * detection_pd_box[i].class_conf;
    }
    std::sort(detection_pd_box.begin(), detection_pd_box.end(), nms_cmp);

    for (int i = 0; i < totol_box; i++) {
        detection_pd_box[i].score = detection_pd_box[i].score / detection_pd_box[i].class_conf;
    }

    int pick_box = 0;
    detection_pd tmp_box;
    memset(&tmp_box, 0, sizeof(detection_pd));
    if (totol_box > 0) {
        tmp_box = detection_pd_box[totol_box - 1];
    }
    int j = totol_box - 1;
    // float weight = 0;
    std::vector<detection_pd> invalid;
    detection_pd weight;
    float weight_sum = 0;
    invalid.push_back(tmp_box);
    while (totol_box > 0) {
        for (int i = j - 1; i >= 0; i--) {
            if (get_intersect_area(tmp_box, detection_pd_box[i]) > thresh) {
                if (tmp_box.sort_class == detection_pd_box[i].sort_class) {
                    invalid.push_back((detection_pd_box)[i]);
                    detection_pd_box.erase(std::begin(detection_pd_box) + i);
                }
            }
        }
        // weight avg
        weight.x = 0;
        weight.y = 0;
        weight.w = 0;
        weight.h = 0;
        weight_sum = 0;
        for (int k = 0; k < invalid.size(); k++) {
            weight.x += invalid[k].x * invalid[k].score;
            weight.y += invalid[k].y * invalid[k].score;
            weight.w += invalid[k].w * invalid[k].score;
            weight.h += invalid[k].h * invalid[k].score;
            weight_sum += invalid[k].score;
        }
        pick_box = pick_box + 1;
        totol_box = (detection_pd_box).size();

        detection_pd_box[totol_box - pick_box].x = weight.x / weight_sum;
        detection_pd_box[totol_box - pick_box].y = weight.y / weight_sum;
        detection_pd_box[totol_box - pick_box].w = weight.w / weight_sum;
        detection_pd_box[totol_box - pick_box].h = weight.h / weight_sum;

        invalid.clear();
        j = totol_box - pick_box - 1;

        if (j <= 0) break;
        tmp_box = detection_pd_box[j];
        invalid.push_back(detection_pd_box[j]);
    }
}

static float img_clip(float x, float min, float max) { return x < min ? min : x > max ? max : x; }

// char tmp_char[ 3584 + 90000];
//  int net_hd_pstprocess(float *p_dst, bounding_box *p_out, int * box_num, float conf_thres, float
//  nms_thres)
//  {
//      // printf("\n*** Access net_hd_pstprocess function!");

//     int max_num = *box_num;
//     int i = 0;

//     // 320*320 small
//     float anchors[24] = {0.523080003,0.652828416, 0.319123441,1.280038469,
//     0.624866514,1.123617274, 0.443425257,1.642074945, 1.094569886,1.041406358,
//     0.900775817,1.618106223, 1.550720298,1.679183872, 0.127625569,0.288732323,
//     0.181917883,0.491345645, 0.208623159,0.727570535, 0.279196269,0.976074704,
//     0.054118467,0.116619808 };

//     std::vector<detection_pd> dets;
//     dets.clear();
//     get_box(p_dst, 10, 10, 42, dets, 1, anchors, conf_thres, 7);
//     get_box(p_dst+10*10*42, 20, 20, 24, dets, 1, anchors+14, conf_thres, 4);
//     get_box(p_dst+10*10*42+20*20*24, 40, 40, 6, dets, 1, anchors+22, conf_thres, 1);

//     for(i=0; i<dets.size(); i++)
//     {
//         int w = 320;
//         int h = 320;
//         dets[i].x = img_clip(dets[i].x * w, 0, w-1);
//         dets[i].y = img_clip(dets[i].y * h, 0, h-1);
//         dets[i].w = img_clip(dets[i].w * w, 0, w-1);
//         dets[i].h = img_clip(dets[i].h * h, 0, h-1);
//     }

//     yolo_nms_multiclass(dets, nms_thres);

//     int count=0;
//     for(i=0; i<dets.size(); i++)
//     {
//         if (i > max_num-1) break;
//         p_out->score = int(dets[i].score*100);
//         p_out->x1    = int(dets[i].x);
//         p_out->y1    = int(dets[i].y);
//         p_out->x2    = int(dets[i].w);
//         p_out->y2    = int(dets[i].h);
//         printf("\nHuman box %d: %d %d %d %d,\tScore: %d", i, p_out->x1, p_out->y1, p_out->x2,
//         p_out->y2, p_out->score); count++; p_out++;
//     }
//     printf("\n\n");
//     *box_num = count;
// }

// int net_hd_saveimg(unsigned char *p_src, char *file_name, int box_num, bounding_box *box_hd)
// {
//     char fname[1256] = {0, };

//     for(int i=0; i<box_num; i++)
//     {
//         rgb888_draw_line(p_src, 320, 320, box_hd->x1, box_hd->y1, box_hd->x2, box_hd->y2, 0x00);
//         box_hd++;
//     }

//     sprintf(&fname[0], "results/%s", file_name);
//     if (!tje_encode_to_file(fname, 320, 320, 3, true, (const unsigned char*)p_src))
//     {
//         printf("\nSave JPEG failed.\n");
//     }
// }

// ##############################

int net_hd_pstprocess(float **p_dst, Detect_result *p_out, float conf_thres, float nms_thres,
                      int src_w, int src_h)
{
    // printf("\n*** Access net_hd_pstprocess function!");

    int i = 0;

    // 320*320 small
    float anchors[24] = {0.523080003, 0.652828416, 0.319123441, 1.280038469, 0.624866514,
                         1.123617274, 0.443425257, 1.642074945, 1.094569886, 1.041406358,
                         0.900775817, 1.618106223, 1.550720298, 1.679183872, 0.127625569,
                         0.288732323, 0.181917883, 0.491345645, 0.208623159, 0.727570535,
                         0.279196269, 0.976074704, 0.054118467, 0.116619808};

    std::vector<detection_pd> dets;
    dets.clear();
    // get_box(p_dst, 10, 10, 42, dets, 1, anchors, conf_thres, 7);
    // get_box(p_dst+10*10*42, 20, 20, 24, dets, 1, anchors+14, conf_thres, 4);
    // get_box(p_dst+10*10*42+20*20*24, 40, 40, 6, dets, 1, anchors+22, conf_thres, 1);
    get_box(p_dst[0], 10, 10, 42, dets, 1, anchors, conf_thres, 7);
    get_box(p_dst[1], 20, 20, 24, dets, 1, anchors + 14, conf_thres, 4);
    get_box(p_dst[2], 40, 40, 6, dets, 1, anchors + 22, conf_thres, 1);

    int w = src_w;
    int h = src_h;
    for (i = 0; i < dets.size(); i++) {
        dets[i].x = img_clip(dets[i].x * w, 0, w - 1);
        dets[i].y = img_clip(dets[i].y * h, 0, h - 1);
        dets[i].w = img_clip(dets[i].w * w, 0, w - 1);
        dets[i].h = img_clip(dets[i].h * h, 0, h - 1);
    }

    yolo_nms_multiclass(dets, nms_thres);

    int count = 0;
    p_out->num = dets.size();
    for (i = 0; i < dets.size(); i++) {
        p_out->obj_box[i].prob = dets[i].score;
        p_out->obj_box[i].x1 = int(dets[i].x);
        p_out->obj_box[i].y1 = int(dets[i].y);
        p_out->obj_box[i].x2 = int(dets[i].w);
        p_out->obj_box[i].y2 = int(dets[i].h);
        //printf("BBBBHuman %d %d %d %d %d %d %f\n",dets.size(), i, p_out->obj_box[i].x1, p_out->obj_box[i].y1, \
        //                    p_out->obj_box[i].x2, p_out->obj_box[i].y2,  p_out->obj_box[i].prob);
        count++;
    }
    return 0;
}

static vip_int32_t zeroPoint_old = -1;
static vip_float_t scale_old = -1;
vip_int32_t uint8_to_fp32(float *fp_pst, vip_uint8_t *data_src, vip_int32_t buff_size,
                          vip_int32_t zeroPoint, vip_float_t scale)
{
    vip_float_t result = 0.0f;
    float *fp_pst_temp = fp_pst;
    vip_uint8_t *data_src_temp = data_src;
    if (zeroPoint_old != -1) {
        if (zeroPoint != zeroPoint_old) {
            // printf("%s line %d, zero: old %d, now %d.\n", __func__, __LINE__, zeroPoint_old,
            // zeroPoint);
        }
    }

    if (scale_old != -1) {
        if (scale != scale_old) {
            // printf("%s line %d, scale: old %f, now %f.\n", __func__, __LINE__, scale_old, scale);
        }
    }

    for (int j = 0; j < buff_size; j++) {
        fp_pst_temp[j] = (data_src_temp[j] - (vip_uint8_t)zeroPoint) * scale;
    }
    zeroPoint_old = zeroPoint;
    scale_old = scale;
    return 0;
}

vip_status_e vnn_CreateInOutBufPrepareNetwork(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    int input_num = 0, i = 0;
    vip_buffer_create_params_t buf_param;

    /* Get network input num */
    status = vip_query_network(network_items->network, VIP_NETWORK_PROP_INPUT_COUNT,
                               &network_items->input_count);
    _CHECK_STATUS(status, final);
    // if(input_num != network_items->input_count) {
    //     printf("Error: Graph need %d inputs, but enter %d inputs!!!\n",
    //         input_num, network_items->input_count);
    //     status = VIP_ERROR_MISSING_INPUT_OUTPUT;
    //     return status;
    // }

    /* Create input buffers */
    network_items->input_buffers =
        (vip_buffer *)malloc(sizeof(vip_buffer) * network_items->input_count);
    for (i = 0; i < network_items->input_count; i++) {
        memset(&buf_param, 0, sizeof(buf_param));
        status = vip_query_input(network_items->network, i, VIP_BUFFER_PROP_DATA_FORMAT,
                                 &buf_param.data_format);
        _CHECK_STATUS(status, final);
        status = vip_query_input(network_items->network, i, VIP_BUFFER_PROP_NUM_OF_DIMENSION,
                                 &buf_param.num_of_dims);
        _CHECK_STATUS(status, final);
        status = vip_query_input(network_items->network, i, VIP_BUFFER_PROP_SIZES_OF_DIMENSION,
                                 buf_param.sizes);
        _CHECK_STATUS(status, final);
        status = vip_query_input(network_items->network, i, VIP_BUFFER_PROP_QUANT_FORMAT,
                                 &buf_param.quant_format);
        _CHECK_STATUS(status, final);
        switch (buf_param.quant_format) {
        case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
            status = vip_query_input(network_items->network, i, VIP_BUFFER_PROP_FIXED_POINT_POS,
                                     &buf_param.quant_data.dfp.fixed_point_pos);
            _CHECK_STATUS(status, final);
            break;
        case VIP_BUFFER_QUANTIZE_TF_ASYMM:
            status = vip_query_input(network_items->network, i, VIP_BUFFER_PROP_TF_SCALE,
                                     &buf_param.quant_data.affine.scale);
            _CHECK_STATUS(status, final);
            status = vip_query_input(network_items->network, i, VIP_BUFFER_PROP_TF_ZERO_POINT,
                                     &buf_param.quant_data.affine.zeroPoint);
            _CHECK_STATUS(status, final);
            break;
        case VIP_BUFFER_QUANTIZE_NONE:
        default:
            break;
        }

        status = vip_create_buffer(&buf_param, sizeof(buf_param), &network_items->input_buffers[i]);
        _CHECK_STATUS(status, final);
    }

    /* Create output buffers */
    status = vip_query_network(network_items->network, VIP_NETWORK_PROP_OUTPUT_COUNT,
                               &network_items->output_count);
    network_items->output_buffers =
        (vip_buffer *)malloc(sizeof(vip_buffer) * network_items->output_count);
    for (i = 0; i < network_items->output_count; i++) {
        memset(&buf_param, 0, sizeof(buf_param));
        status = vip_query_output(network_items->network, i, VIP_BUFFER_PROP_DATA_FORMAT,
                                  &buf_param.data_format);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i, VIP_BUFFER_PROP_NUM_OF_DIMENSION,
                                  &buf_param.num_of_dims);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i, VIP_BUFFER_PROP_SIZES_OF_DIMENSION,
                                  buf_param.sizes);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i, VIP_BUFFER_PROP_QUANT_FORMAT,
                                  &buf_param.quant_format);
        _CHECK_STATUS(status, final);
        switch (buf_param.quant_format) {
        case VIP_BUFFER_QUANTIZE_DYNAMIC_FIXED_POINT:
            status = vip_query_output(network_items->network, i, VIP_BUFFER_PROP_FIXED_POINT_POS,
                                      &buf_param.quant_data.dfp.fixed_point_pos);
            _CHECK_STATUS(status, final);
            break;
        case VIP_BUFFER_QUANTIZE_TF_ASYMM:
            status = vip_query_output(network_items->network, i, VIP_BUFFER_PROP_TF_SCALE,
                                      &buf_param.quant_data.affine.scale);
            _CHECK_STATUS(status, final);
            status = vip_query_output(network_items->network, i, VIP_BUFFER_PROP_TF_ZERO_POINT,
                                      &buf_param.quant_data.affine.zeroPoint);
            _CHECK_STATUS(status, final);
            break;
        case VIP_BUFFER_QUANTIZE_NONE:
        default:
            break;
        }
        status =
            vip_create_buffer(&buf_param, sizeof(buf_param), &network_items->output_buffers[i]);
        _CHECK_STATUS(status, final);
    }

    /* Prepare network */
    status = vip_prepare_network(network_items->network);
    _CHECK_STATUS(status, final);

final:
    return status;
}

static vip_uint64_t get_perf_count()
{
#if defined(__linux__) || defined(__ANDROID__) || defined(__QNX__) || defined(__CYGWIN__)
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    return (vip_uint64_t)((vip_uint64_t)ts.tv_nsec + (vip_uint64_t)ts.tv_sec * BILLION);
#elif defined(_WIN32) || defined(UNDER_CE)
    LARGE_INTEGER ln;

    QueryPerformanceCounter(&ln);

    return (vip_uint64_t)ln.QuadPart;
#endif
}

vip_status_e vnn_ReleaseNeuralNetwork(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    status = destroy_network(network_items);
    _CHECK_STATUS(status, final);
    destroy_network_items(network_items);
final:
    return status;
}

int vnn_netrun(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    vip_uint64_t tmsStart, tmsEnd, sigStart, sigEnd;
    float msVal, usVal;
    sigStart = get_perf_count();
    status = vip_run_network(network_items->network);
    sigEnd = get_perf_count();
    msVal = (float)(sigEnd - sigStart) / 1000000;
    usVal = (float)(sigEnd - sigStart) / 1000;
    // printf("Run  time: %.2fms or %.2fus\n",  msVal, usVal);

    _CHECK_STATUS(status, final);
final:
    /* Destroy resources */
    return status;
}

int vnn_output2float(float **output_float, void **outdata_list, int *outdatas_size,
                     int *outdatas_format, int *zeroPoints, float *scales, int out_count)
{
    // printf("=================>    vnn_output2float       <=================\n");
    for (int i = 0; i < out_count; i++) {
        float *pp_float = output_float[i];
        vip_uint8_t *data;
        data = (vip_uint8_t *)outdata_list[i];
        // printf("=======>   output_NO %d  %d   <=================\n", i, outdatas_size[i]);
        if (outdatas_format[i] == 1) {
            short *pp = (short *)outdata_list[i];
            for (int j = 0; j < outdatas_size[i]; j++) {
                pp_float[j] = fp16_to_fp32(pp[j]);
            }
        } else {
            uint8_to_fp32(pp_float, data, outdatas_size[i], zeroPoints[i], scales[i]);
        }
        // printf("%d %d %d %d %d\n", data[0], data[2], data[3], data[4], data[5]);
        // printf("%f %f %f %f %f\n", pp_float[0], pp_float[2], pp_float[3], pp_float[4],
        // pp_float[5]);
    }
    // printf("=================>    vnn_output2float finish     <=================\n");
    return 0;
}

vip_status_e destroy_network(vip_network_items *network_items)
{
    vip_status_e status = VIP_SUCCESS;
    int i = 0;
    status = vip_finish_network(network_items->network);
    _CHECK_STATUS(status, final);
    status = vip_destroy_network(network_items->network);
    _CHECK_STATUS(status, final);
    for (i = 0; i < network_items->input_count; i++) {
        status = vip_destroy_buffer(network_items->input_buffers[i]);
        _CHECK_STATUS(status, final);
    }
    if (network_items->input_buffers) {
        free(network_items->input_buffers);
        network_items->input_buffers = VIP_NULL;
    }
    for (i = 0; i < network_items->output_count; i++) {
        status = vip_destroy_buffer(network_items->output_buffers[i]);
        _CHECK_STATUS(status, final);
    }
    if (network_items->output_buffers) {
        free(network_items->output_buffers);
        network_items->output_buffers = VIP_NULL;
    }

final:
    return status;
}

void destroy_network_items(vip_network_items *network_items)
{
    if (network_items->nbg_name) {
        free(network_items->nbg_name);
        network_items->nbg_name = VIP_NULL;
    }
    if (network_items->input_names) {
        free(network_items->input_names);
        network_items->input_names = VIP_NULL;
    }
    if (network_items) {
        free(network_items);
    }
}

float fp16_to_fp32(const short in)
{
    const _fp32_t magic = {(254 - 15) << 23};
    const _fp32_t infnan = {(127 + 16) << 23};
    // Non-sign bits
    _fp32_t o;
    o.u = (in & 0x7fff) << 13;
    o.f *= magic.f;
    if (o.f >= infnan.f) {
        o.u |= 255 << 23;
    }
    o.u |= (in & 0x8000) << 16;
    return o.f;
}

int get_inputdata_msg(vip_network_items *network_items, void **inputdata_list, int *inputdata_size,
                      int *input_count)
{
    // printf("=================>    get_inputdata_msg     <=================\n");
    void *data = NULL;
    int buff_size;
    // printf("input_count: %d  ; network_items->input_count: %d  \n", *input_count,
    // network_items->input_count);
    if (*input_count != network_items->input_count) {
        // printf("WARNING network need input: %d  but input_cunt: %d\n",
        // network_items->input_count, *input_count);
        *input_count = network_items->input_count;
    };
    for (int i = 0; i < *input_count; i++) {
        data = vip_map_buffer(network_items->input_buffers[i]);
        buff_size = vip_get_buffer_size(network_items->input_buffers[i]);
        inputdata_list[i] = data;
        inputdata_size[i] = buff_size;
        // printf("input_NO:%d buff_size:%d  addr: %p\n", i, buff_size, data);
    }
    // printf("=================>    get_inputdata_msg finish    <=================\n");
    return 0;
}

int get_outputdata_msg(vip_network_items *network_items, void **outdata_list, int *outdatas_size,
                       int *outdatas_format, int *zeroPoints, float *scales, int *out_count)
{
    // printf("=================>    get_oputdata_msg     <=================\n");
    // printf("out_count %d  ; network_items->output_count %d  \n", *out_count,
    // network_items->output_count);
    vip_status_e status = VIP_SUCCESS;
    if (*out_count != network_items->output_count) {
        // printf("WARNING network need output_count %d  but output_count: %d",
        // network_items->output_count, *out_count);
        *out_count = network_items->output_count;
    };
    for (int i = 0; i < *out_count; i++) {
        outdatas_size[i] = vip_get_buffer_size(network_items->output_buffers[i]);
        if (outdatas_size[i] <= 0) {
            status = VIP_ERROR_IO;
            return status;
        }
        outdata_list[i] = vip_map_buffer(network_items->output_buffers[i]);
        status = vip_query_output(network_items->network, i, VIP_BUFFER_PROP_DATA_FORMAT,
                                  &outdatas_format[i]);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i, VIP_BUFFER_PROP_TF_ZERO_POINT,
                                  &zeroPoints[i]);
        _CHECK_STATUS(status, final);
        status = vip_query_output(network_items->network, i, VIP_BUFFER_PROP_TF_SCALE, &scales[i]);
        _CHECK_STATUS(status, final);
        //printf("output_NO:%d buff_size:%d  dataformat:%d zeropoint:%d scale:%f addr:%p \n", \
        //             i, outdatas_size[i], outdatas_format[i], zeroPoints[i], scales[i], outdata_list[i]);
    }
    // printf("=================>    get_oputdata_msg finish     <=================\n");
final:
    return status;
}