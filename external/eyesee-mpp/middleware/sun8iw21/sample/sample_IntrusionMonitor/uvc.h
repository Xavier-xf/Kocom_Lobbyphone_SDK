#ifndef __SAMPLE_FACE_TRACK_UVC_H
#define __SAMPLE_FACE_TRACK_UVC_H

#include <pthread.h>
#include <cdx_list.h>
#include "linux/videodev2.h"
#include <linux/usb/ch9.h>
#include "video.h"
#include "parser_uvc_configfs.h"


#define UVC_EVENT_FIRST			(V4L2_EVENT_PRIVATE_START + 0)
#define UVC_EVENT_CONNECT		(V4L2_EVENT_PRIVATE_START + 0)
#define UVC_EVENT_DISCONNECT		(V4L2_EVENT_PRIVATE_START + 1)
#define UVC_EVENT_STREAMON		(V4L2_EVENT_PRIVATE_START + 2)
#define UVC_EVENT_STREAMOFF		(V4L2_EVENT_PRIVATE_START + 3)
#define UVC_EVENT_SETUP			(V4L2_EVENT_PRIVATE_START + 4)
#define UVC_EVENT_DATA			(V4L2_EVENT_PRIVATE_START + 5)
#define UVC_EVENT_LAST			(V4L2_EVENT_PRIVATE_START + 5)

#define UVCIOC_SEND_RESPONSE		_IOW('U', 1, struct uvc_request_data)

#define UVC_INTF_CONTROL		0
#define UVC_INTF_STREAMING		1



#define UVC_FORMAT_MJPEG_SUPPORT    (1)
#define UVC_FORMAT_H264_SUPPORT     (1 << 1)
#define UVC_FORMAT_YUYV_SUPPORT     (1 << 2)

typedef struct SampleUVCFrame {
	void *pVirAddr;
	void *pPhyAddr;
	unsigned int iBufLen;
} SampleUVCFrame;

struct uvc_request_data
{
	__s32 length;
	__u8 data[60];
};

struct uvc_event
{
	union {
		enum usb_device_speed speed;
		struct usb_ctrlrequest req;
		struct uvc_request_data data;
	};
};

typedef struct SampleUVCOutBuf {
    unsigned char *pcData;
    unsigned int iDataBufSize;
    unsigned int iDataSize0;
    unsigned int iDataSize1;
    unsigned int iDataSize2;
    struct list_head mList;
} SampleUVCOutBuf;

typedef struct SampleUVCDevice {
    int devId;
    int enable_bulk_mode;
    int is_streaming_flag;
    struct uvc_streaming_control uvc_streaming_probe;
    struct uvc_streaming_control uvc_streaming_commit;
    int control;

    SampleUVCFrame *frame_buffer;
    int frame_buffer_num;
    int format;

    struct uvc_frame *frame;
    struct list_head frame_idle_list; /* SampleUVCOutBuf */
    struct list_head frame_valid_list;
    struct list_head frame_used_list;
    pthread_mutex_t frame_list_lock;
} SampleUVCDevice;

typedef struct SampleUVCConfig
{
    int uvc_enable;
    int uvc_bulk_mode;
    int uac_enable;
}SampleUVCConfig;

typedef struct UvcOutPrivateData {
	void *config;
	void *context;
	SampleUVCDevice uvcDevice;
	struct uvc_configfs_para *configfs_para;

}UvcOutPrivateData;

typedef struct  UvcOutParaConfig {
	int uvcDev;
	SampleUVCConfig config;
} UvcOutParaConfig;

void *sampleIntrusionMonitorUvcOutTask(void *para);
#endif
