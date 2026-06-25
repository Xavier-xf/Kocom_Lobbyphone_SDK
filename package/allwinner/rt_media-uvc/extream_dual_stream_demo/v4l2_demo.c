#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <time.h>
#include <stdint.h>

#include "v4l2_demo.h"
#include "log.h"

#define JPEG_SAVE_PATH		"/mnt/extsd/jpeg"

#define MJPEG_APP0_MARKER   0xFFE0
#define MJPEG_APP7_MARKER   0xFFE7
#define MJPEG_APP8_MARKER   0xFFE8
#define MJPEG_APP9_MARKER   0xFFE9

#define MJPEG_SOI_MARKER        0xFFD8
#define MJPEG_EOI_MARKER        0xFFD9

#define EXTRACT_DUAL_STREAM
#ifdef EXTRACT_DUAL_STREAM
#define SAVE_DUAL_STREAM_FILE	"/mnt/extsd/dual_stream.raw"
struct dual_stream_info
{
    int off0;
    int off1;
    int off2;
    int len0;
    int len1;
    int len2;
};
#endif
#define DEBUG_SAVE_JPEG
//#define CHECK_JPEG_EOI

static int g_exit_flag = 0;
void signal_handler(int signo)
{
	g_exit_flag = 1;
}

#ifdef EXTRACT_DUAL_STREAM
int extract_dual_stram(uint8_t *jpeg_data, int jpeg_data_len, struct dual_stream_info *dual_stream)
{
	uint8_t *data = jpeg_data;
	int offset = 0;
	uint16_t marker_id = 0;

	while (1)
	{
		if (offset >= jpeg_data_len)
			break;

		marker_id = data[offset] << 8 | data[offset + 1];
		logd("0x%x %d", marker_id, offset);
		if (marker_id == 0xFFD8)
		logd("SOI");
		if (marker_id == 0xFFE0)
		{
			logd("APP0");
			offset += 2;
			int16_t app0_len = data[offset] << 8 | data[offset + 1];
			offset += app0_len;
			continue;
		}
		if (marker_id == 0xFFE7)
		{
			logd("APP7");
			offset += 2;
			uint16_t app7_len = data[offset] << 8 | data[offset + 1];
			logd("app7 len 0x%x", app7_len);
			offset += 2;
			uint8_t video_info = data[offset];
			logd("video0 info 0x%x", video_info);
			offset += 1;
			uint8_t video_fps = data[offset];
			logd("video0 fps 0x%x", video_fps);
			offset += 7;
			uint16_t video_len = data[offset] << 8 | data[offset + 1];
			logd("video0 len 0x%x", video_len);
			offset += 2;
			dual_stream->off0 = offset;
			dual_stream->len0 = video_len - 2;
			offset += dual_stream->len0;
			continue;
		}
		if (marker_id == 0xFFE8)
		{
			logd("APP8");
			offset += 2;
			uint16_t app7_len = data[offset] << 8 | data[offset + 1];
			logd("app7 len 0x%x", app7_len);
			offset += 2;
			uint8_t video_info = data[offset];
			logd("video1 info 0x%x", video_info);
			offset += 1;
			uint8_t video_fps = data[offset];
			logd("video1 fps 0x%x", video_fps);
			offset += 7;
			uint16_t video_len = data[offset] << 8 | data[offset + 1];
			logd("video1 len 0x%x", video_len);
			offset += 2;
			dual_stream->off1 = offset;
			dual_stream->len1 = video_len - 2;
			offset += dual_stream->len0;
			continue;
		}
		if (marker_id == 0xFFE9)
		{
			logd("APP8");
			offset += 2;
			uint16_t app7_len = data[offset] << 8 | data[offset + 1];
			logd("app7 len 0x%x", app7_len);
			offset += 2;
			uint8_t video_info = data[offset];
			logd("video2 info 0x%x", video_info);
			offset += 1;
			uint8_t video_fps = data[offset];
			logd("video2 fps 0x%x", video_fps);
			offset += 7;
			uint16_t video_len = data[offset] << 8 | data[offset + 1];
			logd("video2 len 0x%x", video_len);
			offset += 2;
			dual_stream->off2 = offset;
			dual_stream->len2 = video_len - 2;
			offset += dual_stream->len2;
			continue;
		}
		if (marker_id == 0xFFDB)
		{
			logv("DQT");
			break;
		}
		offset++;
	}
	return 0;
}

#endif

