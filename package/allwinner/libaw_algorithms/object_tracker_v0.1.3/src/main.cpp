#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include "aw_Track.h"

int main(int argc, char* argv[])
{
	// initialize
	AW_Tracker_Container* container = (AW_Tracker_Container*)calloc(1, sizeof(AW_Tracker_Container));
	if (!container)
	{
		printf("fail to allocate memory for [container]\n");
	}

	AW_Tracker_Output* tracker_output = (AW_Tracker_Output*)calloc(1, sizeof(AW_Tracker_Output));
	if (!tracker_output)
	{
		printf("fail to allocate memory for [output]\n");
	}

	int frame_rate = 20;
	int max_loss_frames = 20;
	int flag = aw_make_tracker(container, frame_rate, max_loss_frames, tracker_output);
	if (flag == -1)
	{
		printf("fail to make tracker container\n");
	}

	// create testing box
	AW_Box box1;
	box1.tl_x = 10;
	box1.tl_y = 20;
	box1.br_x = 80;
	box1.br_y = 100;
	box1.width = box1.br_x - box1.tl_x;
	box1.height = box1.br_y - box1.tl_y;

	AW_Box box2;
	box2.tl_x = 150;
	box2.tl_y = 350;
	box2.br_x = 290;
	box2.br_y = 570;
	box2.width = box2.br_x - box2.tl_x;
	box2.height = box2.br_y - box2.tl_y;

	AW_Box box3;
	box3.tl_x = 1;
	box3.tl_y = 1;
	box3.br_x = 2;
	box3.br_y = 4;
	box3.width = box3.br_x - box3.tl_x;
	box3.height = box3.br_y - box3.tl_y;

	AW_Det_Input* det_input = (AW_Det_Input*)calloc(1, sizeof(AW_Det_Input));
	if (!det_input)
	{
		printf("fail to allocate memory for [det_input]");
	}

	det_input->num = 3;
	det_input->box_info = (AW_Box_Base*)calloc(det_input->num, sizeof(AW_Box_Base));

	det_input->box_info[0].label = 0;
	det_input->box_info[0].confidence = 0.8f;
	det_input->box_info[0].box = box1;

	det_input->box_info[1].label = 0;
	det_input->box_info[1].confidence = 0.9f;
	det_input->box_info[1].box = box2;

	det_input->box_info[2].label = 0;
	det_input->box_info[2].confidence = 0.9f;
	det_input->box_info[2].box = box3;

	clock_t start_track = clock();
	aw_run_tracker(container, det_input, tracker_output);
	clock_t end_track = clock();
	double elapsed_time_ms = static_cast<double>(end_track - start_track) / CLOCKS_PER_SEC * 1000.0;

	for (size_t j = 0; j < tracker_output->num; j++)
	{
		std::cout << "Track ID" << tracker_output->box_info[j].track_id << ", classID :"<< tracker_output->box_info[j].label << ", Bounding Box : (" << tracker_output->box_info[j].box.tl_x << "," << tracker_output->box_info[j].box.tl_y << "," << tracker_output->box_info[j].box.width << "," << tracker_output->box_info[j].box.height << ")" << std::endl;

	}
	std::cout << "tracking time: " << elapsed_time_ms << "ms" << std::endl;

	aw_free_tracker(container);
	free(container);
	container = NULL;
	free(tracker_output);
	tracker_output = NULL;
	free(det_input);
	det_input = NULL;
}
