#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <hal_mem.h>

#include "awnn.h"

#define MODEL_HEADER_ADDR 0x42000000
#define DATA_RESERVE_ADDR 0x42300000
#define MAX_MODEL_COUNT 10
#define MAX_MODEL_NAME_LENGTH 16

typedef struct ModelInfo {
    int model_count;
    char model_name[MAX_MODEL_COUNT][MAX_MODEL_NAME_LENGTH];
    int model_size[MAX_MODEL_COUNT];
    uint32_t model_start_address[MAX_MODEL_COUNT];
} ModelInfo;

#define PDET_MODEL "npu_pdet_model"
#define PDET_DATA "npu_pdet_data"

#if 0 //3.0
// mem_size: 2829624
#define MODEL_TYPE (AWNN_DET_POST_HUMANOID_3)
#define MODEL_WITDH (384)
#define MODEL_HEIGHT (224)
#define MODEL_THRESH (0.25)
#define IMAGE_WIDTH (384)
#define IMAGE_HEIGHT (216)
#else // 1.0
// mem_size: 1947535
#define MODEL_TYPE (AWNN_DET_POST_HUMANOID_1)
#define MODEL_WITDH (320)
#define MODEL_HEIGHT (192)
#define MODEL_THRESH (0.35)
#define IMAGE_WIDTH (320)
#define IMAGE_HEIGHT (180)
#endif

int awnn_test(int argc, const char **argv)
{
    printf("======awnn_test begin\n");
    unsigned char *model_nb = (unsigned char *)DATA_RESERVE_ADDR;
    unsigned int model_len = 0;
    unsigned char *image_data = (unsigned char *)DATA_RESERVE_ADDR;
    unsigned int dataLen = 0;
    int i;
    ModelInfo info;
    memcpy(&info, (void *)MODEL_HEADER_ADDR, sizeof(ModelInfo));
    for (i = 0; i < info.model_count; i++) {
        printf("model name: %s 0x%08lx %d\n", info.model_name[i], info.model_start_address[i], info.model_size[i]);
        if (strcmp(info.model_name[i], PDET_MODEL) == 0) {
            model_nb = (unsigned char *)info.model_start_address[i];
            model_len = info.model_size[i];
        } else if (strcmp(info.model_name[i], PDET_DATA) == 0) {
            image_data = (unsigned char *)info.model_start_address[i];
            dataLen = info.model_size[i];
        }
    }

    Awnn_Post_t post;
    Awnn_Result_t result;
    post.type = MODEL_TYPE;
    post.width = IMAGE_WIDTH;
    post.height = IMAGE_HEIGHT;
    post.thresh = MODEL_THRESH;
    unsigned char *inputs[2];
    unsigned char *input_data = (unsigned char *)DATA_RESERVE_ADDR;
    memset(input_data, 0, MODEL_WITDH * MODEL_HEIGHT * 3 / 2);
    inputs[0] = input_data;
    inputs[1] = input_data + MODEL_WITDH * MODEL_HEIGHT;

    memcpy(input_data, image_data, IMAGE_WIDTH * IMAGE_HEIGHT);
    memcpy(input_data + MODEL_WITDH * MODEL_HEIGHT, image_data + IMAGE_WIDTH * IMAGE_HEIGHT, IMAGE_WIDTH * IMAGE_HEIGHT / 2);

    awnn_init(0);

    Awnn_Context_t *context = awnn_create2(model_nb, model_len);
    if (!context) {
        printf("======Failed to awnn_create\n");
        goto exit;
    }
    printf("======awnn_set_input_buffers\n");
    awnn_set_input_buffers(context, inputs);
    printf("======awnn_run\n");
    awnn_run(context);
    printf("======awnn_det_post %d\n", post.type);
    awnn_det_post(context, &post, &result);
    printf("======result: %d\n", result.valid_cnt);
    if (result.valid_cnt > 0) {
        for (int j = 0; j < result.valid_cnt; j++) {
            printf("======%d: cls %d, prob %f, rect [%d, %d, %d, %d]\n", j,
                    result.boxes[j].label, result.boxes[j].score,
                    result.boxes[j].xmin, result.boxes[j].ymin, result.boxes[j].xmax, result.boxes[j].ymax);
        }
    }
    printf("======awnn_destroy\n");
    awnn_destroy(context);

exit:
    awnn_uninit();
    printf("======awnn_test end\n");
    return 0;
}
FINSH_FUNCTION_EXPORT_ALIAS(awnn_test, awnn_test, npu awnn test cmd);
