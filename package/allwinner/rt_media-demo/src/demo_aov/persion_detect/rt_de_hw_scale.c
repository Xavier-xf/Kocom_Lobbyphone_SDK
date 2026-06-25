#include "../utils/include/debug.h"

#include "rt_de_hw_scale.h"

static int de_device_open()
{
    int de_fd = -1;
    de_fd = open("/dev/disp", O_RDWR);
    if (de_fd < 0) {
        logd("Open DE device failed!");
        return -1;
    }

    return de_fd;
}

static void de_device_close(int de_fd)
{
    close(de_fd);
}

static int de_reset_device_config(SampleDewbInfo *pDewbInfo)
{
    int ret = -1;
    unsigned long arg[4] = {0};
    pDewbInfo->mDeId = 0;

    struct disp_device_config conf;
    memset(&conf, 0, sizeof(conf));
    conf.type = DISP_OUTPUT_TYPE_NONE; // set DISP_OUTPUT_TYPE_NONE to reset DE

    arg[0] = 0; // pDewbInfo->mDeId;
    arg[1] = (unsigned long)&conf;
    ret = ioctl(pDewbInfo->mDeFd, DISP_DEVICE_SET_CONFIG, (void *)arg);
    if (ret != 0) {
        loge("de set config err %d", ret);
    }

    return 0;
}

static int de_set_device_config(SampleDewbInfo *pDewbInfo)
{
    int ret = -1;
    unsigned long arg[4] = {0};
    pDewbInfo->mDeId = 0;

    struct disp_device_config conf;
    memset(&conf, 0, sizeof(conf));
    conf.type = DISP_OUTPUT_TYPE_RTWB;
    conf.mode = DISP_TV_MOD_480P;
    conf.timing.x_res = pDewbInfo->mOutputInfo.mWidth;
    conf.timing.y_res = pDewbInfo->mOutputInfo.mHeight;
    conf.timing.frame_period = 16666667; //60hz
    conf.format = DISP_CSC_TYPE_RGB; //pDewbInfo->mOutputInfo.ePixFmtTpye; // output format type: DISP_CSC_TYPE_RGB
    conf.cs = DISP_BT709;
    conf.bits = DISP_DATA_8BITS;
    conf.eotf = DISP_EOTF_GAMMA22;
    conf.range = DISP_COLOR_RANGE_16_235;
    conf.dvi_hdmi = DISP_HDMI;
    conf.scan = DISP_SCANINFO_NO_DATA;

    arg[0] = 0; //pDewbInfo->mDeId;
    arg[1] = (unsigned long)&conf;
    ret = ioctl(pDewbInfo->mDeFd, DISP_DEVICE_SET_CONFIG, (void*)arg);
    if (ret != 0) {
        loge("de set config err %d", ret);
    }

    return ret;
}

static int de_capture_start(SampleDewbInfo *pDewbInfo)
{
    int ret = -1;
    unsigned long arg[4] = {0};
    struct disp_capture_init_info capture_info;
    capture_info.port = DISP_CAPTURE_AFTER_DEP;

    arg[0] = 0; //pDewbInfo->mDeId;
    arg[1] = (unsigned long)&capture_info;
    ret = ioctl(pDewbInfo->mDeFd, DISP_CAPTURE_START, (void *)arg);
    if (ret != 0) {
        loge("de capture start err %d", ret);
        return -1;
    }

    return ret;
}

static int de_capture_stop(SampleDewbInfo *pDewbInfo)
{
    int ret = -1;
    unsigned long arg[4] = {0};

    arg[0] = 0; //pDewbInfo->mDeId;
    ret = ioctl(pDewbInfo->mDeFd, DISP_CAPTURE_STOP, (void *)arg);
    if (ret != 0) {
        loge("de capture stop err %d", ret);
        return -1;
    }

    return ret;
}

static int setup_input_config(SampleDewbInfo *pDewbInfo, \
                            struct disp_layer_config2 *config, \
                            struct disp_capture_info2 *info2)
{
    long long input_h = ((long long)pDewbInfo->mInputInfo.mHeight) << 32;
    long long input_w = ((long long)pDewbInfo->mInputInfo.mWidth) << 32;

