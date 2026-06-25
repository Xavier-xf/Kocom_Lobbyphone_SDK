#ifndef _SAMPLE_FACE_TRACK_H_
#define _SAMPLE_FACE_TRACK_H_

#include <stdio.h>
#include <mm_comm_venc.h>
#include "mm_comm_region.h"
#include "awf_detection.h"
#include "aw_Track.h"
#include "uvc.h"

#define MAX_FILE_PATH_SIZE (256)

typedef int (*RecordCallbackFuncType)(unsigned char *addr, unsigned int len, FILE *fp);
typedef int (*UvcCallbackFuncType)(VENC_STREAM_S *pStream, VencHeaderData *pHeader);
typedef int (*NpuCallbackFuncType)(unsigned char *yuvBuffer,AWF_Det_Container *detContainer, AWF_Det_Outputs *detOutputs);
//typedef int (*NpuCallbackFuncType)(unsigned char *yuvBuffer,int width, int height, void *nbg, AWF_Det_Outputs *result);
//typedef int (*TrackCallbackFuncType)(Awnn_Result_t *input, AW_Tracker_Output *output);
typedef int (*TrackCallbackFuncType)(AWF_Det_Outputs *input, AW_Tracker_Container *trackContainer,AW_Det_Input *trackInput, AW_Tracker_Output *trackOutput);
typedef int (*StreamProcessCallbackFuncType)(unsigned char *yBuffer, unsigned char *uBuffer, unsigned char *vBuffer);
typedef int (*CreateRectCallbackFuncType)(AW_Tracker_Output *tracker_output, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, int num);
typedef int (*DestoryRectCallbackFuncType)(AW_Tracker_Output *tracker_output, int num);
typedef int (*CreateLabelCallbackFuncType)(AW_Tracker_Output *tracker_output, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, int num);
typedef int (*DestoryLabelCallbackFuncType)(AW_Tracker_Output *tracker_output, int num);

typedef struct SampleFaceTrackCmdLineParam
{
    char configFilePath[MAX_FILE_PATH_SIZE];
}SampleFaceTrackCmdLineParam;

typedef struct DetectParaConfig {
	/* Vi */
	MPP_CHN_S viChn;

	int viWidth;
	int viHeight;
	int viFps;

	/* Awnn */
	char nbg[256];
	int memSize;
	int detWidth;
	int detHeight;
} DetectParaConfig;

typedef struct StreamProcessParaConfig {
	/* Vi */
	MPP_CHN_S viChn;

	int viWidth;
	int viHeight;
	int viFps;

	/* Venc */
	MPP_CHN_S veChn;
	int venWidth;
	int venHeight;
	int venFps;
	int bitRate;
	int venType;
} StreamProcessParaConfig;

typedef struct SampleFaceTrackConfig
{
	DetectParaConfig detectConfig;
	StreamProcessParaConfig streamProcessConfig;
	UvcOutParaConfig uvcOutConfig;
}SampleFaceTrackConfig;

typedef struct DetectPrivateData {
	void *config;
	//unsigned char *buffers[2];
	unsigned char *buffer;
	NpuCallbackFuncType detectCallback;
	TrackCallbackFuncType trackCallback;

	AWF_Det_Container detContainer;
	AWF_Det_Outputs detOutputs;

	AW_Tracker_Container trackContainer;
	AW_Tracker_Output trackOutput;
	AW_Det_Input trackInput;
//	AWF_Det_Outputs outputs;
//	Awnn_Context_t *nnContext;
//	Awnn_Result_t nnResult;
//	AW_Tracker_Output trackerOutput;
	pthread_mutex_t trackerDataLock;
} DetectPrivateData;

typedef struct StreamProcessPrivateData {
	void *config;
	/* Create rect */
	CreateRectCallbackFuncType createRect;
	/* Destory rect */
	DestoryRectCallbackFuncType destoryRect;
	/* Create label */
	CreateLabelCallbackFuncType createLabel;
	/* Destory lable */
	DestoryLabelCallbackFuncType destoryLabel;

	StreamProcessCallbackFuncType streamProcessCallback;
	RecordCallbackFuncType recordCallback;
	UvcCallbackFuncType uvcCallback;
	FILE* recordFp;
//	VENC_STREAM_S *stream;
} StreamProcessPrivateData;

typedef struct SampleFaceTrackPrivateData
{
	DetectPrivateData detectData;
	StreamProcessPrivateData streamProcessData;
	UvcOutPrivateData uvcOutData;
} SampleFaceTrackPrivateData;

typedef struct SampleFaceTrackContext
{
	int enablePreview;
	int enableRecord;
	SampleFaceTrackCmdLineParam cmdLinePara;
	SampleFaceTrackConfig configPara;
	SampleFaceTrackPrivateData privateData;
} SampleFaceTrackContext;
#endif