#ifdef CHECK_JPEG_EOI
static int checkJPEGEOI(uint8_t *jpeg, int jpeg_len)
{
	int offset = jpeg_len - 3;

	if ((jpeg[offset+1] == 0xff) && (jpeg[offset+2] == 0xd9))
		return 0;
	return -1;
	while (1)
	{
		if (offset <= (jpeg_len / 3 * 2))
			return -1;
		if ((jpeg[offset+1] == 0xff) && (jpeg[offset+2] == 0xd9))
			return 0;
		logd("0x%02x 0x%02x", jpeg[offset+1], jpeg[offset+2])
		offset -= 2;
	}
	return 0;
}
#endif

int main(int argc, char *argv[])
{
	int ret = 0;
	int nBufNum = 3;
	int nPlaneNum = 0;
	v4l2DemoContext *pContext = NULL;

	system("rm /mnt/extsd/jpeg/*");
	if (argc < 2)
	{
		loge("example: ./v4l2_demo /dev/video0!");
		goto _exit;
	}

	signal(SIGINT, signal_handler);

	pContext = (v4l2DemoContext *)malloc(sizeof(v4l2DemoContext));
	if (NULL == pContext)
	{
		loge("fatal error! malloc v4l2DemoContext fail!");
		goto _exit;
	}
	memset(pContext, 0, sizeof(v4l2DemoContext));

	pContext->mDevId = -1;
	pContext->mDevId = open(argv[1], O_RDWR | O_NONBLOCK | O_CLOEXEC, 0);
	if (-1 == pContext->mDevId)
	{
		loge("fatal error! open dev %s fail!", argv[1]);
		goto _free_context;
	}

	struct v4l2_capability cap;
	memset(&cap, 0, sizeof(cap));
	ret = ioctl(pContext->mDevId, VIDIOC_QUERYCAP, &cap);
	if (-1 == ret)
	{
		loge("fatal error! VIDIOC_QURYCAP fail!");
		goto _close_dev;
	}
	logd("capabilities[%d]", cap.capabilities);
	printf("v4l2 info: \n");
	printf("dirver: %s\n", cap.driver);
	printf("card: %s\n", cap.card);
	printf("bus info: %s\n", cap.bus_info);
	printf("version: 0x%x\n", cap.version);
	printf("capabilities: 0x%08x\n", cap.capabilities);
	printf("device caps: 0x%08x\n", cap.device_caps);
	printf("reserved: 0x%x, 0x%x, 0x%x\n", cap.reserved[0], cap.reserved[1], cap.reserved[2]);

	memset(&pContext->mInp, 0, sizeof(pContext->mInp));
	while (0 == ioctl(pContext->mDevId, VIDIOC_ENUMINPUT, &pContext->mInp))
	{
		logd("==========================VIDIOC_ENUMINPUT==========================");
		logd("get input. index[%d], type[0x%x], name[%s], audio[%d], tuner[0x%x]" \
			"std[%lld], status[0x%x], capabilities[0x%x], reserved[0x%x-0x%x-0x%x]", \
		pContext->mInp.index, pContext->mInp.type, \
		pContext->mInp.name, pContext->mInp.audioset, pContext->mInp.tuner, \
		pContext->mInp.std, pContext->mInp.status, pContext->mInp.capabilities, \
		pContext->mInp.reserved[0], pContext->mInp.reserved[1], pContext->mInp.reserved[2]);
		pContext->mInp.index++;
		logd("==========================VIDIOC_ENUMINPUT==========================");
	}

	memset(&pContext->mInp, 0, sizeof(pContext->mInp));
	pContext->mInp.index = 0;
	ret = ioctl(pContext->mDevId, VIDIOC_S_INPUT, &pContext->mInp);
	if (-1 == ret)
	{
		loge("fatal error! VIDIOC_S_INPUT fail!");
		goto _close_dev;
	}

	struct v4l2_frmsizeenum framesizee_enum;
	while (0 == ioctl(pContext->mDevId, VIDIOC_ENUM_FRAMESIZES, &framesizee_enum))
	{
		logd("==========================VIDIOC_ENUM_FRAMESIZES==========================");
		logd("get framesize. index[%d] type[%d] discrete[%dx%d] stepwise[%dx%dx%dx%dx%dx%d]",
			framesizee_enum.index, framesizee_enum.type, framesizee_enum.discrete.width,
			framesizee_enum.discrete.height, framesizee_enum.stepwise.min_width,
			framesizee_enum.stepwise.max_width, framesizee_enum.stepwise.step_width,
			framesizee_enum.stepwise.min_height, framesizee_enum.stepwise.max_height,
			framesizee_enum.stepwise.step_height);
		framesizee_enum.index++;
		logd("==========================VIDIOC_ENUM_FRAMESIZES==========================");
	}

	struct v4l2_fmtdesc fmtdesc;
	while (0 == ioctl(pContext->mDevId, VIDIOC_ENUM_FMT, &fmtdesc))
	{
		logd("==========================VIDIOC_ENUM_FMT==========================");
		logd("get fmt. index[%d] type[%d] flags[%d] desc[%s] fmt[0x%x]",
			fmtdesc.index, fmtdesc.type, fmtdesc.flags,(char *)fmtdesc.description,
			(unsigned int)fmtdesc.pixelformat);
		fmtdesc.index++;
		logd("==========================VIDIOC_ENUM_FMT==========================");
	}

	memset(&pContext->mFmt, 0, sizeof(pContext->mFmt));
	pContext->mFmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	pContext->mFmt.fmt.pix_mp.width = 1280;
	pContext->mFmt.fmt.pix_mp.height = 720;
	pContext->mFmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_MJPEG;
	pContext->mFmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
	pContext->mFmt.fmt.pix_mp.colorspace = V4L2_COLORSPACE_JPEG;
	if (-1 == ioctl(pContext->mDevId, VIDIOC_S_FMT, &pContext->mFmt))
	{
		loge("fatal error! VIDIOC_S_FMT fail!");
		goto _close_dev;
	}

	if (-1 == ioctl(pContext->mDevId, VIDIOC_G_FMT, &pContext->mFmt))
	{
		loge("fatal error! VIDIOC_G_FMT fail!");
		goto _close_dev;
	}
	else
	{
		logd("get fmt. type[0x%x], size[%dx%d], pixelformat[0x%x], " \
			"field[0x%x], colorspace[0x%x], planes num[%d]", \
		pContext->mFmt.type, pContext->mFmt.fmt.pix_mp.width, pContext->mFmt.fmt.pix_mp.height, \
		pContext->mFmt.fmt.pix_mp.pixelformat, pContext->mFmt.fmt.pix_mp.field, \
		pContext->mFmt.fmt.pix_mp.colorspace, pContext->mFmt.fmt.pix_mp.num_planes);
	}

	pContext->mpBufferPool = (struct buffer_pool*)malloc(sizeof(*pContext->mpBufferPool));
	if (NULL == pContext->mpBufferPool)
	{
		loge("fatal error! malloc buffer pool fail!");
		goto _close_dev;
	}
	memset(pContext->mpBufferPool, 0, sizeof(*pContext->mpBufferPool));
	pContext->mpBufferPool->nbufs = nBufNum;

	memset(&pContext->mReqBuf, 0, sizeof(pContext->mReqBuf));
	pContext->mReqBuf.count = nBufNum;
	pContext->mReqBuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	pContext->mReqBuf.memory = V4L2_MEMORY_MMAP;
	if (-1 == ioctl(pContext->mDevId, VIDIOC_REQBUFS, &pContext->mReqBuf))
	{
		loge("fatal error! VIDIOC_REQBUFS fail!");
		goto _free_video_plane;
	}
	if (pContext->mReqBuf.count > pContext->mpBufferPool->nbufs)
	{
		loge("fatal error! driver need buf num[%d] buffer pool buf num[%d]", \
			pContext->mReqBuf.count, pContext->mpBufferPool->nbufs);
		goto _free_video_plane;
	}

	struct video_buffer *pVideoBuf = pContext->mpBufferPool->buffers = NULL;
	pVideoBuf = calloc(nBufNum, sizeof(*pVideoBuf));
	if (NULL == pVideoBuf)
	{
		loge("fatal error! calloc video buffer fail!");
		goto _free_buffer_pool;
	}

	struct video_plane *pVideoPlane = NULL;
	nPlaneNum = pContext->mFmt.fmt.pix_mp.num_planes;
	logd("planes %d", pContext->mFmt.fmt.pix_mp.num_planes);
	if (!pContext->mFmt.fmt.pix_mp.num_planes)
		nPlaneNum = 1;
	for (int i = 0; i < nBufNum; i++)
	{
		pVideoPlane = NULL;
		pVideoPlane = calloc(nPlaneNum, sizeof(*pVideoPlane));
		if (NULL == pVideoPlane)
		{
			loge("fatal error! calloc video buffer[%d] plane fail!", i);
			goto _free_video_plane;
		}
		pVideoBuf[i].index = i;
		pVideoBuf[i].planes = pVideoPlane;
		pVideoBuf[i].nplanes = nPlaneNum;
	}

	for (int i = 0; i < nBufNum; i++)
	{
		struct v4l2_buffer stv4l2Buf;
		struct v4l2_plane stv4l2Plane[8];
		memset(&stv4l2Buf, 0, sizeof(stv4l2Buf));
		memset(&stv4l2Buf, 0, sizeof(stv4l2Plane));

		stv4l2Buf.index = i;
		stv4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		stv4l2Buf.memory = V4L2_MEMORY_MMAP;

		if (-1 == ioctl(pContext->mDevId, VIDIOC_QUERYBUF, &stv4l2Buf))
		{
			loge("fatal error! VIDIOC_QUERYBUF fail!");
			goto _free_video_plane;
		}
		logd("buf %d length %d offset %d",
			i, stv4l2Buf.length, stv4l2Buf.m.offset);
		pVideoBuf[i].planes[0].size = stv4l2Buf.length;
		pVideoBuf[i].planes[0].mem = mmap(NULL, stv4l2Buf.length, \
			PROT_READ | PROT_WRITE, MAP_SHARED, pContext->mDevId, \
		stv4l2Buf.m.offset);
		if (MAP_FAILED == pVideoBuf[i].planes[0].mem)
		{
			loge("fatal error! mmap buf[%d] planes[%d] fail!", i, 0);
			goto _munmap_mem;
		}
	}

	for (int i = 0; i < nBufNum; i++)
	{
		struct v4l2_buffer stv4l2Buf;
		struct v4l2_plane stv4l2Plane[8];
		memset(&stv4l2Buf, 0, sizeof(stv4l2Buf));
		memset(stv4l2Plane, 0, sizeof(stv4l2Plane));

		stv4l2Buf.index = pVideoBuf[i].index;
		stv4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		stv4l2Buf.memory = V4L2_MEMORY_MMAP;

		if (-1 == ioctl(pContext->mDevId, VIDIOC_QBUF, &stv4l2Buf))
		{
			loge("fatal error! VIDIOC_QBUF fail!");
			goto _free_video_plane;
		}
	}

	int brightness = 100;
	logd("set value 0x%x", brightness);
	struct v4l2_control ctrl;
	ctrl.id = V4L2_CID_BRIGHTNESS;
	ctrl.value = brightness;
	if (-1 == ioctl(pContext->mDevId, VIDIOC_S_CTRL, &ctrl))
	{
		loge("V4L2_CID_BRIGHTNESS fail!");
	}
	ctrl.id = V4L2_CID_BRIGHTNESS;
	ctrl.value = 0;
	if (-1 == ioctl(pContext->mDevId, VIDIOC_G_CTRL, &ctrl))
	{
		loge("V4L2_CID_BRIGHTNESS fail!");
	}
	logd("get H264 resolution 0x%x", ctrl.value);

	pContext->mType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 == ioctl(pContext->mDevId, VIDIOC_STREAMON, &pContext->mType))
	{

		loge("fatal error! VIDIOC_STREAMON fail!");
		goto _munmap_mem;
	}

	fd_set fds;
	struct timeval tv;
	int frame_cnt = 0, err_cnt = 0;