    config->channel                 = 0;
    config->layer_id                = 0;
    config->enable                  = 1;
    config->info.mode               = LAYER_MODE_BUFFER;
    config->info.zorder             = 0;
    config->info.alpha_mode         = 2;    // global pixel alpha
    config->info.alpha_value        = 0xff; // global pixel alpha
    config->info.screen_win.x       = 0;
    config->info.screen_win.y       = 0;
    config->info.screen_win.width   = pDewbInfo->mOutputInfo.mWidth;
    config->info.screen_win.height  = pDewbInfo->mOutputInfo.mHeight;
    config->info.fb.crop.y          = 0;
    config->info.fb.crop.x          = 0;
    config->info.fb.crop.height     = input_h; // 定点小数。高32bit 为整数，低32bit 为小数
    config->info.fb.crop.width      = input_w; // 定点小数。高32bit 为整数，低32bit 为小数
    config->info.fb.size[0].width   = pDewbInfo->mInputInfo.mWidth;
    config->info.fb.size[0].height  = pDewbInfo->mInputInfo.mHeight;
    config->info.fb.fd              = pDewbInfo->mInputInfo.mIonFd; // layer->input.fb.fd;
    config->info.fb.eotf            = DISP_EOTF_UNDEF;
    config->info.fb.format          = DISP_FORMAT_YUV420_SP_UVUV; //pDewbInfo->mInputInfo.ePixFmt; //DISP_FORMAT_RGB_888; // DISP_FORMAT_YUV420_P
    config->info.fb.align[0]        = 4;

    if (config->info.fb.format == DISP_FORMAT_YUV420_P
            || config->info.fb.format == DISP_FORMAT_YUV420_SP_UVUV
            || config->info.fb.format == DISP_FORMAT_YUV420_SP_VUVU
            || config->info.fb.format == DISP_FORMAT_YUV420_SP_UVUV_10BIT
            || config->info.fb.format == DISP_FORMAT_YUV420_SP_VUVU_10BIT) {
        config->info.fb.size[1].width       = pDewbInfo->mInputInfo.mWidth / 2;
        config->info.fb.size[1].height      = pDewbInfo->mInputInfo.mHeight / 2;
        config->info.fb.size[2].width       = pDewbInfo->mInputInfo.mWidth / 2;
        config->info.fb.size[2].height      = pDewbInfo->mInputInfo.mHeight / 2;
    } else if (config->info.fb.format == DISP_FORMAT_YUV444_P) {
        config->info.fb.size[1].width       = pDewbInfo->mInputInfo.mWidth;
        config->info.fb.size[1].height      = pDewbInfo->mInputInfo.mHeight;
        config->info.fb.size[2].width       = pDewbInfo->mInputInfo.mWidth;
        config->info.fb.size[2].height      = pDewbInfo->mInputInfo.mHeight;
    } else if (config->info.fb.format == DISP_FORMAT_YUV422_P
            || config->info.fb.format == DISP_FORMAT_YUV422_SP_UVUV
            || config->info.fb.format == DISP_FORMAT_YUV422_SP_VUVU
            || config->info.fb.format == DISP_FORMAT_YUV422_SP_UVUV_10BIT
            || config->info.fb.format == DISP_FORMAT_YUV422_SP_VUVU_10BIT) {
        config->info.fb.size[1].width    = config->info.fb.size[0].width / 2;
        config->info.fb.size[1].height   = config->info.fb.size[0].height;
        config->info.fb.size[2].width    = config->info.fb.size[0].width / 2;
        config->info.fb.size[2].height   = config->info.fb.size[0].height;
    } else if (config->info.fb.format == DISP_FORMAT_YUV411_P
                    || config->info.fb.format == DISP_FORMAT_YUV411_SP_UVUV
                    || config->info.fb.format == DISP_FORMAT_YUV411_SP_VUVU
                    || config->info.fb.format == DISP_FORMAT_YUV411_SP_UVUV_10BIT
                    || config->info.fb.format == DISP_FORMAT_YUV411_SP_VUVU_10BIT) {
        config->info.fb.size[1].width    = config->info.fb.size[0].width / 4;
        config->info.fb.size[1].height   = config->info.fb.size[0].height;
        config->info.fb.size[2].width    = config->info.fb.size[0].width / 4;
        config->info.fb.size[2].height   = config->info.fb.size[0].height;
    } else {
        config->info.fb.size[1].width    = config->info.fb.size[0].width;
        config->info.fb.size[1].height   = config->info.fb.size[0].height;
        config->info.fb.size[2].width    = config->info.fb.size[0].width;
        config->info.fb.size[2].height   = config->info.fb.size[0].height;
    }

