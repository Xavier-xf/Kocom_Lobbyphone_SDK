#ifndef __AW_ALG_PIX_KIT_H__
#define __AW_ALG_PIX_KIT_H__

#include <linux/types.h>
#include <pthread.h>
#include <stdbool.h>

#include "p_code.h"
#include "pix_algokit_public_type.h"
#include "pix_facekit_api.h"
#include "pix_palmkit_api.h"
#include "doorlock_common.h"
#include "aw_alg.h"

#ifdef __cplusplus
extern "C" {
#endif /* End of #ifdef __cplusplus */

#define CARE_INSIDE_IMAGE_PERSON_NUM 1
#define PIX_HARDWARE_INFO_BYTES 32
#define PIX_AUTH_KEY_BYTES 128

#define NIR_IMG_WIDTH 480
#define NIR_IMG_HEIGHT 640
#define RGB_IMG_WIDTH 480
#define RGB_IMG_HEIGHT 640

#define ALG_MAX_PIXFACEKIT_NUM CARE_INSIDE_IMAGE_PERSON_NUM
#define ALG_DEFAULT_PIXFACEKIT_NUM CARE_INSIDE_IMAGE_PERSON_NUM
#define ALG_PIXFACEKIT_ID_LEN PIX_FR_FEATURE_BYTES

typedef enum {
    FEATURE_TYPE_FACE = 0,
    FEATURE_TYPE_PALM = 1,
    FEATURE_TYPE_NULL = 8
} feature_type_t;

typedef struct alg_pix_kit_s {
    uint8_t *detect_model_path;
    uint8_t *face_model_path;
    uint8_t *face_lazy_model_path;
    uint8_t *palm_model_path;
    uint8_t auth_key[PIX_AUTH_KEY_BYTES];
    void *yuv_vir_addrs[2];
    void *yuv_phy_addrs[2];
    uint32_t yuv_sizes[2];
    uint32_t yuv_widths[2];
    uint32_t yuv_heights[2];
    uint32_t yuv_formats[2];
    aw_alg_t *aw_alg_info;
} alg_pix_kit_t;

typedef struct alg_pix_face_result_s {
    uint32_t x1;
    uint32_t y1;
    uint32_t x2;
    uint32_t y2;
    float fr_score_threshold;
    float fr_score;
    float mask_threshold;
    float mask_score;
    float liveness_threshold;
    float liveness;
    float distance;
    float left_right;
    float roll;
    float up_down;
    pix_fr_feature_info_t fr_feature;
} alg_pix_face_result_t;

typedef struct alg_pix_palm_result_s {
    float pr_score_threshold;
    float pr_score;
    pix_pr_feature_info_t pr_feature;
} alg_pix_palm_result_t;

typedef struct alg_pix_result_s {
    alg_pix_face_result_t face_result_list[ALG_MAX_PIXFACEKIT_NUM];
    alg_pix_palm_result_t palm_result;
} alg_pix_result_t;

int alg_pix_init(alg_pix_kit_t *alg_pix_info);
int alg_pix_deinit(alg_pix_kit_t *alg_pix_info);
int alg_pix_detect(alg_pix_kit_t *alg_pix_info, uint8_t register_flag,
                alg_pix_result_t *pPixResult, uint8_t *detect_num, uint8_t *detect_type);
int alg_pix_face_compare(alg_pix_kit_t *alg_pix_info, pix_fr_feature_info_t *feature0,
                      pix_fr_feature_info_t *feature1, float *fr_score);
int alg_pix_palm_compare(alg_pix_kit_t *alg_pix_info, pix_pr_feature_info_t *feature0,
                      pix_pr_feature_info_t *feature1, float *pr_score);
#ifdef __cplusplus
}
#endif /* End of #ifdef __cplusplus */

#endif /* __AW_ALG_PIX_KIT_H__ */
