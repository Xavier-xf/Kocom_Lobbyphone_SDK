#define UVC_DEMO_LOG_LEVEL UVC_DEMO_LOG_DEBUG

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <unistd.h>

#include "../utils/include/debug.h"

#include "rt_aiservice_pdet.h"

static int nv21_draw_point(unsigned char* yBuffer, unsigned char* uvBuffer, int w, int h, int x, int y, int yColor, int uColor, int vColor) {
    if (x < 0 || x >= w) return -1;
    if (y < 0 || y >= h) return -1;
    yBuffer[y*w+x] = yColor;
    uvBuffer[(y/2)*w+x/2*2] = uColor;
    uvBuffer[(y/2)*w+x/2*2+1] = vColor;
    return 0;
}

static int nv21_draw_rect(unsigned char* yBuffer, unsigned char* uvBuffer, int w, int h, int left, int top, int right, int bottom, int yColor, int uColor, int vColor) {
    int i;
    for (i = left; i <= right; i++) {
        nv21_draw_point(yBuffer, uvBuffer, w, h, i, top, yColor, uColor, vColor);
        nv21_draw_point(yBuffer, uvBuffer, w, h, i, bottom, yColor, uColor, vColor);
    }
    for (i = top; i <= bottom; i++) {
        nv21_draw_point(yBuffer, uvBuffer, w, h, left, i, yColor, uColor, vColor);
        nv21_draw_point(yBuffer, uvBuffer, w, h, right, i, yColor, uColor, vColor);
    }
    return 0;
}

int pdet_init(SamplePdetInfo *pPdetInfo)
{
    unsigned int mem_size = 0;

    if (access(pPdetInfo->pdet_model_filename, F_OK)) {
        loge("fatal error! pdet model %s not exist!", pPdetInfo->pdet_model_filename);
        return -1;
    }

    pPdetInfo->nbinfo = awnn_get_info(pPdetInfo->pdet_model_filename);
    logd("human nbinfo %s %s %u %u %u %f\n", pPdetInfo->nbinfo->name, pPdetInfo->nbinfo->md5,
            pPdetInfo->nbinfo->width, pPdetInfo->nbinfo->height, pPdetInfo->nbinfo->mem_size,
            pPdetInfo->nbinfo->thresh);
    mem_size += pPdetInfo->nbinfo->mem_size;

    awnn_init(mem_size);

    pPdetInfo->awnn_context = awnn_create(pPdetInfo->pdet_model_filename);
    if (!pPdetInfo->awnn_context) {
        loge("fatal errpr! awnn create fail!");
    }

    pPdetInfo->buf_len = pPdetInfo->nbinfo->width * pPdetInfo->nbinfo->height * 3 / 2;
    pPdetInfo->buf_vir_addr = malloc(pPdetInfo->buf_len);
    if (!pPdetInfo->buf_vir_addr)
        loge("fatal error! malloc buf fail!");
    memset(pPdetInfo->buf_vir_addr, 0, pPdetInfo->buf_len);

    return 0;
}


void pdet_deinit(SamplePdetInfo *pPdetInfo)
{
    awnn_destroy(pPdetInfo->awnn_context);
    awnn_uninit();
    if (pPdetInfo->buf_vir_addr) {
        free(pPdetInfo->buf_vir_addr);
        pPdetInfo->buf_len = 0;
    }
}

int pdet_run(SamplePdetInfo *pPdetInfo, SamplePdetInputFrameInfo *in_frame)
{
    if (in_frame->vir_addr[0])
        memcpy(pPdetInfo->buf_vir_addr, in_frame->vir_addr[0], (in_frame->width * in_frame->height));
    if (in_frame->vir_addr[1]) {
        memcpy(pPdetInfo->buf_vir_addr + (pPdetInfo->nbinfo->width * pPdetInfo->nbinfo->height), in_frame->vir_addr[1],
                    (in_frame->width * in_frame->height / 2));
    }

    unsigned char *body_input_buf[2] = {NULL, NULL};
    body_input_buf[0] = pPdetInfo->buf_vir_addr;
    body_input_buf[1] = pPdetInfo->buf_vir_addr + (pPdetInfo->nbinfo->width * pPdetInfo->nbinfo->height);
    awnn_set_input_buffers(pPdetInfo->awnn_context, body_input_buf);

    awnn_run(pPdetInfo->awnn_context);

    Awnn_Post_t post;
    post.type = AWNN_DET_POST_HUMANOID_1;
    post.width = pPdetInfo->nbinfo->width;
    post.height = pPdetInfo->nbinfo->height;
    post.thresh = pPdetInfo->nbinfo->thresh;
    memset(&pPdetInfo->detect_result, 0, sizeof(pPdetInfo->detect_result));
    awnn_det_post(pPdetInfo->awnn_context, &post, &pPdetInfo->detect_result);

    return 0;
}
