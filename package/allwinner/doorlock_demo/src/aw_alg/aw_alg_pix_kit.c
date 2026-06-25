#include "aw_alg_pix_kit.h"

static uint8_t *pix_detect_model_ptr = NULL;
static uint8_t *pix_face_model_ptr = NULL;
static uint8_t *pix_face_lazy_model_ptr = NULL;
static uint8_t *pix_palm_model_ptr = NULL;

/* debug use */
const unsigned char pix_license_auth_key[] = {
    0x11, 0x8a, 0x4c, 0xfd, 0xef, 0x85, 0xc2, 0xe7, 0x73, 0x4e, 0x61, 0xe1, 0xeb, 0x00, 0xd8, 0x78,
    0xe1, 0x4a, 0x10, 0x64, 0xba, 0x3a, 0xed, 0xa1, 0x98, 0xde, 0x8b, 0x64, 0x5f, 0xcf, 0x78, 0x30,
    0xff, 0x49, 0xb3, 0x10, 0x17, 0x0f, 0x71, 0x0c, 0xe4, 0xcd, 0x62, 0x38, 0x70, 0xd6, 0x47, 0x2e,
    0xea, 0x10, 0x85, 0x2d, 0x5f, 0x3c, 0x97, 0x28, 0xa0, 0x34, 0x2e, 0xf4, 0x12, 0xf9, 0xbb, 0x90,
    0xa6, 0x4f, 0x56, 0x23, 0x5e, 0xc0, 0x3a, 0x5d, 0xba, 0xd5, 0x7c, 0x51, 0x37, 0x13, 0x8c, 0x63,
    0x64, 0xbe, 0x9d, 0xae, 0xc6, 0x8e, 0xb1, 0x12, 0xab, 0xaa, 0x03, 0x0e, 0xc8, 0xf5, 0x25, 0x82,
    0x9f, 0xf9, 0x8f, 0xb7, 0xab, 0x1b, 0x7b, 0x6e, 0xb4, 0xb5, 0xf3, 0xa8, 0x83, 0x2f, 0x15, 0x67,
    0x92, 0xfc, 0x14, 0x8d, 0xf3, 0xcd, 0x74, 0xbc, 0xfb, 0xa7, 0xe7, 0xa3, 0x11, 0xf7, 0x3e, 0x99,
};

static int read_file_to_malloc_heap(const char *file_path, uint8_t **content_ptr, int *file_size)
{
    *content_ptr = NULL;

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        DOORLOCK_ERR("Fail to open file %s\n", file_path);
        return SDK_CODE_INVALID_ARG;
    }
    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (*file_size < 1) {
        DOORLOCK_ERR("Bad file %d\n", file_size);
        fclose(file);
        return SDK_CODE_INVALID_ARG;
    }
    *content_ptr = (uint8_t *)malloc(*file_size);
    int read_size = fread(*content_ptr, *file_size, 1, file);
    if (read_size < 1) {
        DOORLOCK_ERR("Fail to read file %d with %d\n", *file_size, read_size);
        if (*content_ptr) {
            free(*content_ptr);
        }
        fclose(file);
        return SDK_CODE_INVALID_ARG;
    }
    fclose(file);

    return SDK_CODE_OK;
}

static void dump_pix_coord_info(pix_coord_info_t *coord_info)
{
    DOORLOCK_INFO("Face number:%d\n", coord_info->box_number);
#if 0
    for(int i = 0; i < coord_info->box_number; i++){
        DOORLOCK_INFO("Face %d coord info ", i);
        for(int j = 0; j < API_PER_BOX_ELEMENTS - 1; j++){
            printf("%f  ", coord_info->box[i * API_PER_BOX_ELEMENTS + j]);
        }
        printf("\n");
    }
#endif
}

