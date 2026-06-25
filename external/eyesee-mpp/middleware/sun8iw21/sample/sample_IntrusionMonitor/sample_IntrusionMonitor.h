#ifndef _SAMPLE_FACE_TRACK_H_
#define _SAMPLE_FACE_TRACK_H_

#include <stdio.h>
#include <mm_comm_venc.h>
#include "mm_comm_region.h"
#include "aw_person_detection.h"
#include "uvc.h"

#define MAX_FILE_PATH_SIZE (256)

typedef int (*RecordCallbackFuncType)(unsigned char *addr, unsigned int len, FILE *fp);
typedef int (*UvcCallbackFuncType)(VENC_STREAM_S *pStream, VencHeaderData *pHeader);
typedef int (*NpuCallbackFuncType)(unsigned char **yuvBuffer,AW_PDet_Container *detContainer, AW_PDet_Output *detOutputs);
typedef int (*StreamProcessCallbackFuncType)(unsigned char *yBuffer, unsigned char *uBuffer, unsigned char *vBuffer);
typedef int (*CreateRectCallbackFuncType)(AW_PDet_Output *detOutputs, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, int num);
typedef int (*CreateMonitorRegionCallbackFuncType)(void *region, int width, int height, unsigned char* yBuffer, unsigned char* uBuffer, unsigned char* vBuffer);

typedef int (*DestoryRectCallbackFuncType)(AW_PDet_Output *detOutputs, int num);
typedef int (*CreateLabelCallbackFuncType)(AW_PDet_Output *detOutputs, RGN_ATTR_S *pStRegion, RGN_CHN_ATTR_S *pStRgnChnAttr, int num);
typedef int (*DestoryLabelCallbackFuncType)(AW_PDet_Output *detOutputs, int num);

typedef struct SampleIntrusionMonitorCmdLineParam
{
    char configFilePath[MAX_FILE_PATH_SIZE];
}SampleIntrusionMonitorCmdLineParam;

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

//#define REGIONS_MAX_NUM             (4)  /*自定义多分区侦测最大区域个数*/
//#define MAX_VE_MOTION_NUM           (50)  /*VE侦测最大返回区域个数*/

#define CROSS_LINE_MAX_NUM          (1)  /*虚拟线最大条数*/
#define CROSS_RECT_MAX_NUM          (1)  /*区域入侵最大区域个数*/
#define CROSS_RECT_MAX_POINT_NUM    (4)  /*入侵区域最大顶点个数*/

// 区域检测模式
typedef enum {
	CROSS_WORK_MODE_NONE,
	CROSS_WORK_MODE_LINE,
	CROSS_WORK_MODE_RECT,
	CROSS_WORK_MODE_HUMANOID
} WorkMode;

typedef struct {
	int x0;
	int y0;
	int x1;
	int y1;
//	int direction; // 触发方向 0：双向，1：左->右，2：右->左
} MonitorLine;

typedef struct {
	int x[CROSS_RECT_MAX_POINT_NUM];
	int y[CROSS_RECT_MAX_POINT_NUM];
//	int direction;      // 触发方向 0：双向，1：进入，2：离开
} MonitorRect;

typedef struct {
	WorkMode mode;
	int enable;                            // 使能
	int sensitivity;                        // 灵敏度（0~100）
	int outdoor;                            // 模式 0：室外，1：室内
	int num;                                // 区域数量
	union {
		MonitorLine lines[CROSS_LINE_MAX_NUM];   // 界线参数
		MonitorRect rects[CROSS_RECT_MAX_NUM];   // 区域参数
	} settings;
} MonitorRegion;

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

	MonitorRegion region;
} StreamProcessParaConfig;

typedef struct SampleIntrusionMonitorConfig
{
	DetectParaConfig detectConfig;
	StreamProcessParaConfig streamProcessConfig;
	UvcOutParaConfig uvcOutConfig;
}SampleIntrusionMonitorConfig;

typedef struct DetectPrivateData {
	void *config;
	void *context;
//	unsigned char *buffers[2];
	unsigned char *buffer;
	NpuCallbackFuncType detectCallback;
	//TrackCallbackFuncType trackCallback;

	//AWF_Det_Container detContainer;
//	AWF_Det_Outputs detOutputs;

	AW_PDet_Container detContainer;
	AW_PDet_Output detOutputs;


	AW_PDet_Output detOutputsNorm;
	//AWF_Det_Outputs detOutputsNorm;
	pthread_mutex_t detectDataLock;

//	AW_Tracker_Output trackerOutput;
//	pthread_mutex_t trackerDataLock;
	//AWF_Det_Outputs detOutputsNorm;
} DetectPrivateData;


typedef struct StreamProcessPrivateData {
	void *config;
	void *context;

	MonitorRegion regionNorm;
//	void *region;
	/* Create monitor region */
	CreateMonitorRegionCallbackFuncType createMonitorRegion;

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

typedef struct SampleIntrusionMonitorPrivateData
{
	DetectPrivateData detectData;
	StreamProcessPrivateData streamProcessData;
	UvcOutPrivateData uvcOutData;
} SampleIntrusionMonitorPrivateData;

typedef struct SampleIntrusionMonitorContext
{
	int enablePreview;
	int enableRecord;
	SampleIntrusionMonitorCmdLineParam cmdLinePara;
	SampleIntrusionMonitorConfig configPara;
	SampleIntrusionMonitorPrivateData privateData;
} SampleIntrusionMonitorContext;
#endif
