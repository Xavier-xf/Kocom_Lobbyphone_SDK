#ifndef __V4L2_DEMO_H__
#define __V4L2_DEMO_H__

#include <stdbool.h>
#include <linux/videodev2.h>

struct video_plane
{
	unsigned int size;
	int dma_fd;
	void *mem;
	unsigned int mem_phy;
};

struct video_buffer
{
	unsigned int index;
	unsigned int bytesused;
	unsigned int frame_cnt;
	unsigned int exp_time;
	struct timeval timestamp;
	bool error;
	bool allocated;
	unsigned int nplanes;
	struct video_plane *planes;
};

struct buffer_pool
{
	unsigned int nbufs;
	struct video_buffer *buffers;
};

typedef struct v4l2DemoContext
{
	int mDevId;
    int mType;
	struct v4l2_format mFmt;
	struct v4l2_input mInp;
	struct v4l2_streamparm mParam;
    struct v4l2_requestbuffers mReqBuf;
	struct buffer_pool *mpBufferPool;
}v4l2DemoContext;

#endif
