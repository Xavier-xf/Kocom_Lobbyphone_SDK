#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#include <sys/stat.h>
#include <sys/time.h>

#include "awnn.h"
#include "list.h"

static unsigned long long GetTime()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return (unsigned long long)(time.tv_usec + time.tv_sec * 1000000LL);
}

static int nv12_draw_point(unsigned char* yBuffer, unsigned char* uvBuffer, int w, int h, int x, int y, int yColor, int uColor, int vColor) {
    if (x < 0 || x >= w) return -1;
    if (y < 0 || y >= h) return -1;
    yBuffer[y*w+x] = yColor;
    uvBuffer[(y/2)*w+x/2*2] = uColor;
    uvBuffer[(y/2)*w+x/2*2+1] = vColor;
    return 0;
}

static int nv12_draw_rect(unsigned char* yBuffer, unsigned char* uvBuffer, int w, int h, int left, int top, int right, int bottom, int yColor, int uColor, int vColor) {
    int i;
    for (i = left; i <= right; i++) {
        nv12_draw_point(yBuffer, uvBuffer, w, h, i, top, yColor, uColor, vColor);
        nv12_draw_point(yBuffer, uvBuffer, w, h, i, bottom, yColor, uColor, vColor);
    }
    for (i = top; i <= bottom; i++) {
        nv12_draw_point(yBuffer, uvBuffer, w, h, left, i, yColor, uColor, vColor);
        nv12_draw_point(yBuffer, uvBuffer, w, h, right, i, yColor, uColor, vColor);
    }
    return 0;
}

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

        fseek(fp, 0, SEEK_SET);
        size = fread(dst, size, 1, fp);

        fclose(fp);
    }

    return size;
}

