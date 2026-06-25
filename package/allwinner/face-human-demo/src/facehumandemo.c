
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include "awnn_det.h"

#define TEST_PDET	"pdet"
#define TEST_FDET	"fdet"

static unsigned int get_file_size(const char *name)
{
    FILE    *fp = fopen(name, "rb");
    unsigned int size = 0;

    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);

        fclose(fp);
    }
    else {
        printf("Checking file %s failed.\n", name);
    }

    return size;
}

static unsigned int load_file(const char *name, void *dst)
{
    FILE *fp = fopen(name, "rb");
    unsigned int size = 0;

    if (fp != NULL) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);

	printf("open file[%s] size[%d]\n", name, size);
        fseek(fp, 0, SEEK_SET);
        size = fread(dst, size, 1, fp);
        fclose(fp);
    }

    return size;
}

unsigned char *get_test_data(
    char *file_name,
    int *file_size
    )
{
    unsigned char *tensorData;

    *file_size = get_file_size((const char *)file_name);
    tensorData = (unsigned char *)malloc(*file_size * sizeof(unsigned char));
    load_file(file_name, (void *)tensorData);

    return tensorData;
}

int main(int argc, char* argv[])
{
    if (argc != 5) {
        printf("mode: pdet or fdet\n");
        printf("%s mode nb yuv loop\n", argv[0]);
        return -1;
    }
    unsigned char *human_input_buffers[2];
    unsigned char *face_input_buffers[2];
    BBoxResults_t *res;
    int data_len;
    unsigned char *yuvData = NULL;
    int testmode = atoi(argv[1]);
    char *human_nb = NULL;
    char *human_yuv = NULL;
    char *face_nb = NULL;
    char *face_yuv = NULL;
    if (!strcmp(argv[1], TEST_PDET)) {
        printf("test pdet!\n");
        human_nb = argv[2];
        human_yuv = argv[3];
    } else if (!strcmp(argv[1], TEST_FDET)){
        face_nb = argv[2];
        face_yuv = argv[3];
    }
    else {
	printf("input mode: pdet or fdet!\n");
	return -1;
    }
    int loop = atoi(argv[4]);
    printf("test started loop: %d.\n\n", loop);

    if (human_nb) {
        yuvData = get_test_data(human_yuv, &data_len);
        human_input_buffers[0] = yuvData;  // img y vir addr
        human_input_buffers[1] = yuvData + 352 * 352; // img vu vir addr
    } else {
        human_nb = NULL;
        printf("human nb null\n");
    }
    if (face_nb) {
        yuvData = get_test_data(face_yuv, &data_len);
        face_input_buffers[0] = yuvData;  // img y vir addr
        face_input_buffers[1] = yuvData + 480 * 480; // img vu vir addr
    } else {
        face_nb = NULL;
        printf("face nb null\n");
    }

    sleep(2);

    Awnn_Det_Handle bodyDet_handle;
    awnn_detect_init(&bodyDet_handle, human_nb, face_nb, 0, 0);

    if (bodyDet_handle) {
        for (unsigned int l = 0; l < loop; l++) {
            if (human_nb) {
                res = awnn_detect_run(bodyDet_handle, AWNN_HUMAN_DET, human_input_buffers);

                for(int i = 0; i < res->valid_cnt; i++) {
                    printf("cls %d, prob %d, rect %d, %d, %d, %d\n",res->boxes[i].label, res->boxes[i].score,
                            res->boxes[i].xmin, res->boxes[i].xmax, res->boxes[i].ymin, res->boxes[i].ymax);

                    // 1920*1080的区域坐标，4个点的x、y数组如下
                    /*int xs[4] = { 520, 385, 1382, 1680 };
                    int ys[4] = { 301, 821, 896, 250 };
                    // 简单的取人形坐标中的判断越界，注意尺寸比例不是1：1则需做坐标转化
                    int x = (res->boxes[i].xmin + res->boxes[i].xmax) / 2 * 1080 / 198;
                    int y = (res->boxes[i].ymin + res->boxes[i].ymax) / 2 * 1080 / 198;
                    int side = pnpoly(4, xs, ys, x, y); // 多边形区域内:0,多边形区域外:1
                    printf("(%d,%d) is %s the region\n", x, y, side ? "out of" : "in");

                    int x1 = 520, y1 = 301, x2 = 385, y2 = 821;
                    // 0左边范围内，1右边范围内，2左边范围外，3右边范围外
                    side = line_side(x1, y1, x2, y2, x, y);
                    printf("(%d,%d) is %s of the line\n", x, y, (side & 1 == 0) ? "left" : "right");*/
                }
            }
            if (face_nb) {
                res = awnn_detect_run(bodyDet_handle, AWNN_FACE_DET, face_input_buffers);

                for(int i = 0; i < res->valid_cnt; i++) {
                    printf("cls %d, prob %d, rect %d, %d, %d, %d\n",res->boxes[i].label, res->boxes[i].score,
                            res->boxes[i].xmin, res->boxes[i].xmax, res->boxes[i].ymin, res->boxes[i].ymax);
                    printf("landmark: (%d , %d) (%d , %d) (%d , %d) (%d , %d) (%d, %d) .\n",
                            res->boxes[i].landmark_x[0], res->boxes[i].landmark_y[0],
                            res->boxes[i].landmark_x[1], res->boxes[i].landmark_y[1],
                            res->boxes[i].landmark_x[2], res->boxes[i].landmark_y[2],
                            res->boxes[i].landmark_x[3], res->boxes[i].landmark_y[3],
                            res->boxes[i].landmark_x[4], res->boxes[i].landmark_y[4]);
                }
            }
        }
        awnn_detect_exit(bodyDet_handle);
    } else {
        printf("create body Det failed\n");
    }

    if (yuvData)
    {
	free(yuvData);
	yuvData = NULL;
    }
}