#ifdef EXTRACT_DUAL_STREAM
	FILE *fp_dual_stream = fopen(SAVE_DUAL_STREAM_FILE, "wb");
	if (!fp_dual_stream)
		loge("open file %s fail!", SAVE_DUAL_STREAM_FILE);
#endif
	while (1)
	{
		if (g_exit_flag || (frame_cnt == 1000) || (err_cnt == 10))
			break;

#ifdef DEBUG_SAVE_JPEG
		if (frame_cnt >= 100)
			break;
#endif

		FD_ZERO(&fds);
		FD_SET(pContext->mDevId, &fds);
		memset(&tv, 0, sizeof(tv));
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		ret = select(pContext->mDevId+1, &fds, NULL, NULL, &tv);
		if (-1 == ret)
		{
			err_cnt++;
			loge("fatal error! select error!");
			continue;
		}
		else if (0 == ret)
		{
			err_cnt++;
			logw("select timeout!");
			continue;
		}

		struct v4l2_buffer stv4l2Buf;
		struct v4l2_plane stv4l2Plane[8];
		memset(&stv4l2Buf, 0, sizeof(stv4l2Buf));
		memset(&stv4l2Plane, 0, sizeof(stv4l2Plane));
		stv4l2Buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		stv4l2Buf.memory = V4L2_MEMORY_MMAP;
		if (-1 == ioctl(pContext->mDevId, VIDIOC_DQBUF, &stv4l2Buf))
		{
			loge("fatal error! VIDIOC_DQBUF fail!");
			goto _stream_off;
		}

		uint8_t *buf = (uint8_t *)pVideoBuf[stv4l2Buf.index].planes[0].mem;
		if ((buf[0] != 0xff) && (buf[1] != 0xd8))
		{
			loge("get zero data jpeg!");
			ioctl(pContext->mDevId, VIDIOC_QBUF, &stv4l2Buf);
			continue;
		}
		//logd("get frame %d addr %p len %d", stv4l2Buf.index, pVideoBuf[stv4l2Buf.index].planes[0].mem,
		//		stv4l2Buf.bytesused);

#ifdef EXTRACT_DUAL_STREAM
		struct dual_stream_info dual_stream;
		memset(&dual_stream, 0, sizeof(dual_stream));
		extract_dual_stram((uint8_t *)pVideoBuf[stv4l2Buf.index].planes[0].mem, stv4l2Buf.bytesused, &dual_stream);
		if (fp_dual_stream)
		{
			if (dual_stream.off0 && dual_stream.off0)
				fwrite(pVideoBuf[stv4l2Buf.index].planes[0].mem+dual_stream.off0, dual_stream.len0, 1, fp_dual_stream);
			if (dual_stream.off1 && dual_stream.len1)
				fwrite(pVideoBuf[stv4l2Buf.index].planes[0].mem+dual_stream.off1, dual_stream.len1, 1, fp_dual_stream);
			if (dual_stream.off2 && dual_stream.len2)
				fwrite(pVideoBuf[stv4l2Buf.index].planes[0].mem+dual_stream.off2, dual_stream.len2, 1, fp_dual_stream);
		}
#endif
#ifdef DEBUG_SAVE_JPEG
		char jpeg[64] = {0};
		sprintf(jpeg, "/mnt/extsd/jpeg/%02d_jpeg.jpg", frame_cnt);
		FILE *fp = fopen(jpeg, "wb");
		if (!fp)
			loge("open file %s fail!", jpeg);
		else
		{
			fwrite(pVideoBuf[stv4l2Buf.index].planes[0].mem, stv4l2Buf.bytesused, 1, fp);
			fclose(fp);
		}
#endif
#ifdef CHECK_JPEG_EOI
		if (checkJPEGEOI(pVideoBuf[stv4l2Buf.index].planes[0].mem, stv4l2Buf.bytesused))
		{
			g_exit_flag = 1;
			loge("jpeg %d less EOI!", frame_cnt);
		}
#endif
		if (-1 == ioctl(pContext->mDevId, VIDIOC_QBUF, &stv4l2Buf))
		{
			loge("fatal error! VIDIOC_QBUF fail!");
			goto _stream_off;
		}

		frame_cnt++;
	}
