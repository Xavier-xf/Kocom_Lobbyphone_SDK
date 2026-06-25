#include "aw_rtmedia.h"

#include <unistd.h>

#include "linux/videodev2.h"
#include "aw_osd.h"

int rt_media_init(void)
{
    DOORLOCK_DBG("enter ===>\n");
    AWVideoInput_Init();
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_deinit(void)
{
    DOORLOCK_DBG("enter ===>\n");
    AWVideoInput_DeInit();
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_chn_reset(rt_media_chn_t *rt_media_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    int encodeType = 0;
    int chn = rt_media_chn_info->vi_dev;
    uint32_t v4l2_fmt = rt_media_chn_info->v4l2_format_type;

    switch (v4l2_fmt) {
    case V4L2_PIX_FMT_H264:
        encodeType = 0;
        break;
    case V4L2_PIX_FMT_H265:
        encodeType = 2;
        break;
    case V4L2_PIX_FMT_MJPEG:
        encodeType = 1;
        break;
    case V4L2_PIX_FMT_JPEG:
        encodeType = 1;
        break;
    // case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_NV21:
    case V4L2_PIX_FMT_NV12:
        encodeType = 0;
        break;
    default:
        encodeType = 0;
        break;
    }

    AWVideoInput_Start(chn, 0);
    AWVideoInput_ResetEncoderType(chn, encodeType);
    AWVideoInput_ResetSize(chn, rt_media_chn_info->dst_width, rt_media_chn_info->dst_height);
    AWVideoInput_Start(chn, 1);

    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_chn_init(rt_media_chn_t *rt_media_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    VideoInputConfig *video_input_config = &rt_media_chn_info->chn_attr;
    video_input_config->channelId = rt_media_chn_info->chn;  // chn id
    DOORLOCK_DBG("vi_dev = %d, chn = %d !!\n", rt_media_chn_info->vi_dev, rt_media_chn_info->chn);

    if (rt_media_chn_info->dst_width == 0) {
        rt_media_chn_info->dst_width = 640;
    }
    if (rt_media_chn_info->dst_height == 0) {
        rt_media_chn_info->dst_height = 480;
    }
    if (rt_media_chn_info->dst_fps == 0) {
        rt_media_chn_info->dst_fps = 30;
    }
    if (rt_media_chn_info->input_rt_format == 0xffffffff) {
        rt_media_chn_info->input_rt_format = RT_PIXEL_LBC_25X;
    }
    if (rt_media_chn_info->bitrate == 0xffffffff &&
        rt_media_chn_info->v4l2_format_type != V4L2_PIX_FMT_YUYV) {
        rt_media_chn_info->bitrate = rt_media_compute_video_bitrate(
            video_input_config->pixelformat, rt_media_chn_info->v4l2_format_type,
            rt_media_chn_info->dst_width, rt_media_chn_info->dst_height, rt_media_chn_info->dst_fps);
        rt_media_chn_info->bitrate = rt_media_chn_info->bitrate / 1024;
    }

    video_input_config->venc_video_signal.video_format = DEFAULT;
    video_input_config->venc_video_signal.full_range_flag = 1;
    video_input_config->venc_video_signal.src_colour_primaries = VENC_YCC;
    video_input_config->venc_video_signal.dst_colour_primaries = VENC_YCC;

    switch (rt_media_chn_info->v4l2_format_type) {
    case V4L2_PIX_FMT_H264:
        video_input_config->encodeType = 0;
        video_input_config->output_mode = OUTPUT_MODE_STREAM;
        video_input_config->bitrate = rt_media_chn_info->bitrate /*/1024*/;
        video_input_config->pixelformat = rt_media_chn_info->input_rt_format;  // RT_PIXEL_LBC_25X;
        DOORLOCK_DBG("V4L2_PIX_FMT_H264 !!!\n");
        break;
    case V4L2_PIX_FMT_H265:
        video_input_config->encodeType = 2;
        video_input_config->output_mode = OUTPUT_MODE_STREAM;
        video_input_config->bitrate = rt_media_chn_info->bitrate /*/1024*/;
        video_input_config->pixelformat = rt_media_chn_info->input_rt_format;  // RT_PIXEL_LBC_25X;
        DOORLOCK_DBG("V4L2_PIX_FMT_H265 !!!\n");
        break;
    case V4L2_PIX_FMT_MJPEG:
        video_input_config->encodeType = 1;
        video_input_config->jpg_quality = 100;
        video_input_config->jpg_mode = 1;
        video_input_config->bitrate = rt_media_chn_info->bitrate /*/1024*/;
        video_input_config->bit_rate_range.bitRateMin = rt_media_chn_info->bitrate /*/1024*/;
        video_input_config->bit_rate_range.bitRateMax = rt_media_chn_info->bitrate /*/1024*/;
        video_input_config->output_mode = OUTPUT_MODE_STREAM;
        video_input_config->pixelformat = rt_media_chn_info->input_rt_format;  // RT_PIXEL_LBC_25X;
        DOORLOCK_DBG("V4L2_PIX_FMT_MJPEG !!!\n");
        break;
    case V4L2_PIX_FMT_JPEG:
        video_input_config->encodeType = 1;
        video_input_config->jpg_quality = rt_media_chn_info->enc_quality;
        video_input_config->jpg_mode = 0;
        video_input_config->bitrate = rt_media_chn_info->bitrate /*/1024*/;
        video_input_config->bit_rate_range.bitRateMin = rt_media_chn_info->bitrate /*/1024*/;
        video_input_config->bit_rate_range.bitRateMax = rt_media_chn_info->bitrate /*/1024*/;
        video_input_config->output_mode = OUTPUT_MODE_STREAM;
        video_input_config->pixelformat = rt_media_chn_info->input_rt_format;  // RT_PIXEL_LBC_25X;
        DOORLOCK_DBG("V4L2_PIX_FMT_JPEG !!!\n");
        break;
    // case V4L2_PIX_FMT_YUYV:
    case V4L2_PIX_FMT_NV21:
        video_input_config->encodeType = 0;
        video_input_config->output_mode = OUTPUT_MODE_YUV;
        video_input_config->bitrate = 0;
        video_input_config->pixelformat = RT_PIXEL_YVU420SP;
        DOORLOCK_DBG("V4L2_PIX_FMT_NV21 !!!\n");
        break;
    case V4L2_PIX_FMT_NV12:
        video_input_config->encodeType = 0;
        video_input_config->output_mode = OUTPUT_MODE_YUV;
        video_input_config->bitrate = 0;
        video_input_config->pixelformat = RT_PIXEL_YUV420SP;
        DOORLOCK_DBG("V4L2_PIX_FMT_NV12 !!!\n");
        break;
    default:
        DOORLOCK_ERR("unsupport encode type[0x%x], use default yuv\n",
                    rt_media_chn_info->v4l2_format_type);
        video_input_config->encodeType = 0;
        video_input_config->output_mode = OUTPUT_MODE_YUV;
        video_input_config->bitrate = 0;
        video_input_config->pixelformat = RT_PIXEL_YVU420SP;
        break;
    }

    video_input_config->width = rt_media_chn_info->src_width;
    video_input_config->height = rt_media_chn_info->src_height;
    video_input_config->dst_width = rt_media_chn_info->dst_width;
    video_input_config->dst_height = rt_media_chn_info->dst_height;
    video_input_config->fps = rt_media_chn_info->dst_fps;
    video_input_config->gop = rt_media_chn_info->dst_fps;
    video_input_config->drop_frame_num = rt_media_chn_info->drop_frame_num;
    video_input_config->enable_wdr = 0;
    video_input_config->enable_overlay = 0;
    video_input_config->demo_start = 0;
    video_input_config->enable_sharp = 0;

    video_input_config->share_buf_num = 2;
    video_input_config->breduce_refrecmem = 1;
    video_input_config->bonline_channel = 0;

    if (rt_media_chn_info->v4l2_format_type == V4L2_PIX_FMT_H264) {
        video_input_config->profile = VENC_H264ProfileMain;
        video_input_config->level = VENC_H264Level51;
        video_input_config->qp_range.nMinqp = 35;
        video_input_config->qp_range.nMaxqp = 51;
        video_input_config->qp_range.nMinPqp = 35;
        video_input_config->qp_range.nMaxPqp = 51;
        video_input_config->qp_range.nQpInit = 35;
        video_input_config->qp_range.bEnMbQpLimit = 0;
    } else if (rt_media_chn_info->v4l2_format_type == V4L2_PIX_FMT_H265) {
        video_input_config->profile = VENC_H265ProfileMain;
        video_input_config->level = VENC_H265Level51;
        video_input_config->qp_range.nMinqp = 35;
        video_input_config->qp_range.nMaxqp = 51;
        video_input_config->qp_range.nMinPqp = 35;
        video_input_config->qp_range.nMaxPqp = 51;
        video_input_config->qp_range.nQpInit = 35;
        video_input_config->qp_range.bEnMbQpLimit = 0;
    }

    video_input_config->breduce_refrecmem = 1;  // 不能与重编码 SuperFrame共用!!!
	int vbvBufferThresh = rt_media_chn_info->bitrate / 8 / rt_media_chn_info->dst_fps * 15 * 1024;
	int vbvBufferSize = rt_media_chn_info->bitrate / 8 * 3 * 1024 + vbvBufferThresh;

	DOORLOCK_WARN("vbvBufferThresh:%d, vbvBufferSize:%d\n", vbvBufferThresh, vbvBufferSize);
	video_input_config->vbv_buf_size = vbvBufferSize;//2*1024*1024;
	video_input_config->vbv_thresh_size = vbvBufferThresh;//100*1024;

	DOORLOCK_WARN("vbv_buf_size:%d, vbv_thresh_size:%d\n", video_input_config->vbv_buf_size, video_input_config->vbv_thresh_size);

    AWVideoInput_Configure(rt_media_chn_info->vi_dev, video_input_config);
    if (video_input_config->output_mode == OUTPUT_MODE_STREAM) {
        AWVideoInput_CallBack(rt_media_chn_info->vi_dev, rt_media_chn_info->stream_callback, 1);
    }

    if (rt_media_chn_info->exit_callback) {
        AWVideoInput_SetChannelThreadExitCb(rt_media_chn_info->chn, rt_media_chn_info->exit_callback);
    }

    VideoChannelInfo video_chn_info;
    memset(&video_chn_info, 0, sizeof(VideoChannelInfo));
    AWVideoInput_GetChannelInfo(rt_media_chn_info->vi_dev, &video_chn_info);
    DOORLOCK_DBG("chn[%d] encodeType[%d] size[%dx%d] fps[%d] bitrate[%d]\n",
                video_chn_info.mConfig.channelId, video_chn_info.mConfig.encodeType,
                video_chn_info.mConfig.width, video_chn_info.mConfig.height,
                video_chn_info.mConfig.fps, video_chn_info.mConfig.bitrate);

    pthread_mutex_init(&rt_media_chn_info->chn_mutex, NULL);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_chn_deinit(rt_media_chn_t *rt_media_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    AWVideoInput_Destroy(rt_media_chn_info->vi_dev);
    pthread_mutex_destroy(&rt_media_chn_info->chn_mutex);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_chn_start(rt_media_chn_t *rt_media_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    AWVideoInput_Start(rt_media_chn_info->vi_dev, 1);
    rt_media_chn_info->chn_state = 1;
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_chn_stop(rt_media_chn_t *rt_media_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    rt_media_chn_info->chn_state = 0;
    AWVideoInput_Start(rt_media_chn_info->vi_dev, 0);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_chn_restart(rt_media_chn_t *rt_media_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    rt_media_chn_stop(rt_media_chn_info);
    rt_media_chn_deinit(rt_media_chn_info);

    rt_media_chn_init(rt_media_chn_info);
    rt_media_chn_start(rt_media_chn_info);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_chn_request_yuv_data(rt_media_chn_t *rt_media_chn_info, VideoYuvFrame *frame)
{
    int ret = 0;

    if (AWVideoInput_Check_Wait_Start(rt_media_chn_info->vi_dev)) {
        return -1;
    }

    pthread_mutex_lock(&rt_media_chn_info->chn_mutex);
    if (rt_media_chn_info->chn_state == 1) {
        ret = AWVideoInput_GetYuvFrame(rt_media_chn_info->vi_dev, frame);// IOCTL_REQUEST_YUV_FRAME
        rt_media_chn_info->chn_state = 2;
    } else {
        ret = -1;
    }
    pthread_mutex_unlock(&rt_media_chn_info->chn_mutex);
    return ret;
}

int rt_media_chn_return_yuv_data(rt_media_chn_t *rt_media_chn_info, VideoYuvFrame *frame)
{
    int ret = 0;

    pthread_mutex_lock(&rt_media_chn_info->chn_mutex);
    ret = AWVideoInput_ReleaseYuvFrame(rt_media_chn_info->vi_dev, frame);  // IOCTL_RETURN_YUV_FRAME
    if (rt_media_chn_info->chn_state == 2) {
        rt_media_chn_info->chn_state = 1;
    }
    pthread_mutex_unlock(&rt_media_chn_info->chn_mutex);
    return ret;
}

int rt_media_chn_get_jpeg_data(rt_media_chn_t *rt_media_chn_info, uint8_t *data_buf, uint32_t max_data_len,
                          uint32_t jpeg_width, uint32_t jpeg_height, uint8_t jpeg_quality,
                          uint32_t rotate_angle)
{
    int jpeg_len = 0;
    int chn_id = rt_media_chn_info->vi_dev;
    int channel_state = AWVideoInput_Get_channel_state(chn_id);
    int csi_status = AWVideoInput_Get_csi_status(chn_id);
    DOORLOCK_INFO("GetJpegData chn_id = %d\n", chn_id);

    if (jpeg_quality == 0) {
        jpeg_quality = 99;
    }
    if (csi_status == 0 || channel_state != VIDEO_INPUT_STATE_EXCUTING ||
        rt_media_chn_info->chn_state == 0) {
        DOORLOCK_ERR("csi_status = %d, channel_state = %d, chn_state = %d\n", csi_status,
                    channel_state, rt_media_chn_info->chn_state);
        jpeg_len = 0;
    } else {
        for (int i = 0; i < 100; i++) {
            pthread_mutex_lock(&rt_media_chn_info->chn_mutex);
            if (rt_media_chn_info->chn_state == 1) {
                catch_jpeg_config jpg_config;
                memset(&jpg_config, 0, sizeof(catch_jpeg_config));
                jpg_config.channel_id = chn_id;
                jpg_config.width = jpeg_width;
                jpg_config.height = jpeg_height;
                jpg_config.qp = jpeg_quality;  // 99
                jpg_config.rotate_angle = rotate_angle;
                AWVideoInput_CatchJpegConfig(&jpg_config);

                jpeg_len = max_data_len;
                if (AWVideoInput_CatchJpeg(data_buf, &jpeg_len, chn_id) == 0) {
                } else {
                    DOORLOCK_ERR("AWVideoInput_CatchJpeg failed\n");
                    jpeg_len = 0;
                }
                pthread_mutex_unlock(&rt_media_chn_info->chn_mutex);
                break;
            } else {
                DOORLOCK_ERR("state not right %d, loop %d\n", rt_media_chn_info->chn_state, i);
                jpeg_len = 0;
            }
            pthread_mutex_unlock(&rt_media_chn_info->chn_mutex);
            usleep(10 * 1000);
        }
    }
    return jpeg_len;
}

int rt_media_chn_set_ir_mode(int chn_id, bool ir_mode)
{
    RTIrParam ir_param;

    memset(&ir_param, 0, sizeof(RTIrParam));
    if (ir_mode) {
        ir_param.grey = 1;
        ir_param.ir_on = 1;
    }
    ir_param.ir_flash_on = 0;
    AWVideoInput_SetIrParam(chn_id, &ir_param);
}

int rt_media_chn_set_flip_ctrl(int chn_id, int flip_mode)
{
    if (flip_mode & 0x01) {
        AWVideoInput_SetHFlip(chn_id, 1);
    } else {
        AWVideoInput_SetHFlip(chn_id, 0);
    }
    if (flip_mode & 0x02) {
        AWVideoInput_SetVFlip(chn_id, 1);
    } else {
        AWVideoInput_SetVFlip(chn_id, 0);
    }
    return 0;
}

int rt_media_isp_set_local_exparea(int chn_id, int res_w, int res_h, int x1, int y1, int x2, int y2)
{
    int channel_state = AWVideoInput_Get_channel_state(chn_id);

    if (channel_state != VIDEO_INPUT_STATE_EXCUTING) {
        return -1;
    }

    RTIspCtrlAttr isp_ctrl_attr;
    isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_AE_ROI;
    struct isp_ae_roi_attr *ae_roi = &isp_ctrl_attr.isp_attr_cfg.ae_roi_area;

    int size = 2000;     // H3A_PIC_SIZE;//
    int offset = -1000;  // H3A_PIC_OFFSET;//

    ae_roi->coor.x1 = x1 * size / res_w + offset;
    ae_roi->coor.x2 = x2 * size / res_w + offset;
    ae_roi->coor.y1 = y1 * size / res_h + offset;
    ae_roi->coor.y2 = y2 * size / res_h + offset;

    AWVideoInput_SetIspAttrCfg(chn_id, &isp_ctrl_attr);
    return 0;
}

int rt_media_isp_set_local_exparea_force(int chn_id, int res_w, int res_h, int x1, int y1, int x2, int y2,
                                   uint8_t force_value)
{
    int channel_state = AWVideoInput_Get_channel_state(chn_id);

    if (channel_state != VIDEO_INPUT_STATE_EXCUTING) {
        return -1;
    }
    DOORLOCK_INFO("[%d], w[%d], h[%d], [%d %d %d %d] value[%d]\n", chn_id, res_w, res_h, x1, y1, x2,
                 y2, force_value);
    RTIspCtrlAttr isp_ctrl_attr;
    isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_AE_ROI_TARGET;
    struct isp_ae_roi_attr *ae_roi = &isp_ctrl_attr.isp_attr_cfg.ae_roi_area;
    if (force_value) {
        ae_roi->enable = 1;
        ae_roi->force_ae_target = force_value;  // 1~255
    } else {
        ae_roi->enable = 0;
        ae_roi->force_ae_target = force_value;  // 1~255
    }
    int size = 2000;     // H3A_PIC_SIZE;//
    int offset = -1000;  // H3A_PIC_OFFSET;//

    ae_roi->coor.x1 = x1 * size / res_w + offset;
    ae_roi->coor.x2 = x2 * size / res_w + offset;
    ae_roi->coor.y1 = y1 * size / res_h + offset;
    ae_roi->coor.y2 = y2 * size / res_h + offset;
    AWVideoInput_SetIspAttrCfg(chn_id, &isp_ctrl_attr);
    return 0;
}

int rt_media_isp_save_ae(int chn_id)
{
    RTIspCtrlAttr isp_ctrl_attr;

//    isp_ctrl_attr.isp_attr_cfg.cfg_id = RT_ISP_CTRL_AE_SAVE;
//    AWVideoInput_SetIspAttrCfg(chn_id, &isp_ctrl_attr);
    return 0;
}

// recommend, not must
unsigned int rt_media_compute_video_bitrate(uint32_t src_pix_format, uint32_t v4l2_fmt, int width,
                                        int height, int fps)
{
    unsigned int compression_ratio = 20;                                            // default
    unsigned int bitrate = (width * height * 3 / 2) * fps * 8 / compression_ratio;  // default

    switch (v4l2_fmt) {
    case V4L2_PIX_FMT_H264:
        compression_ratio = 250;
        break;
    case V4L2_PIX_FMT_H265:
        compression_ratio = 300;
        break;
    case V4L2_PIX_FMT_JPEG:
        // useless
        compression_ratio = 20;  // 10~40
        break;
    case V4L2_PIX_FMT_MJPEG:
        compression_ratio = 10;  // 20~80
        break;
    default:
        break;
    }
    bitrate = (width * height * 3 / 2) * fps * 8 / compression_ratio;
#if 1
    switch (src_pix_format) {
    case RT_PIXEL_LBC_25X:
        bitrate = (width * height * 3 / 2) * fps * 8 / compression_ratio;
        break;
    case RT_PIXEL_LBC_2X:
        bitrate = (width * height * 3 / 2) * fps * 8 / compression_ratio;
        break;
    case RT_PIXEL_YUV420SP:
    case RT_PIXEL_YVU420SP:
    case RT_PIXEL_YUV420P:
    case RT_PIXEL_YVU420P:
        bitrate = (width * height * 3 / 2) * fps * 8 / compression_ratio;
        break;
    default:
        break;
    }
#endif
    return bitrate;
}

int rt_media_osd_init(rt_media_osd_t *media_osd)
{
    DOORLOCK_DBG("enter ===>\n");
    VideoInputOSD *overlay_info = &media_osd->video_osd_info;
#if 1
    memset(media_osd->item_valid, 0, sizeof(media_osd->item_valid));
    memset(overlay_info, 0, sizeof(VideoInputOSD));
    overlay_info->osd_num = MAX_OVERLAY_ITEM_SIZE;
#endif
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_osd_deinit(rt_media_osd_t *media_osd)
{
    DOORLOCK_DBG("enter ===>\n");
    VideoInputOSD *overlay_info = &media_osd->video_osd_info;
    overlay_info->osd_num = 0;
    memset(overlay_info, 0, sizeof(VideoInputOSD));
    AWVideoInput_SetOSD(media_osd->vi_dev, overlay_info);

    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_osd_chn_update(rt_media_osd_chn_t *rt_media_osd_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    // rt_media_osd_chn_deinit(rt_media_osd_chn_info);
    rt_media_osd_chn_init(rt_media_osd_chn_info);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_osd_chn_init(rt_media_osd_chn_t *rt_media_osd_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    int ret = 0;
    rt_media_osd_t *media_osd = rt_media_osd_chn_info->media_osd;
    VideoInputOSD *overlay_info = &media_osd->video_osd_info;
    int index = rt_media_osd_chn_info->chn;  // MAX_OVERLAY_ITEM_SIZE

    if (media_osd == NULL) {
        DOORLOCK_ERR("media_osd NULL\n");
        DOORLOCK_DBG("exit <===\n");
        return -1;
    }
    if (rt_media_osd_chn_info->osd_type == 0 || rt_media_osd_chn_info->osd_type == 1) {
        if (index >= MAX_OVERLAY_ITEM_SIZE) {
            DOORLOCK_ERR("index %d > MAX_OVERLAY_ITEM_SIZE(%d)\n", index, MAX_OVERLAY_ITEM_SIZE);
            DOORLOCK_DBG("exit <===\n");
            return -1;
        }
    }

    if (rt_media_osd_chn_info->show) {
        /*
        typedef enum OVERLAY_ARGB_TYPE {
            OVERLAY_ARGB_MIN   = -1,
            OVERLAY_ARGB8888   = 0,
            OVERLAY_ARGB4444   = 1,
            OVERLAY_ARGB1555   = 2,
            OVERLAY_ARGB_MAX   = 3,
        } OVERLAY_ARGB_TYPE;
        */
        if (rt_media_osd_chn_info->osd_type == 0) {
            overlay_info->item_info[index].start_x = rt_media_osd_chn_info->pos_x;
            overlay_info->item_info[index].start_y = rt_media_osd_chn_info->pos_y;
            overlay_info->item_info[index].widht = rt_media_osd_chn_info->width;
            overlay_info->item_info[index].height = rt_media_osd_chn_info->height;

            overlay_info->item_info[index].osd_type = NORMAL_OVERLAY;
            overlay_info->argb_type = rt_media_osd_chn_info->overlay_argb_type;
            if (overlay_info->argb_type == VENC_OVERLAY_ARGB8888) {
                overlay_info->item_info[index].data_size = overlay_info->item_info[index].widht *
                                                           overlay_info->item_info[index].height *
                                                           4;
            } else if (overlay_info->argb_type == VENC_OVERLAY_ARGB4444) {
                overlay_info->item_info[index].data_size = overlay_info->item_info[index].widht *
                                                           overlay_info->item_info[index].height *
                                                           2;
            } else if (overlay_info->argb_type == VENC_OVERLAY_ARGB1555) {
                overlay_info->item_info[index].data_size = overlay_info->item_info[index].widht *
                                                           overlay_info->item_info[index].height *
                                                           2;
            } else {
                overlay_info->item_info[index].data_size = 0;
            }
            // rt_media_osd_chn_info->argb_data = malloc(overlay_info->item_info[index].data_size);
            // if(rt_media_osd_chn_info->argb_data == NULL){
            //	DOORLOCK_ERR("malloc %d failed\n", overlay_info->item_info[index].data_size);
            // }
            overlay_info->item_info[index].data_buf = rt_media_osd_chn_info->argb_data;
        } else if (rt_media_osd_chn_info->osd_type == 1) {
            overlay_info->item_info[index].start_x = rt_media_osd_chn_info->pos_x;
            overlay_info->item_info[index].start_y = rt_media_osd_chn_info->pos_y;
            overlay_info->item_info[index].widht = rt_media_osd_chn_info->width;
            overlay_info->item_info[index].height = rt_media_osd_chn_info->height;

            overlay_info->item_info[index].data_buf = NULL;
            overlay_info->item_info[index].osd_type = COVER_OVERLAY;
            uint8_t color_y, color_u, color_v;
            color_y = rt_media_osd_chn_info->color_y;
            color_u = rt_media_osd_chn_info->color_u;
            color_v = rt_media_osd_chn_info->color_v;
            // if(rt_media_osd_chn_info->color_rgb)
            if (color_y == 0 && color_u == 0 && color_v == 0) {
                uint8_t colorR, colorG, colorB;
                colorR = rt_media_osd_chn_info->color_rgb >> 16;
                colorG = rt_media_osd_chn_info->color_rgb >> 8;
                colorB = rt_media_osd_chn_info->color_rgb >> 0;
                COLOR_RGB_TO_YUV(colorR, colorG, colorB, &color_y, &color_u, &color_v);
            }
            DOORLOCK_DBG("y = 0x%x, u = 0x%x, v = 0x%x\n", color_y, color_u, color_v);
            overlay_info->item_info[index].cover_yuv.cover_y = color_y;  // COLOR_RGB_TO_YUV
            overlay_info->item_info[index].cover_yuv.cover_u = color_u;
            overlay_info->item_info[index].cover_yuv.cover_v = color_v;
        }
    } else {
        if (rt_media_osd_chn_info->osd_type == 0 || rt_media_osd_chn_info->osd_type == 1) {
            memset(&overlay_info->item_info[index], 0, sizeof(OverlayItemInfo));
        }
    }
    if (rt_media_osd_chn_info->osd_type == 0 || rt_media_osd_chn_info->osd_type == 1) {
#if 1
        media_osd->item_valid[index] = 1;
        VideoInputOSD tmp_osd;
        memset(&tmp_osd, 0, sizeof(tmp_osd));
        tmp_osd.osd_num = 0;
        for (int i = 0; i < MAX_OVERLAY_ITEM_SIZE; i++) {
            if (media_osd->item_valid[i]) {
                memcpy(&tmp_osd.item_info[tmp_osd.osd_num], &overlay_info->item_info[i],
                       sizeof(tmp_osd.item_info[0]));
                tmp_osd.osd_num++;
            }
        }
        ret = AWVideoInput_SetOSD(media_osd->vi_dev, &tmp_osd);
#else
        ret = AWVideoInput_SetOSD(media_osd->vi_dev, overlay_info);
#endif
    }
    // int AWVideoInput_SetOSD(int channel, VideoInputOSD *pOsdInfo);
    // int AWVideoInput_SetIspOrl(int channel, RTIspOrl *isp_orl);
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int rt_media_osd_chn_deinit(rt_media_osd_chn_t *rt_media_osd_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    int ret = 0;
    rt_media_osd_t *media_osd = rt_media_osd_chn_info->media_osd;
    VideoInputOSD *overlay_info = &media_osd->video_osd_info;
    int index = rt_media_osd_chn_info->chn;

    if (media_osd == NULL) {
        DOORLOCK_ERR("media_osd NULL\n");
        DOORLOCK_DBG("exit <===\n");
        return -1;
    }

    if (rt_media_osd_chn_info->osd_type == 0 || rt_media_osd_chn_info->osd_type == 1) {
        if (index >= MAX_OVERLAY_ITEM_SIZE) {
            DOORLOCK_DBG("exit <===\n");
            return -1;
        }
        memset(&overlay_info->item_info[index], 0, sizeof(OverlayItemInfo));
#if 1
        media_osd->item_valid[index] = 0;
        VideoInputOSD tmp_osd;
        memset(&tmp_osd, 0, sizeof(tmp_osd));
        tmp_osd.osd_num = 0;
        for (int i = 0; i < MAX_OVERLAY_ITEM_SIZE; i++) {
            if (media_osd->item_valid[i]) {
                memcpy(&tmp_osd.item_info[tmp_osd.osd_num], &overlay_info->item_info[i],
                       sizeof(tmp_osd.item_info[0]));
                tmp_osd.osd_num++;
            }
        }
        ret = AWVideoInput_SetOSD(media_osd->vi_dev, &tmp_osd);
#else
        if (overlay_info->osd_num) {
            overlay_info->osd_num--;
        }
        ret = AWVideoInput_SetOSD(media_osd->vi_dev, overlay_info);
#endif
    }

    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int rt_media_osd_chn_draw_text(rt_media_osd_chn_t *rt_media_osd_chn_info, char *str_buf, int rel_x, int rel_y,
                          uint32_t ft_color, uint32_t bk_color, uint8_t font_size, char *font_dir)
{
    font_info_t fontInfo;
    font_info_t *pFontInfo = &fontInfo;

    draw_font_init(pFontInfo, font_size, font_dir);
    draw_text_utf8(rt_media_osd_chn_info->argb_data, 4, rt_media_osd_chn_info->width,
                   rt_media_osd_chn_info->height, str_buf, rel_x, rel_y, pFontInfo, ft_color, bk_color);
}

int rt_media_osd_chn_draw_line(rt_media_osd_chn_t *rt_media_osd_chn_info, int rel_x, int rel_y, int width,
                          int height, uint32_t color, uint8_t line_width)
{
    draw_line(rt_media_osd_chn_info->argb_data, 4, rt_media_osd_chn_info->width,
              rt_media_osd_chn_info->height, rel_x, rel_y, rel_x + width, rel_y + height, color,
              line_width);
}

int rt_media_osd_chn_draw_rect(rt_media_osd_chn_t *rt_media_osd_chn_info, int rel_x, int rel_y, int width,
                          int height, uint32_t color, uint8_t line_width)
{
    draw_rect(rt_media_osd_chn_info->argb_data, 4, rt_media_osd_chn_info->width,
              rt_media_osd_chn_info->height, rel_x, rel_y, rel_x + width, rel_y + height, color,
              line_width);
}

int rt_media_orl_init(rt_media_orl_t *media_orl_info)
{
    DOORLOCK_DBG("enter ===>\n");
    RTIspOrl *isp_orl = &media_orl_info->isp_osd_info;
    isp_orl->orl_cnt = media_orl_info->num;  // MAX_ISP_ORL_NUM;

    isp_orl->on = 1;
    isp_orl->orl_width = media_orl_info->line_width;
    for (int i = 0; i < isp_orl->orl_cnt; i++) {
        isp_orl->orl_win[i].width = media_orl_info->width[i];
        isp_orl->orl_win[i].height = media_orl_info->height[i];
        isp_orl->orl_win[i].left = media_orl_info->pos_x[i];
        isp_orl->orl_win[i].top = media_orl_info->pos_y[i];
        isp_orl->orl_win[i].rgb_orl = media_orl_info->color_rgb[i];
    }

    AWVideoInput_SetIspOrl(media_orl_info->vi_dev, isp_orl);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_orl_deinit(rt_media_orl_t *media_orl_info)
{
    DOORLOCK_DBG("enter ===>\n");
    RTIspOrl *isp_orl = &media_orl_info->isp_osd_info;

    isp_orl->orl_cnt = 0;
    isp_orl->on = 0;

    AWVideoInput_SetIspOrl(media_orl_info->vi_dev, isp_orl);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int rt_media_orl_update(rt_media_orl_t *media_orl_info)
{
    DOORLOCK_DBG("enter ===>\n");
    rt_media_orl_deinit(media_orl_info);
    rt_media_orl_init(media_orl_info);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}
