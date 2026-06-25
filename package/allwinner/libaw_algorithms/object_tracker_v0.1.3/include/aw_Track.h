#ifndef AW_TRACK_H
#define AW_TRACK_H

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#include "aw_image.h"

#define MAX_NUM 500

	/*
	* A struct that store the parameters for the purpose of object tracker:
	*/
	typedef struct _aw_tracker_container AW_Tracker_Container;
	struct _aw_tracker_container {
		void* models;
		void* settings;
		void* history;
	};

	/*
	* A struct that stores the setting parameter.
	* frame_rate: Frame rate at which tracking is performed.
	* max_loss_frames: Maximum number of consecutive frames an object can be lost before being considered terminated.
	*/
	typedef struct _aw_tracker_settings AW_Tracker_Settings;
	struct _aw_tracker_settings
	{
		int frame_rate;
		float max_loss_frames;
	};

	/*
	* A struct that represents basic information about a detecting/tracking object.
	* label: ClassID of the object.
	* track_id: Tracking object's ID.
	* confidence: The confidence level of the current frame detection box.
	* box: Bounding box enclosing the object in the image.
	*/
	typedef struct _aw_box_base_information AW_Box_Base;
	struct  _aw_box_base_information {
		int label;
		int track_id;
		float confidence;
		AW_Box box;
	};

	//Detection Input
	typedef struct _aw_detection_input AW_Det_Input;
	struct _aw_detection_input {
		int num;
		AW_Box_Base* box_info;
	};

	//Tracker Output
	typedef struct _aw_tracker_output AW_Tracker_Output;
	struct _aw_tracker_output
	{
		int num;
		AW_Box_Base* box_info;
	};

	/*
	* Initializes for container.
	*
	* Input:
	*   container - The container need to be initialized.
	*   frame_rate: Frame rate at which tracking is performed.
	*   max_loss_frames: Maximum number of consecutive frames an object can be lost before being considered terminated.
	*   output - Pointer to the output structure to be prepared.
	*
	* Output:
	*   0 - Initialization successful.
	*  -1 - An error occurred during initialization.
	*/
	int aw_make_tracker(AW_Tracker_Container* container, int frame_rate, int max_loss_frames, AW_Tracker_Output* output);

	/*
	* Run tracker.
	*
	* Input:
	*   container: The initialized tracker container.
	*   input: The output result of the detector.
	*   output: Output results of the tracker.
	*/
	void aw_run_tracker(AW_Tracker_Container* container, AW_Det_Input* input, AW_Tracker_Output* output);

	/*
	* Free memory for container.
	*/
	int aw_free_tracker(AW_Tracker_Container* container);

#ifdef __cplusplus
};
#endif// __cplusplus

#endif
