#ifndef __AW_PERSON_DETECTION_H__
#define __AW_PERSON_DETECTION_H__

#define	PDET_MAX_NUM 20

#ifdef __cplusplus
extern "C"
{
#endif //__cplusplus

	typedef struct _aw_person_box AW_PBox;
	struct _aw_person_box {
	float tl_x;   // top_left_x
		float tl_y;   // top_left_y
		float br_x;   // bottom_right_x
		float br_y;   // bottom_right_y

		int width;    // width = br_x - tl_x
		int height;   // height = br_y - tl_y

		int area;     // area = width * height
	};

	typedef	struct _aw_detected_person AW_Person;
	struct	_aw_detected_person {
		AW_PBox bbox;
		int label;
		float score;
	};

	typedef struct _aw_person_detection_output AW_PDet_Output;
	struct _aw_person_detection_output {
		int num;
		AW_Person *person;
	};

	/*
	* A struct that stores models, setting and history for the purpose of
	* person detection.
	*
	* models:	  the handle of models.
	* setting:    the handle of runtime setting.
	* history:	  the handle of history information.
	*/
	typedef struct _aw_person_detection_container AW_PDet_Container;
	struct _aw_person_detection_container{
		void *vip_net;
		int input_count;
		int output_count;
		void *vip_inbuffer;
		void *vip_outbuffer;
		void **output;
		void *setting;
		void *history;
	};

	/*
	* A struct that stores the runtime setting.
	*
	* net_width: 	the width of network input.
	* net_height: 	the height of network input.
	* score_th:	    the score score_th of person detect.
	*/
	typedef struct _aw_person_detection_setting AW_PDet_Setting;
	struct _aw_person_detection_setting{
		int net_width;
		int net_height;
		float score_th;
	};

	/*
	* Initialization for container.
	*
	* Input:
	*   container:  the container need to be initialized.
	*	nbg_path:   the path of nb model files which input type is NV12.
	*	score_th:   the parameter of person detection score score_th.
	*	output:     the result of person detection.
	* Output:
	*	0,  initialized successfully.
	*  -1,  something wrong.
	*/
	int aw_make_person_detection_container(AW_PDet_Container *container,
		const char *nbg_path, int net_width, int net_height, float score_th, AW_PDet_Output *output);

	/*
	* Run person_detection.
	*
	* Input:
	*   container:   the initialized container.
	*   yuv_data:	 the yuv input buffer.
	*	output:      the result of person detection.
	* Output:
	*	dst_img:	 the result of person detection.
	*/
	int aw_run_person_detection(AW_PDet_Container *container,
		unsigned char **yuv_data, AW_PDet_Output *output);

	/*
	* Free memory for container.
	*/
	void aw_free_person_detection_container(AW_PDet_Container *container, AW_PDet_Output *output);

#ifdef __cplusplus
}
#endif //__cplusplus

#endif // __AW_PERSON_DETECTION_H__