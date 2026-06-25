#ifndef __SAMPLE_MUXER_MULTI_STREAM_H__
#define __SAMPLE_MUXER_MULTI_STREAM_H__

#include <mm_common.h>
#include <mm_comm_video.h>
#include <mm_comm_vi.h>
#include <mm_comm_venc.h>
#include <mm_comm_mux.h>
#include <mm_comm_sys.h>
#include <tsemaphore.h>
#include "tmessage.h"

#define MAX_FILE_PATH_SIZE      (256)
#define MAX_STREAM_NUM          (4)
#define MsgQueue_Stop           (-1)

struct sample_muxer_multi_stream_cmdline
{
    char config_file[MAX_FILE_PATH_SIZE];
};

struct sample_muxer_multi_stream_config
{
    int vi_dev;
    int isp_dev;
    int cap_width;
    int cap_height;
    int cap_framerate;
    PIXEL_FORMAT_E cap_format;
    int vi_bufnum;
    int enable_wdr;

    int enc_online_enable;
    int enc_online_share_bufnum;
    PAYLOAD_TYPE_E enc_type;
    int enc_width;
    int enc_height;
    int enc_framerate;
    int enc_bitrate;
    int enc_rcmode;
    int encpp_enable;
    int isp_ve_linkage;
    PIXEL_FORMAT_E enc_ve_ref_frame_lbc;
    int enc_key_framerate;
};

struct sample_muxer_multi_stream_stream
{
    int stream_valid;
    struct sample_muxer_multi_stream_config stream_config;

    VI_DEV vi_dev;
    ISP_DEV isp_dev;
    VI_CHN vi_chn;
    VI_ATTR_S vi_attr;

    VENC_CHN ve_chn;
    VENC_CHN_ATTR_S ve_chn_attr;
    VENC_RC_PARAM_S ve_rc_param;
    VencHeaderData ve_spspps_info;
};

struct sample_muxer_multi_stream_context
{
    struct sample_muxer_multi_stream_cmdline cmdline;

    int stream_num;
    struct sample_muxer_multi_stream_stream stream[MAX_STREAM_NUM];
    int test_duration;
    int video_file_duration;
    int video_file_max_cnt;
    int video_file_cnt;
    char video_dst_file[MAX_FILE_PATH_SIZE];

    MUX_CHN mux_chn;
    MUX_CHN_ATTR_S mux_chn_attr;
    cdx_sem_t sem_exit;
    message_queue_t msg_queue;
    pthread_t msg_queue_trd;
};

#endif
