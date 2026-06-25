#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <sys/time.h>
#include <algorithm>
#include <iostream>

#include "aw_image.h"
#include "awf_detection.h"

#define  DBL_MAX   100000000
#define IMG_W 640
#define IMG_H 360

using namespace std;

double getCurrentTime()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int main(int argc, char* argv[])
{
    char *det_model_filename = argv[1];
    char *image_path = argv[2];

    int dst_w = IMG_W;
    int dst_h = IMG_H;
    int buffer_sz = dst_w * dst_h * 1.5;

    unsigned char *yuv_buffer = (unsigned char *)calloc(buffer_sz, sizeof(unsigned char));
    if(!yuv_buffer)
    {
	printf("yuv image buffer calloc failed!\n");
    }


    FILE *fp = fopen(image_path, "rb");
    if(fp == nullptr){
	printf("File to open yuv image file\n");
    }
    fread(yuv_buffer, sizeof(unsigned char), buffer_sz, fp);
    fclose(fp);


    double timeMin = DBL_MAX;
	double timeMax = -DBL_MAX;
	double timeAvg = 0;
	int loop = 1;
    for(int i =0; i < loop; i++)
    {
        double start = getCurrentTime();
		AWF_Det_Container* container = (AWF_Det_Container*)malloc(sizeof(AWF_Det_Container));
		if (!container)
		{
			printf("fail to allocate memory for [det_container]\n");
		}
		AWF_Det_Outputs* outputs = (AWF_Det_Outputs*)malloc(sizeof(AWF_Det_Outputs));
		if (!outputs) {
			printf("fail to allocate memory for [det_outputs]\n");
		}
		int flag = awf_make_det_container(container, det_model_filename, outputs);
		if (flag == -1) {
			printf("fail to make face detction container\n");
		}

		float confid_ths = 0.6;
		float nms_ths = 0.45;
		int flag_2 = awf_set_det_runtime(container, confid_ths, nms_ths, dst_w, dst_h);
		if (flag_2 == -1) {
			printf("fail to set det runtime\n");
		}
        awf_run_det(container, yuv_buffer, outputs);
	    double end = getCurrentTime();
		double time = end - start;
		timeMin = std::min(timeMin, time);
		timeMax = std::max(timeMax, time);
		timeAvg += time;
		// printf("det num: %d", outputs->num);
	    for (int j = 0; j < outputs->num; j++)
		{
			// float scale_w = rgb_img.w * 1.0 / resized_img.w;
			// float scale_h = rgb_img.h * 1.0 / resized_img.h;
			// float x1 = float((outputs->faces[j].bbox.tl_x) * scale_w);
			// float y1 = float((outputs->faces[j].bbox.tl_y) * scale_h);
			// float x2 = float((outputs->faces[j].bbox.br_x) * scale_w);
			// float y2 = float((outputs->faces[j].bbox.br_y) * scale_h);
			float x1 = float((outputs->faces[j].bbox.tl_x));
			float y1 = float((outputs->faces[j].bbox.tl_y));
			float x2 = float((outputs->faces[j].bbox.br_x));
			float y2 = float((outputs->faces[j].bbox.br_y));
			float s = float(outputs->faces[j].confid_score);
			printf("rect: %f %f %f %f %f.\n", x1, y1, x2, y2, s);

			for (int k = 0; k < NUM_KEYPOINTS; k++)
			{
				// short landms_x = short((outputs->faces[j].ldmk.cx[k]) * scale_w);
				// short landms_y = short((outputs->faces[j].ldmk.cy[k]) * scale_h);
				short landms_x = short((outputs->faces[j].ldmk.cx[k]));
				short landms_y = short((outputs->faces[j].ldmk.cy[k]));
				printf("ldmk: %hd, %hd.\n", landms_x, landms_y);
			}
		}
		awf_free_det_container(container, outputs);

		free(yuv_buffer);

    }
	timeAvg /= loop;
    fprintf(stderr, "The  facedet costs :  min = %7.2f  max = %7.2f  avg = %7.2f\n", timeMin, timeMax, timeAvg);

	return 0;
}
