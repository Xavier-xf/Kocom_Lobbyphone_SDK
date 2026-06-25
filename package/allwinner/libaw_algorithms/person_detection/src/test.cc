#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/time.h>
#include <iostream>
#include <fstream>

#include "vip_lite.h"
#include "aw_person_detection.h"

using namespace std;

/* load yuv data */
static unsigned int get_file_size(const char *name)
{
    FILE *fp = fopen(name, "rb");
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

unsigned char *get_test_data(
    const char *file_name,
    int *file_size
    )
{
    unsigned char *tensorData;

    *file_size = get_file_size(file_name);
    tensorData = (unsigned char *)malloc(*file_size * sizeof(unsigned char));
    load_file(file_name, (void *)tensorData);

    return tensorData;
}

int main(int argc, char* argv[])
{
	char *nbg_path = argv[1];
	char *img_name = argv[2];
	float threshold = atof(argv[3]);
	int loop  = atoi(argv[4]);

	printf("Load image:\n");

	/* input type: yuv data: NV12 */
	std::string img_path(img_name);
	printf("%s\n", img_path.c_str());

	int img_w = 384;
	int img_h = 216;
	int nbinput_w = 384;
	int nbinput_h = 224;

	unsigned char *yuv_buffers[2] = {};
	yuv_buffers[0] = (unsigned char *)malloc(nbinput_w * nbinput_h);  // img y vir addr
	yuv_buffers[1] = (unsigned char *)malloc(nbinput_w * nbinput_h / 2); // img vu vir addr
	int data_len;
	unsigned char *data = NULL;
	data = get_test_data(img_path.c_str(), &data_len);
	if (!data) {
		printf("load data %s failed\n", img_path.c_str());
	}

	memset(yuv_buffers[0], 0, nbinput_w * nbinput_h);
	memset(yuv_buffers[1], 0, nbinput_w * nbinput_h / 2);
	memcpy(yuv_buffers[0], data, img_w * img_h);
	memcpy(yuv_buffers[1], data + img_w * img_h, img_w * img_h / 2);

	/* start person detection. */
	int status = 0;
	AW_PDet_Container *container = (AW_PDet_Container *)calloc(1, sizeof(AW_PDet_Container));
	AW_PDet_Output *output = (AW_PDet_Output *)calloc(1, sizeof(AW_PDet_Output));

	if (!container) {
		printf("Failed to allocate memory for containers.\n");
		return -1;
	}
	if (!output) {
		printf("Failed to allocate memory for output.\n");
		return -1;
	}

	status = aw_make_person_detection_container(container, nbg_path, nbinput_w, nbinput_h, threshold, output);
	if (status) {
		printf("failed to aw_make_person_detect_container!\n");
		return -1;
	}

	printf("Run ...\n");

	for (int i=0; i<loop; i++){
		aw_run_person_detection(container, yuv_buffers, output);
		if (status) {
			printf("failed to aw_run_person_detect!\n");
			return -1;
		}
	}

	if (output->num > 0) {
		for (int j = 0; j < output->num; j++) {
			if (output->person[j].label == 1) {
				printf("Detect person id %d: cls %d, prob %f, rect [%f, %f, %f, %f]\n", j,
						output->person[j].label, output->person[j].score,
						output->person[j].bbox.tl_x * img_w, output->person[j].bbox.tl_y * img_h, output->person[j].bbox.br_x * img_w, output->person[j].bbox.br_y * img_h);
			}

		}
	}
	else{
		printf("%s No humanoid is detected in the curent frame.\n");
	}


	printf("Free memory ..\n");
	aw_free_person_detection_container(container, output);
	free(container);
	free(output);
	free(yuv_buffers[0]);
	free(yuv_buffers[1]);
	printf("Finished!\n");
}