static unsigned int save_file(const char *name, void *buf, unsigned int size)
{
    FILE *fp = fopen(name, "wb");

    if (fp != NULL) {
        size = fwrite(buf, size, 1, fp);

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

typedef struct {
    unsigned char **buffers;
    int width;
    int height;
    unsigned int loop;
} thread_param_t;

typedef struct {
    char path[256];
    int width;
    int height;
} input_t;

typedef struct {
    char nbg[256];
    int type;
    float thresh;
    unsigned int loop;
    int num;
    input_t *inputs;
    awnn_info_t *nbinfo;
} network_info_t;

network_info_t **create_infos_from_testcase(char *testcase, int &count) {
    list_ *text_plist = get_paths(testcase);
    char **lines = (char **)list_to_array(text_plist);
    int num = text_plist->size;

    bool create = false;
    network_info_t **infos = NULL;
    network_info_t *info = NULL;
    input_t *input;
    for (int i = 0; i < num; i++) {
        if (strlen(lines[i]) == 0 || lines[i][0] == '#')
            continue;
        if (!strncmp(lines[i], "[network]", strlen("[network]"))) {
            create = true;
            network_info_t **temp = (network_info_t **)realloc(infos, sizeof(network_info_t *) * ++count);
            if (!temp) {
                printf("network_info_t** realloc failed\n");
                break;
            } else {
                infos = temp;
            }
            continue;
        }
        if (create) {
            create = false;
            info = infos[count - 1] = (network_info_t *)malloc(sizeof(network_info_t));
            info->num = 0;
            info->inputs = NULL;
            sscanf(lines[i], "%255s %d %f %u\n", info->nbg, &info->type, &info->thresh, &info->loop);
            //printf("info: %s, %d, %f, %d\n", info->nbg, info->type, info->thresh, info->loop);
        } else {
            input_t *temp = (input_t *)realloc(info->inputs, sizeof(input_t) * ++info->num);
            if (!temp) {
                printf("input_t* realloc failed\n");
                break;
            } else {
                info->inputs = temp;
            }
            input = &info->inputs[info->num - 1];
            sscanf(lines[i], "%255s %d %d\n", input->path, &input->width, &input->height);
            //printf("input:%s, %d, %d\n", input->path, input->width, input->height);
        }
    }

    free(lines);
    lines = NULL;
    free_list_contents(text_plist);
    free_list(text_plist);
    text_plist = NULL;
    return infos;
}

void destroy_infos(network_info_t **infos, int count) {
    if (infos) {
        for (int i = 0; i < count; i++) {
            if (infos[i]) {
                for (int j = 0; j < infos[i]->num; j++) {
                    if (infos[i]->inputs) {
                        free (infos[i]->inputs);
                        infos[i]->inputs = NULL;
                    }
                }
                free(infos[i]);
                infos[i] = NULL;
            }
        }
        free(infos);
    }
}

static void* detect_thread(void *param) {
    network_info_t *info = (network_info_t *)param;
    char result_path[256];
    mkdir("post_data", 0755);
    sprintf(result_path, "post_data/result_%d.txt", (int)gettid());
	FILE *fp_result = fopen(result_path, "wt");
    if (fp_result) {
		fprintf(fp_result, "model: %s %f\n", info->nbg, info->thresh);
    } else {
        printf("Failed to open result path %s\n", result_path);
    }
    sprintf(result_path, "post_data/performance_%d.txt", (int)gettid());
	FILE *fp_perf = fopen(result_path, "wt");
    if (fp_perf) {
		fprintf(fp_perf, "model: %s\n", info->nbg);
    } else {
        printf("Failed to open result path %s\n", result_path);
    }
    unsigned long long now;

    now = GetTime();
    Awnn_Context_t *context = awnn_create(info->nbg);
    if (fp_perf) {
        fprintf(fp_perf, "awnn_create: %.2f ms.\n", (float)(GetTime() - now)/1000);
    }

    if (!context) {
        printf("Failed to awnn_create\n");
        if (fp_result) {
            fclose(fp_result);
        }
        if (fp_perf) {
            fclose(fp_perf);
        }
        return NULL;
    }
    unsigned char *input_buffers[2] = {};
    printf("malloc %dx%d nv12 buffer\n", info->nbinfo->width, info->nbinfo->height);
    input_buffers[0] = (unsigned char *)malloc(info->nbinfo->width * info->nbinfo->height);  // img y vir addr
    input_buffers[1] = (unsigned char *)malloc(info->nbinfo->width * info->nbinfo->height / 2); // img vu vir addr
    int data_len;
    unsigned char *data = NULL;
    Awnn_Post_t post;
    post.type = (AWNN_DET_POST_TYPE) info->type;
    Awnn_Result_t result;
    for (unsigned int l = 0; l < info->loop ; l++) {
        for (int i = 0; i < info->num; i++) {
            if (info->nbinfo->width < info->inputs[i].width || info->nbinfo->height < info->inputs[i].height) {
                printf("%s not support input size %dx%d\n", info->nbg, info->inputs[i].width, info->inputs[i].height);
                continue;
            }
            data = get_test_data(info->inputs[i].path, &data_len);
            if (!data) {
                printf("load data %s failed\n", info->inputs[i].path);
                continue;
            }
            memset(input_buffers[0], 0, info->nbinfo->width * info->nbinfo->height);
            memset(input_buffers[1], 0, info->nbinfo->width * info->nbinfo->height / 2);
            memcpy(input_buffers[0], data, info->inputs[i].width * info->inputs[i].height);
            memcpy(input_buffers[1], data + info->inputs[i].width * info->inputs[i].height, info->inputs[i].width * info->inputs[i].height / 2);
            awnn_set_input_buffers(context, input_buffers);
            if (l == 0) {
                now = GetTime();
            }
            awnn_run(context);
            if (l == 0 && fp_perf) {
                fprintf(fp_perf, "awnn_run %s: %.2f ms.\n", info->inputs[i].path, (float)(GetTime() - now)/1000);
            }
            char post_file[255];
            sprintf(post_file, "post_%s", info->inputs[i].path);
            char cmd[255];
            sprintf(cmd, "mkdir -p $(dirname %s)\n", post_file);
            //printf(cmd);
            system(cmd);
            if (l == 0) {
                awnn_dump_io(context, post_file);
            }
            post.width = info->inputs[i].width;
            post.height = info->inputs[i].height;
            post.thresh = info->thresh;
            if (l == 0) {
                now = GetTime();
            }
            awnn_det_post(context, &post, &result);
            if (l == 0 && fp_perf) {
                fprintf(fp_perf, "awnn_det_post: %.2f ms.\n", (float)(GetTime() - now)/1000);
            }
            if (l == 0) {
                if (fp_result) {
                    fprintf(fp_result, "%s result: %d\n", info->inputs[i].path, result.valid_cnt);
                }
                if (result.valid_cnt > 0) {
                    for (int j = 0; j < result.valid_cnt; j++) {
                        if (fp_result) {
                            fprintf(fp_result, "%d: cls %d, prob %f, rect [%d, %d, %d, %d]", j,
                                    result.boxes[j].label, result.boxes[j].score,
                                    result.boxes[j].xmin, result.boxes[j].ymin, result.boxes[j].xmax, result.boxes[j].ymax);
                            if (info->type == 4) {
                                fprintf(fp_result, ", landmark: (%d, %d) (%d, %d) (%d, %d) (%d, %d) (%d, %d)\n",
                                        result.boxes[j].landmark_x[0], result.boxes[j].landmark_y[0],
                                        result.boxes[j].landmark_x[1], result.boxes[j].landmark_y[1],
                                        result.boxes[j].landmark_x[2], result.boxes[j].landmark_y[2],
                                        result.boxes[j].landmark_x[3], result.boxes[j].landmark_y[3],
                                        result.boxes[j].landmark_x[4], result.boxes[j].landmark_y[4]);
                            } else {
                                fprintf(fp_result, "\n");
                            }
                        }
                        if (result.boxes[j].label != 0) continue;
                        nv12_draw_rect(data, data + info->inputs[i].width * info->inputs[i].height, info->inputs[i].width, info->inputs[i].height,
                                result.boxes[j].xmin, result.boxes[j].ymin, result.boxes[j].xmax, result.boxes[j].ymax, 0x96, 0x2C, 0x15);
                        if (info->type == 4) {
                            for (int k = 0; k < 5; k++)
                                nv12_draw_point(data, data + info->inputs[i].width * info->inputs[i].height, info->inputs[i].width, info->inputs[i].height,
                                        result.boxes[j].landmark_x[k], result.boxes[j].landmark_y[k], 0x96, 0x2C, 0x15);
                        }
                    }
                }
                printf("save file: %s\n", post_file);
                save_file(post_file, data, info->inputs[i].width * info->inputs[i].height * 3 / 2);
            }
            free(data);
        }
    }
    free(input_buffers[0]);
    free(input_buffers[1]);
    now = GetTime();
    awnn_destroy(context);
    if (fp_perf) {
        fprintf(fp_perf, "awnn_destroy: %.2f ms.\n", (float)(GetTime() - now)/1000);
    }

    if (fp_result) {
        fclose(fp_result);
    }
    if (fp_perf) {
        fclose(fp_perf);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("%s testcase.txt\n", argv[0]);
        return 0;
    }
    int count = 0;
    network_info_t **infos = create_infos_from_testcase(argv[1], count);
    printf("count=%d\n", count);

    if (count && infos) {
        awnn_info_t * nbinfo;
        unsigned int mem_size = 0;
        pthread_t thread[count] = {0};

        for (int i = 0; i < count; i++) {
            nbinfo = awnn_get_info(infos[i]->nbg);
            printf("info %s %s %u %u %u %f\n", nbinfo->name, nbinfo->md5,
                    nbinfo->width, nbinfo->height, nbinfo->mem_size, nbinfo->thresh);
            infos[i]->nbinfo = nbinfo;
            mem_size += nbinfo->mem_size;
        }

        printf("mem_size: %u\n", mem_size);
        awnn_init(mem_size);

        for (int i = 0; i < count; i++) {
            pthread_create(&thread[i], NULL, detect_thread, infos[i]);
        }

        for (int i = 0; i < count; i++) {
            if (thread[i] != 0)
                pthread_join(thread[i], NULL);
        }
        awnn_uninit();
    }

    destroy_infos(infos, count);
    return 0;
}