#ifdef EXTRACT_DUAL_STREAM
    if (fp_dual_stream)
        fclose(fp_dual_stream);
#endif
_stream_off:
	if (-1 == ioctl(pContext->mDevId, VIDIOC_STREAMOFF, &pContext->mType))
	{
		loge("fatal error! VIDIOC_STREAMOFF fail!");
    }
_munmap_mem:
	for (int i = 0; i < nBufNum; i++)
	{
		for (int j = 0; j < nPlaneNum; j++)
		{
			if (NULL == pVideoBuf[i].planes[j].mem)
			{
				continue;
			}
			if (munmap(pVideoBuf[i].planes[j].mem, pVideoBuf[i].planes[j].size))
			{
				loge("fatal error! buf num[%d] planes[%d] munmap fail!", i, j);
				goto _free_video_plane;
			}
		}
	}
_free_video_plane:
	for (int i = 0; i < nBufNum; i++)
	{
		if (pVideoBuf[i].planes)
		{
			free(pVideoBuf[i].planes);
			pVideoBuf[i].planes = NULL;
		}
	}
	if (pContext->mpBufferPool->buffers)
	{
		free(pContext->mpBufferPool->buffers);
		pContext->mpBufferPool->buffers = NULL;
	}
_free_buffer_pool:
	if (pContext->mpBufferPool)
	{
		free(pContext->mpBufferPool);
		pContext->mpBufferPool = NULL;
	}
_close_dev:
	if (-1 == pContext->mDevId)
	{
		close(pContext->mDevId);
		pContext->mDevId = -1;
	}
_free_context:
	if (pContext)
	{
		free(pContext);
		pContext = NULL;
	}
_exit:
		return err_cnt;
}
