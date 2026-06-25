#ifndef __VIDEO_ENCODE_H__
#define __VIDEO_ENCODE_H__

enum video_encode_type
{
    VIDEO_ENCODE_TYPE_COLORBAR = 0,
    VIDEO_ENCODE_TYPE_RT_MEDIA,
    VIDEO_ENCODE_TYPE_MPP,
    VIDEO_ENCODE_TYPE_FILE,
    VIDEO_ENCODE_TYPE_BUTT
};

enum video_encode_pixfmt {
    VIDEO_ENCODE_PIXFMT_NV21 = 0,
    VIDEO_ENCODE_PIXFMT_NV12,
    VIDEO_ENCODE_PIXFMT_NV61,
    VIDEO_ENCODE_PIXFMT_NV16,
    VIDEO_ENCODE_PIXFMT_LBC1_0X,
    VIDEO_ENCODE_PIXFMT_LBC1_5X,
    VIDEO_ENCODE_PIXFMT_LBC2_0X,
    VIDEO_ENCODE_PIXFMT_LBC2_5X,
    VIDEO_ENCODE_PIXFMT_BUTT
};

enum video_encode_stream_type {
    VIDEO_ENCODE_STREAM_TYPE_YUV = 0,
    VIDEO_ENCODE_STREAM_TYPE_H264,
    VIDEO_ENCODE_STREAM_TYPE_H265,
    VIDEO_ENCODE_STREAM_TYPE_MJPEG,
    VIDEO_ENCODE_STREAM_TYPE_JPEG,
    VIDEO_ENCODE_STREAM_TYPE_BUTT
};

struct video_encode_spspps_info {
    void *buf;
    unsigned int len;
};

struct video_encode_frame {
    int id;
    int key;
    int width;
    int height;
    void *vir_addr[3];
    unsigned int phy_addr[3];
    unsigned int len[3];
    unsigned long long pts;
    enum video_encode_pixfmt pix_fmt;
};

struct video_encode_config
{
    int channel;
    int width;
    int height;
    int format;
    int framerate;
    int bitrate;
};

struct motion_search_region {
	int pix_x_bgn;
	int pix_x_end;
	int pix_y_bgn;
	int pix_y_end;
	int thumb_x_bgn;
	int thumb_x_end;
	int thumb_y_bgn;
	int thumb_y_end;
	int thumb_pix_num;
	int total_num;
	int intra_num;
	int large_mv_num;
	int small_mv_num;
	int zero_mv_num;
	int large_mad_num;
	int is_motion;
};

struct motion_search_result {
	int total_region_num;
	int motion_region_num;
	struct motion_search_region *region;
};

struct video_encode_osd_item {
    int                 index;
    int                 enable;
    unsigned short		x;
    unsigned short		y;
    unsigned int		w;
    unsigned int		h;
    unsigned int        osd_type;	//reference definition of VENC_OVERLAY_TYPE
    unsigned char       *data_buf;	//the vir addr of overlay block
    unsigned int		data_size;	//the size of bitmap
};

struct video_encode_ops {
    int (*create)(void *thiz);
    int (*destroy)(void *thiz);
    int (*start)(void *thiz, struct video_encode_config *base_config);
    int (*stop)(void *thiz);
    int (*put_frame)(void *thiz, struct video_encode_frame *frame);
    int (*get_frame)(void *thiz, struct video_encode_frame *frame);
    int (*release_frame)(void *thiz, struct video_encode_frame *frame);
    void (*get_motion_search_result)(void *thiz, struct motion_search_result *result);
    int (*request_idr)(void *thiz);
    int (*pause)(void *thiz, int flag);
    int (*get_spspps_info)(void *thiz, struct video_encode_spspps_info *info);
    int (*set_osd)(void *thiz, void *osd);
    int (*set_sharp_param)(void *thiz, void *param);
};

struct video_encode
{
    void *ops_data;
    const struct video_encode_ops *ops;
};

struct video_encode *video_encode_create(enum video_encode_type video_encode_type);
void video_encode_destroy(struct video_encode *video_encode);
int video_encode_start(struct video_encode *video_encode, struct video_encode_config *config);
int video_encode_stop(struct video_encode *video_encode);
int video_encode_put_frame(struct video_encode *video_encode, struct video_encode_frame *frame);
int video_encode_get_frame(struct video_encode *video_encode, struct video_encode_frame *frame);
int video_encode_release_frame(struct video_encode *video_encode, struct video_encode_frame *frame);
void video_encode_get_motion_search_result(struct video_encode *video_encode, struct motion_search_result *result);
int video_encode_request_idr(struct video_encode *video_encode);
int video_encode_pause(struct video_encode *video_encode, int flag);
int video_encode_get_spspps_info(struct video_encode *video_encode, struct video_encode_spspps_info *info);
int video_encode_set_osd(struct video_encode *video_encode, void *osd);
int video_encode_set_sharp_param(struct video_encode *video_encode, void *param);

#endif