static int get_face_info(pix_image_face_info_t *face_info, int is_register_flag,
                         uint8_t *pair_img1_ptr, uint8_t *pair_img2_ptr)
{
    int ret;
    DOORLOCK_INFO("+++++++get_face_info+++++++++++\n");

    ret = pix_set_normal_analyze_image(pair_img1_ptr, pair_img2_ptr);
    if (SDK_CODE_OK != ret) {
        DOORLOCK_ERR("Fail to pix_set_normal_analyze_image with %d\n", ret);
        goto exit;
    }
    ret = pix_normal_fd(&face_info->coord);
    if (SDK_CODE_OK != ret) {
        DOORLOCK_ERR("Fail to pix_normal_fd with %d\n", ret);
        goto exit;
    }

    dump_pix_coord_info(&face_info->coord);

    if (face_info->coord.box_number > CARE_INSIDE_IMAGE_PERSON_NUM) {
        DOORLOCK_DBG("Detect %d person, but only care %d(order by pixel area)\n",
                     face_info->coord.box_number, CARE_INSIDE_IMAGE_PERSON_NUM);
        face_info->coord.box_number = CARE_INSIDE_IMAGE_PERSON_NUM;
    }

    for (int i = 0; i < face_info->coord.box_number; i++) {
        face_info->liveness[i].score1 = 0;
        DOORLOCK_INFO("[%d] pix_liveness_check\n", i);
        ret = pix_liveness_check(i, &face_info->liveness[i].score1);
        if (SDK_CODE_OK != ret) {
            DOORLOCK_ERR("Fail to pix_liveness_check with %d\n", ret);
            face_info->liveness[i].score1 = 0;
            goto exit;
        }
        DOORLOCK_INFO("[%d] liveness score is %f \n", i, face_info->liveness[i].score1);
        // 注册阶段要求活体通过、没遮挡且姿态角符合预设值，才抽取识别特征入库
        // 识别阶段可以只要活体通过就抽取识别特征与底库进行比对（跳过遮挡、姿态角判断）
        // 识别阶段活体没过，可以调用遮挡接口判断脸部是否有遮挡、调用姿态接口判断角度是否超规格，若超了提示“正视摄像头”等相应信息。
        if (face_info->liveness[i].score1 < pix_get_liveness_threshold()) {
            DOORLOCK_INFO("[%d] Is not person\n", i);
            // 如果有多个人，这里不能直接退出（不过一般也不会处理多张脸）
            goto exit;
        } else {
            DOORLOCK_INFO("[%d] Is person\n", i);
        }
        ret = pix_with_mask_confidence(i, &face_info->mask_score[i]);
        if (SDK_CODE_OK != ret) {
            DOORLOCK_INFO("[%d] Fail to pix_mask_check with %d\n", i, ret);
            face_info->mask_score[i] = 0;
            goto exit;
        }
        DOORLOCK_INFO("[%d] the mask score is %f\n", i, face_info->mask_score[i]);
        ret = pix_get_face_attr_info(i, &face_info->face_attr[i]);
        if (SDK_CODE_OK != ret) {
            DOORLOCK_DBG("[%d] Fail to pix_get_face_attr_info with %d\n", i, ret);
            memset(&face_info->face_attr[i], 0, sizeof(pix_face_attr_info_t));
            goto exit;
        }
        DOORLOCK_INFO("[%d] the face attr info %f %f %f %f\n", i, face_info->face_attr[i].distance,
                      face_info->face_attr[i].left_right, face_info->face_attr[i].roll,
                      face_info->face_attr[i].up_down);

        ret = pix_get_face_fr_feature(i, is_register_flag, &face_info->fr_feature[i]);
        if (SDK_CODE_OK != ret) {
            DOORLOCK_DBG("[%d] Fail to pix_get_face_fr_feature with %d\n", i, ret);
            memset(&face_info->fr_feature[i], 0, sizeof(pix_fr_feature_info_t));
            goto exit;
        }
        DOORLOCK_INFO("[%d] pix_get_face_fr_feature end\n", i);
    }

exit:
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int get_palm_info(pix_pr_feature_info_t *pr_info, int is_register_flag, uint8_t *pair_img1_ptr,
                  uint8_t *pair_img2_ptr)
{
    int ret = 0;
    int quality_is_good = 0;
    int palm_index = 0;
    float palm_liveness_score = 0;
    pix_detect_target_type_e target_type;
    pix_coord_info_t coord_info;

    DOORLOCK_INFO("+++++++get_palm_info+++++++++++\n");
    ret = pix_set_normal_analyze_image_palm(pair_img1_ptr, pair_img2_ptr);
    if (SDK_CODE_OK != ret) {
        DOORLOCK_ERR("Fail to pix_set_normal_analyze_image_palm with %d\n", ret);
        goto exit;
    }

    ret = pix_get_palm_liveness_score(palm_index, &palm_liveness_score);
    if (ret != SDK_CODE_OK) {
        DOORLOCK_ERR("Fail to pix_get_palm_liveness_score with %d\n", ret);
        goto exit;
    }

    DOORLOCK_INFO("palm_liveness_score is %f\n", palm_liveness_score);
    if (palm_liveness_score < pix_palm_liveness_threshold()) {
        // DOORLOCK_ERR("Not person hand!\n");
        ret = -1;
        goto exit;
    }

    ret = pix_get_pr_feature(palm_index, is_register_flag, pr_info);
    if (ret != SDK_CODE_OK) {
        DOORLOCK_ERR("Fail to pix_get_pr_feature with %d\n", ret);
        goto exit;
    }

#if 0
    if (is_register_flag) {
        pix_is_palm_quality_good_for_register(palm_index, quality_is_good);
        if (quality_is_good == 0) {
            DOORLOCK_ERR("palm quality is not good!\n");
            ret = -1;
            goto exit;
        }
    }
#endif

    return SDK_CODE_OK;

exit:
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int alg_pix_init(alg_pix_kit_t *alg_pix_info)
{
    int ret = 0;
    int detect_model_bytes;
    int face_mode_bytes;
    int face_lazy_mode_bytes;
    int palm_model_bytes;
    DOORLOCK_DBG("enter ===>\n");

    aw_alg_t *aw_alg_info = alg_pix_info->aw_alg_info;
    if (aw_alg_info == NULL) {
        DOORLOCK_ERR("aw_alg_info == NULL\n");
        return -1;
    }
    const char *facekit_version = pix_facekit_version();
    DOORLOCK_INFO("The version of the facekit is:%s\n", facekit_version);
    pthread_mutex_lock(&aw_alg_info->alg_mutex);
    if (alg_pix_info->detect_model_path == NULL ||
        access(alg_pix_info->detect_model_path, R_OK) != 0) {
        DOORLOCK_ERR("DetectModel Path not right\n");
        ret = -1;
        goto exit;
    }
    if (alg_pix_info->face_model_path == NULL || access(alg_pix_info->face_model_path, R_OK) != 0) {
        DOORLOCK_ERR("FaceModel Path not right\n");
        ret = -1;
        goto exit;
    }
    if (alg_pix_info->face_lazy_model_path == NULL ||
        access(alg_pix_info->face_lazy_model_path, R_OK) != 0) {
        DOORLOCK_ERR("FaceLazyModelPath not right\n");
        ret = -1;
        goto exit;
    }
    if (alg_pix_info->palm_model_path == NULL || access(alg_pix_info->palm_model_path, R_OK) != 0) {
        DOORLOCK_ERR("mPalmModel Path not right\n");
        ret = -1;
        goto exit;
    }

    ret = read_file_to_malloc_heap(alg_pix_info->detect_model_path, &pix_detect_model_ptr,
                                   &detect_model_bytes);
    if (ret != SDK_CODE_OK) {
        DOORLOCK_ERR("Fail to read %s\n", alg_pix_info->detect_model_path);
        goto exit;
    }
    ret = read_file_to_malloc_heap(alg_pix_info->face_model_path, &pix_face_model_ptr,
                                   &face_mode_bytes);
    if (ret != SDK_CODE_OK) {
        DOORLOCK_ERR("Fail to read %s\n", alg_pix_info->face_model_path);
        goto exit;
    }
    ret = read_file_to_malloc_heap(alg_pix_info->face_lazy_model_path, &pix_face_lazy_model_ptr,
                                   &face_lazy_mode_bytes);
    if (ret != SDK_CODE_OK) {
        DOORLOCK_ERR("Fail to read %s\n", alg_pix_info->face_lazy_model_path);
        goto exit;
    }

    ret = read_file_to_malloc_heap(alg_pix_info->palm_model_path, &pix_palm_model_ptr,
                                   &palm_model_bytes);
    if (ret != SDK_CODE_OK) {
        DOORLOCK_ERR("Fail to read %s\n", alg_pix_info->palm_model_path);
        goto exit;
    }

    pix_qg310_product_id_e product_id = pix_qg310_product_id_161C;
    pix_set_product_id(product_id);

    pix_auth_info_t auth_info;
    // Temporary use
    //auth_info.pix_licence = alg_pix_info->auth_key;
    auth_info.pix_licence = pix_license_auth_key;
    ret = pix_init_detect_model(&auth_info, pix_detect_model_ptr, detect_model_bytes);
    if (SDK_CODE_OK != ret) {
        DOORLOCK_ERR("Fail to pix_init_detect_model with %d\n", ret);
        goto exit;
    }
    DOORLOCK_INFO("success pix_init_detect_model\n");
    // memset(pix_detect_model_ptr, 0, detect_model_bytes);

    ret = pix_init_facekit_model(&auth_info, pix_face_model_ptr, face_mode_bytes);
    if (SDK_CODE_OK != ret) {
        DOORLOCK_ERR("Fail to pix_init_facekit_model with %d\n", ret);
        goto exit;
    }
    DOORLOCK_INFO("success pix_init_facekit_model\n");

    ret =
        pix_init_facekit_model_lazy_part(&auth_info, pix_face_lazy_model_ptr, face_lazy_mode_bytes);
    if (SDK_CODE_OK != ret) {
        DOORLOCK_ERR("Fail to pix_init_facekit_model_lazy_part with %d\n", ret);
        goto exit;
    }
    DOORLOCK_INFO("success pix_init_facekit_model_lazy_part\n");

    ret = pix_init_palmkit_model(&auth_info, pix_palm_model_ptr, palm_model_bytes);
    if (SDK_CODE_OK != ret) {
        DOORLOCK_ERR("Fail to pix_init_palmkit_model with %d\n", ret);
        goto exit;
    }
    DOORLOCK_INFO("success pix_init_palmkit_model\n");

exit:
    pthread_mutex_unlock(&aw_alg_info->alg_mutex);
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int alg_pix_deinit(alg_pix_kit_t *alg_pix_info)
{
    int ret = 0;
    aw_alg_t *aw_alg_info = alg_pix_info->aw_alg_info;

    if (aw_alg_info == NULL) {
        DOORLOCK_ERR("mpAlgNnaInfo == NULL\n");
        return -1;
    }
    DOORLOCK_DBG("enter ===>\n");

    pthread_mutex_lock(&aw_alg_info->alg_mutex);
    ret = pix_release_facekit_model();
    if (ret != SDK_CODE_OK) {
        DOORLOCK_ERR("Fail to pix_release_facekit_model with %d, continue free resource\n", ret);
    }
    if (pix_face_model_ptr) {
        free(pix_face_model_ptr);
        pix_face_model_ptr = NULL;
    }

    ret = pix_release_facekit_model_lazy_part();
    if (ret != SDK_CODE_OK) {
        DOORLOCK_ERR(
            "Fail to pix_release_facekit_model_lazy_part with %d, continue free resource\n", ret);
    }
    if (pix_face_lazy_model_ptr) {
        free(pix_face_lazy_model_ptr);
        pix_face_lazy_model_ptr = NULL;
    }

    ret = pix_release_palmkit_model();
    if (ret != SDK_CODE_OK) {
        DOORLOCK_ERR("Fail to pix_release_palmkit_model with %d, continue free resource\n", ret);
    }
    if (pix_palm_model_ptr) {
        free(pix_palm_model_ptr);
        pix_palm_model_ptr = NULL;
    }

    pthread_mutex_unlock(&aw_alg_info->alg_mutex);
    DOORLOCK_DBG("exit <===\n");

    return ret;
}

int alg_pix_detect(alg_pix_kit_t *alg_pix_info, uint8_t register_flag, alg_pix_result_t *pix_result,
                   uint8_t *detect_num, uint8_t *detect_type)
{
    int ret = 0;
    aw_alg_t *aw_alg_info = alg_pix_info->aw_alg_info;

    if (aw_alg_info == NULL) {
        DOORLOCK_ERR("aw_alg_info == NULL\n");
        return -1;
    }

    pthread_mutex_lock(&aw_alg_info->alg_mutex);
    *detect_num = 0;
    uint8_t *pair_img1_ptr = alg_pix_info->yuv_vir_addrs[0];
    uint8_t *pair_img2_ptr = alg_pix_info->yuv_vir_addrs[1];
    pix_image_face_info_t pix_image_face;
    pix_pr_feature_info_t pix_pr_info;

    pix_detect_target_type_e target_type = pix_detect_target_type_undefine;
    ret = pix_commone_detect(pair_img1_ptr, &pix_image_face.coord, &target_type);
    if (ret != SDK_CODE_OK) {
        // DOORLOCK_ERR("Fail to pix_commone_detect with %d\n", ret);
        goto exit;
    }
    if (target_type == pix_detect_target_type_face) {
        ret = get_face_info(&pix_image_face, register_flag, pair_img1_ptr, pair_img2_ptr);
        if (ret != SDK_CODE_OK) {
            DOORLOCK_ERR("Fail to get person face info\n");
            goto exit;
        }
        *detect_type = FEATURE_TYPE_FACE;
        *detect_num = pix_image_face.coord.box_number;
        for (int i = 0; i < pix_image_face.coord.box_number; i++) {
            alg_pix_face_result_t *tmp_result =
                (alg_pix_face_result_t *)((uint8_t *)pix_result->face_result_list +
                                          sizeof(alg_pix_face_result_t) * i);
            memcpy(&tmp_result->fr_feature, &pix_image_face.fr_feature[i],
                   sizeof(pix_fr_feature_info_t));
            tmp_result->fr_score_threshold = pix_get_fr_threshold();
            tmp_result->fr_score = 0.0;
            tmp_result->mask_threshold = 0;
            tmp_result->mask_score = pix_image_face.mask_score[i];
            tmp_result->liveness_threshold = pix_get_liveness_threshold();
            tmp_result->liveness = pix_image_face.liveness[i].score1;
            tmp_result->distance = pix_image_face.face_attr[i].distance;
            tmp_result->left_right = pix_image_face.face_attr[i].left_right;
            tmp_result->roll = pix_image_face.face_attr[i].roll;
            tmp_result->up_down = pix_image_face.face_attr[i].up_down;
            tmp_result->x1 = aw_alg_coord_scale(
                RGB_IMG_WIDTH, pix_image_face.coord.box[i * API_PER_BOX_ELEMENTS + 0],
                alg_pix_info->yuv_widths[0]);
            tmp_result->x2 = aw_alg_coord_scale(
                RGB_IMG_WIDTH, pix_image_face.coord.box[i * API_PER_BOX_ELEMENTS + 2],
                alg_pix_info->yuv_widths[0]);
            tmp_result->y1 = aw_alg_coord_scale(
                RGB_IMG_HEIGHT, pix_image_face.coord.box[i * API_PER_BOX_ELEMENTS + 1],
                alg_pix_info->yuv_heights[0]);
            tmp_result->y2 = aw_alg_coord_scale(
                RGB_IMG_HEIGHT, pix_image_face.coord.box[i * API_PER_BOX_ELEMENTS + 3],
                alg_pix_info->yuv_heights[0]);
            DOORLOCK_INFO("tmp_result[%d], x1 = %d, x2 = %d, y1 = %d, y2 = %d\n", i, tmp_result->x1,
                          tmp_result->x2, tmp_result->y1, tmp_result->y2);
        }
    } else if (target_type == pix_detect_target_type_hand) {
        ret = get_palm_info(&pix_pr_info, register_flag, pair_img1_ptr, pair_img2_ptr);
        if (ret != SDK_CODE_OK) {
            DOORLOCK_ERR("Fail to get person hand info\n");
            goto exit;
        }
        *detect_num = 1;
        *detect_type = FEATURE_TYPE_PALM;
        alg_pix_palm_result_t *tmp_result = &(pix_result->palm_result);
        memcpy(&tmp_result->pr_feature, &pix_pr_info, sizeof(pix_pr_feature_info_t));
        tmp_result->pr_score_threshold = pix_pr_threshold();
        tmp_result->pr_score = 0.0;
    }

exit:
    pthread_mutex_unlock(&aw_alg_info->alg_mutex);
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int alg_pix_face_compare(alg_pix_kit_t *alg_pix_info, pix_fr_feature_info_t *feature0,
                         pix_fr_feature_info_t *feature1, float *fr_score)
{
    *fr_score = 0.0;
    float tmp_fr_score = 0;

    int ret = pix_cal_fea_sim(feature0, feature1, &tmp_fr_score);
    if (SDK_CODE_OK != ret) {
        DOORLOCK_ERR("Fail to pix_cal_fea_sim with %d\n", ret);
        return -1;
    }

    *fr_score = tmp_fr_score;
    if (tmp_fr_score > pix_get_fr_threshold()) {
        DOORLOCK_INFO("FaceCompare, is the same person\n");
    } else {
        DOORLOCK_INFO("FaceCompare, is diff person\n");
        return -1;
    }

    return 0;
}

int alg_pix_palm_compare(alg_pix_kit_t *alg_pix_info, pix_pr_feature_info_t *feature0,
                         pix_pr_feature_info_t *feature1, float *pr_score)
{
    *pr_score = 0.0;
    float tmp_pr_score = 0;

    int ret = pix_cal_fea_sim_palm(feature0, feature1, &tmp_pr_score);
    if (SDK_CODE_OK != ret) {
        DOORLOCK_ERR("Fail to pix_cal_fea_sim_palm with %d\n", ret);
        return -1;
    }

    DOORLOCK_INFO("pix_cal_fea_sim_palm, pr_score %f\n", tmp_pr_score);

    *pr_score = tmp_pr_score;
    if (tmp_pr_score > pix_pr_threshold()) {
        DOORLOCK_INFO("PalmCompare, is the same person\n");
    } else {
        DOORLOCK_INFO("PalmCompare, is diff person\n");
        return -1;
    }

    return 0;
}