    info2->window.x = 0;
    info2->window.y = 0;
    info2->window.width = pDewbInfo->mOutputInfo.mWidth;
    info2->window.height = pDewbInfo->mOutputInfo.mHeight;
    info2->out_frame.size[0].width = pDewbInfo->mOutputInfo.mWidth;
    info2->out_frame.size[0].height = pDewbInfo->mOutputInfo.mHeight;
    info2->out_frame.crop.x = 0;
    info2->out_frame.crop.y = 0;
    info2->out_frame.crop.width = pDewbInfo->mOutputInfo.mWidth;
    info2->out_frame.crop.height = pDewbInfo->mOutputInfo.mHeight;
    info2->out_frame.format = DISP_FORMAT_RGB_888; // pDewbInfo->mOutputInfo.ePixFmt; // DISP_FORMAT_RGB_888
    info2->out_frame.fd = pDewbInfo->mOutputInfo.mIonFd;

    return 0;
}


int De_Init(SampleDewbInfo *pDewbInfo)
{
    int ret = -1;

    /* open disp drv */
    pDewbInfo->mDeFd =  de_device_open();
    if(pDewbInfo->mDeFd < 0)
        return -1;

    /* set rtwb device mode */
    ret = de_set_device_config(pDewbInfo);
    if (ret != 0) {
        loge("de_set_device_config failed, ret=%d", ret);
        de_device_close(pDewbInfo->mDeFd);
        return ret;
    }

    /* capture start */
    ret = de_capture_start(pDewbInfo);
    if ( ret != 0) {
        loge("de capture start failed, ret=%d", ret);
        if (pDewbInfo->mDeFd > 0)
            de_device_close(pDewbInfo->mDeFd);
    }

    return ret;
}

int De_Wb_Scale(SampleDewbInfo *pDewbInfo)
{
    int ret = -1;
    unsigned long arg[4] = {0};

    /* setup input config */
    struct disp_layer_config2 config;
    struct disp_capture_info2 info2;
    memset(&config, 0, sizeof(struct disp_layer_config2));
    memset(&info2, 0, sizeof(struct disp_capture_info2));
    setup_input_config(pDewbInfo,&config, &info2);

    /* capture commit */
    arg[0] = 0; // pDewbInfo->mDeId;    // screen 0
    arg[1] = (unsigned long)&config;
    arg[2] = 1;
    arg[3] = (unsigned long)&info2;
    ret = ioctl(pDewbInfo->mDeFd, DISP_RTWB_COMMIT, (void *)(arg));
    if (ret != 0) {
        loge("de capture rtwb commit err %d", ret);
    }

    return ret;
}

int De_reset(SampleDewbInfo *pDewbInfo)
{
    int ret = -1;

    ret = de_capture_stop(pDewbInfo);
    if ( ret != 0 ) {
        loge("de capture stop failed, ret=%d", ret);
        return ret;
    }

    /*reset DE config*/
    de_reset_device_config(pDewbInfo);
    printf("De_reset, de_reset_device_config");

    /* set rtwb device mode */
    de_set_device_config(pDewbInfo);
    printf("De_reset, de_set_device_config");

    /* capture start */
    ret = de_capture_start(pDewbInfo);
    if ( ret != 0 ) {
        loge("de capture start failed, ret=%d", ret);
        return ret;
    }

    return 0;
}

int De_Deinit(SampleDewbInfo *pDewbInfo)
{
    int ret = -1;

    ret = de_capture_stop(pDewbInfo);
    if ( ret != 0) {
        loge("de capture stop failed, ret=%d", ret);
        de_device_close(pDewbInfo->mDeFd);
        return ret;
    }

    if (pDewbInfo->mDeFd > 0)
        de_device_close(pDewbInfo->mDeFd);

    return ret;
}
