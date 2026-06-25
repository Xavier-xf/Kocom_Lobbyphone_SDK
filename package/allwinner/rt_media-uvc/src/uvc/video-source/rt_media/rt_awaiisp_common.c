#include <sys/time.h>
#include <string.h>
#include <unistd.h>

#include "AW_VideoInput_API.h"
#include "include/rt_awaiisp_common.h"

//#define RT_AWAIISP_COMMON_DUMP_TDM_DATA

#ifdef RT_AWAIISP_COMMON_DUMP_TDM_DATA
#define RT_AWAIISP_COMMON_DUMP_TDM_FRAME_INTERVAL    (10)
#define RT_AWAIISP_COMMON_DUMP_TDM_FRAME_CNT_MIN     (50)
#define RT_AWAIISP_COMMON_DUMP_TDM_FRAME_CNT_MAX     (51)
#endif

/* print level */
#define rt_awaiisp_common_err_print(fmt, arg...)      printf("[RT_AWAIISP_COMMON_ERR]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define rt_awaiisp_common_warn_print(fmt, arg...)     printf("[RT_AWAIISP_COMMON_WRN]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define rt_awaiisp_common_dbg_print(fmt, arg...)      //printf("[RT_AWAIISP_COMMON_DBG]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define rt_awaiisp_common_ver_print(fmt, arg...)      //printf("[RT_AWAIISP_COMMON_VER]%s:%u: " fmt "\n", __FUNCTION__, __LINE__, ##arg)


#define RT_AWAIISP_COMMON_GAMMA_TYPE (3)

typedef struct rt_awaiisp_common_context {
    int isp_id;
    unsigned int tdm_frm_head_len;
    unsigned int tdm_frm_fill_len;
    int npu_lut_enable;
    int aiisp_enable;
} rt_awaiisp_common_context;

static rt_awaiisp_common_context gRtAiIspCommonContext[RT_AWAIISP_COMMON_NUM_MAX];

static unsigned int get_sys_tick_ms()
{
    unsigned int ms = 0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ms = tv.tv_sec*1000 + tv.tv_usec/1000;
    return ms;
}

static unsigned char get_isp_cfg_aiisp_en(int npu_lut_enable, awaiisp_mode mode)
{
    unsigned char ai_isp_en = 0;
    if (0 == npu_lut_enable)
    {
        ai_isp_en = 0;
    }
    else
    {
        if (AWAIISP_MODE_NPU == mode || AWAIISP_MODE_NORMAL_GAMMA == mode)
            ai_isp_en = RT_AWAIISP_COMMON_GAMMA_TYPE;
        else
            ai_isp_en = 0;
    }
    return ai_isp_en;
}

static unsigned char get_isp_cfg_ir_status(awaiisp_mode mode)
{
    unsigned char ir_status = 0;
    if (AWAIISP_MODE_NPU == mode)
        ir_status = 2;
    else
        ir_status = 0;
    return ir_status;
}

static struct rt_awaiisp_common_context *rt_awaiisp_get_common_context(int isp)
{
    if (isp >= RT_AWAIISP_COMMON_NUM_MAX)
    {
        rt_awaiisp_common_err_print("fatal error! invalid ch %d >= %d", isp, RT_AWAIISP_COMMON_NUM_MAX);
        return NULL;
    }
    return &gRtAiIspCommonContext[isp];
}

#ifdef RT_AWAIISP_COMMON_DUMP_TDM_DATA
void rt_awaiisp_dump_tdm_data_once_callback(int isp, int buf_id, unsigned int buf_size, char *flag, int mode, int frm_cnt)
{
    struct rt_awaiisp_common_context *pContext = rt_awaiisp_get_common_context(isp);

    //if (RT_AWAIISP_COMMON_DUMP_TDM_FRAME_CNT_MIN <= frm_cnt && frm_cnt <= RT_AWAIISP_COMMON_DUMP_TDM_FRAME_CNT_MAX)
    if (0 == (frm_cnt % RT_AWAIISP_COMMON_DUMP_TDM_FRAME_INTERVAL))
    {
        if (AWAIISP_MODE_NPU == mode)
        {
            struct vin_isp_tdm_data data;
            memset(&data, 0, sizeof(struct vin_isp_tdm_data));
            data.buf = malloc(buf_size);
            if (data.buf == NULL)
            {
                rt_awaiisp_common_err_print("ch%d malloc size is %d error!", isp, buf_size);
                return;
            }
            data.buf_size = buf_size;
            data.req_buf_id = buf_id;
            AWVideoInput_GetTdmData(isp, &data);
            if (flag)
            {
                char fdstr[128];
                FILE *fp = NULL;
                snprintf(fdstr, 128, "/mnt/extsd/tdm_test/%d_%s.bin", frm_cnt, flag);
                fp = fopen(fdstr, "w");
                fwrite(data.buf + pContext->tdm_frm_fill_len + pContext->tdm_frm_head_len, data.buf_size - pContext->tdm_frm_fill_len - pContext->tdm_frm_head_len, 1, fp);
                rt_awaiisp_common_dbg_print("ch%d save file %d_%s.bin success", isp, frm_cnt, flag);
                fclose(fp);
            }
            if (data.buf)
            {
                free(data.buf);
                data.buf = NULL;
                data.buf_size = 0;
            }
        }
    }
}
#endif

void rt_awaiisp_return_tdm_buffer_callback(struct vin_isp_tdm_event_status *status)
{
    int id = status->dev_id;
    struct rt_awaiisp_common_context *pContext = rt_awaiisp_get_common_context(id);
    pContext->isp_id = id;
    pContext->tdm_frm_fill_len = status->fill_len;
    pContext->tdm_frm_head_len = status->head_len;
    AWVideoInput_ReturnTdmBuf(id, status);
}

int rt_awaiisp_common_set_ulimit_fd(int num)
{
    return awaiisp_set_ulimit_fd(num);
}

int rt_awaiisp_common_enable(int isp, rt_awaiisp_common_config_param *param)
{
    int ret = 0;
    int load_isp_cfg_bin = 0;
    RTIspCtrlAttr isp_ctrl_attr;

    if (NULL == param)
    {
        rt_awaiisp_common_err_print("fatal error! param is NULL! isp=%d", isp);
        return -1;
    }

    struct rt_awaiisp_common_context *pContext = rt_awaiisp_get_common_context(isp);
    if (1 == pContext->aiisp_enable)
    {
        rt_awaiisp_common_warn_print("ch%d aiisp repeatedly enable, please check it.", isp);
        return 0;
    }

    memset(pContext, 0, sizeof(rt_awaiisp_common_context));

    pContext->aiisp_enable = 1;

    param->config.ion_mem_open = 1;

    if ((param->isp_cfg_bin_path) && (strlen(param->isp_cfg_bin_path) != 0))
    {
        if (0 != access(param->isp_cfg_bin_path, F_OK))
        {
            rt_awaiisp_common_warn_print("fatal error! ch%d isp_cfg_bin_path %s is not exist, disable load_isp_cfg_bin", isp, param->isp_cfg_bin_path);
            load_isp_cfg_bin = 0;
        }
        else
        {
            load_isp_cfg_bin = 1;
        }
    }
    else
    {
        load_isp_cfg_bin = 0;
        rt_awaiisp_common_dbg_print("ch%d user disable load_isp_cfg_bin", isp);
    }

    if (strlen(param->config.lut_model_file) != 0)
    {
        pContext->npu_lut_enable = 1;
    }
    else
    {
        pContext->npu_lut_enable = 0;
        rt_awaiisp_common_dbg_print("ch%d user disable npu lut", isp);
    }

    ret |= awaiisp_open(isp, &param->config);

    ret |= awaiisp_start(isp);

    ret |= awaiisp_register_return_tdm_buffer_callback(isp, &rt_awaiisp_return_tdm_buffer_callback);

#ifdef RT_AWAIISP_COMMON_DUMP_TDM_DATA
    ret |= awaiisp_register_dump_tdm_data_once_callback(isp, &rt_awaiisp_dump_tdm_data_once_callback);
    awaiisp_set_dump_g2d_data_once_path(isp, "/mnt/extsd/tdm_test");
#endif

    if (load_isp_cfg_bin)
    {
        memset(&isp_ctrl_attr, 0, sizeof(RTIspCtrlAttr));
        isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_AI_ISP;
        isp_ctrl_attr.isp_attr_cfg.ai_isp_en = get_isp_cfg_aiisp_en(pContext->npu_lut_enable, param->config.mode);
        isp_ctrl_attr.isp_attr_cfg.ai_isp_update = 0;
        rt_awaiisp_common_dbg_print("ch%d set ai_isp_en:%d, ai_isp_update:%d", isp,
            isp_ctrl_attr.isp_attr_cfg.ai_isp_en, isp_ctrl_attr.isp_attr_cfg.ai_isp_update);
        ret |= AWVideoInput_SetIspAttrCfg(isp, &isp_ctrl_attr);

        memset(&isp_ctrl_attr, 0, sizeof(RTIspCtrlAttr));
        isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_READ_BIN_PARAM;
        strncpy(isp_ctrl_attr.isp_attr_cfg.path, param->isp_cfg_bin_path, 100);
        rt_awaiisp_common_dbg_print("ch%d user set isp cfg bin file %s", isp, param->isp_cfg_bin_path);
        ret |= AWVideoInput_SetIspAttrCfg(isp, &isp_ctrl_attr);
    }
    else
    {
        memset(&isp_ctrl_attr, 0, sizeof(RTIspCtrlAttr));
        isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_AI_ISP;
        isp_ctrl_attr.isp_attr_cfg.ai_isp_en = get_isp_cfg_aiisp_en(pContext->npu_lut_enable, param->config.mode);
        isp_ctrl_attr.isp_attr_cfg.ai_isp_update = 1;
        rt_awaiisp_common_dbg_print("ch%d set ai_isp_en:%d, ai_isp_update:%d", isp,
            isp_ctrl_attr.isp_attr_cfg.ai_isp_en, isp_ctrl_attr.isp_attr_cfg.ai_isp_update);
        ret |= AWVideoInput_SetIspAttrCfg(isp, &isp_ctrl_attr);

        memset(&isp_ctrl_attr, 0, sizeof(RTIspCtrlAttr));
        isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_IR_STATUS;
        isp_ctrl_attr.isp_attr_cfg.ir_status = get_isp_cfg_ir_status(param->config.mode);
        rt_awaiisp_common_dbg_print("ch%d set isp ir status %d", isp, isp_ctrl_attr.isp_attr_cfg.ir_status);
        ret |= AWVideoInput_SetIspAttrCfg(isp, &isp_ctrl_attr);
    }

    ret |= AWVideoInput_RegisterTdmBufDoneCallback(isp, &awaiisp_tdm_buffer_process_callback);

    ret |= AWVideoInput_StartProcessTdmBuf(isp);

    return ret;
}

int rt_awaiisp_common_disable(int isp)
{
    int ret = 0;
    struct rt_awaiisp_common_context *pContext = rt_awaiisp_get_common_context(isp);

    if (0 == pContext->aiisp_enable)
    {
        rt_awaiisp_common_warn_print("ch%d aiisp is not enable or repeatedly disable.", isp);
        return 0;
    }

    ret |= AWVideoInput_RegisterTdmBufDoneCallback(isp, NULL);

    ret |= awaiisp_stop(isp);

    ret |= awaiisp_close(isp);

    pContext->aiisp_enable = 0;

    return ret;
}

int rt_awaiisp_common_switch_mode(rt_awaiisp_common_switch_param *param)
{
    int start_time = get_sys_tick_ms();
    int end_time0 = 0, end_time1 = 0, end_time2 = 0, end_time3 = 0, end_time4 = 0;
    int ret = 0;
    RTIspCtrlAttr isp_ctrl_attr;
    int load_isp_cfg_bin = 0;
    int total_aiisp_enable_cnt = 0;

    if (NULL == param)
    {
        rt_awaiisp_common_err_print("fatal error! param is NULL!");
        return -1;
    }

    /* multi camera scenes must be stopped together */
    for (int i = 0; i < RT_AWAIISP_COMMON_NUM_MAX; i++)
    {
        rt_awaiisp_common_switch_channel_param *channel_param = &param->channel_param[i];
        if (0 == channel_param->enable)
            continue;

        int isp = channel_param->isp;
        struct rt_awaiisp_common_context *pContext = rt_awaiisp_get_common_context(isp);
        total_aiisp_enable_cnt += pContext->aiisp_enable;

        int vipp = channel_param->vipp;
        rt_awaiisp_common_switch_case switch_case = channel_param->switch_case;
        enum set_bit_width bitwidth = (AWAIISP_MODE_NPU == channel_param->config.mode) ? B10_TO_B8 : B8_TO_B10;

        if (RT_AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case)
        {
            rt_awaiisp_common_ver_print("vipp %d set bitwidth %d for stop", vipp, bitwidth);
            ret |= AWVideoInput_SetInputBitWidthStop(vipp, bitwidth);
        }
    }

    if (0 == total_aiisp_enable_cnt)
    {
        rt_awaiisp_common_warn_print("aiisp all channels are not enable, please check.");
        return -1;
    }

    end_time0 = get_sys_tick_ms();

    for (int i = 0; i < RT_AWAIISP_COMMON_NUM_MAX; i++)
    {
        rt_awaiisp_common_switch_channel_param *channel_param = &param->channel_param[i];
        if (0 == channel_param->enable)
            continue;

        int isp = channel_param->isp;
        int vipp = channel_param->vipp;
        rt_awaiisp_common_switch_case switch_case = channel_param->switch_case;
        struct rt_awaiisp_common_context *pContext = rt_awaiisp_get_common_context(isp);

        if (pContext->aiisp_enable)
        {
            channel_param->config.switch_fast = (RT_AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case) ? 1 : 0;
            ret |= awaiisp_switch_mode(isp, &channel_param->config);
        }

        end_time1 = get_sys_tick_ms();

        if (RT_AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case)
        {
            unsigned int drop_num = channel_param->drop_frame_num;
            if (0 < drop_num)
            {
                rt_awaiisp_common_dbg_print("vipp %d set drop_num %d", vipp, drop_num);
                ret |= AWVideoInput_SetTdmDropFrame(vipp, drop_num);
            }
        }

        end_time2 = get_sys_tick_ms();

        if ((channel_param->isp_cfg_bin_path) && (strlen(channel_param->isp_cfg_bin_path) != 0))
        {
            if (0 != access(channel_param->isp_cfg_bin_path, F_OK))
            {
                rt_awaiisp_common_warn_print("fatal error! ch%d isp_cfg_bin_path %s is not exist, disable load_isp_cfg_bin",
                    isp, channel_param->isp_cfg_bin_path);
                load_isp_cfg_bin = 0;
            }
            else
            {
                load_isp_cfg_bin = 1;
            }
        }
        else
        {
            load_isp_cfg_bin = 0;
            rt_awaiisp_common_dbg_print("ch%d user disable load_isp_cfg_bin", isp);
        }

        if (load_isp_cfg_bin)
        {
            if (pContext->aiisp_enable)
            {
                memset(&isp_ctrl_attr, 0, sizeof(RTIspCtrlAttr));
                isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_AI_ISP;
                isp_ctrl_attr.isp_attr_cfg.ai_isp_en = get_isp_cfg_aiisp_en(pContext->npu_lut_enable, channel_param->config.mode);
                isp_ctrl_attr.isp_attr_cfg.ai_isp_update = 0;
                rt_awaiisp_common_dbg_print("ch%d set ai_isp_en:%d, ai_isp_update:%d", isp,
                    isp_ctrl_attr.isp_attr_cfg.ai_isp_en, isp_ctrl_attr.isp_attr_cfg.ai_isp_update);
                ret |= AWVideoInput_SetIspAttrCfg(isp, &isp_ctrl_attr);
            }

            if (pContext->aiisp_enable)
            {
                memset(&isp_ctrl_attr, 0, sizeof(RTIspCtrlAttr));
                isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_READ_BIN_PARAM;
                strncpy(isp_ctrl_attr.isp_attr_cfg.path, channel_param->isp_cfg_bin_path, 100);
                rt_awaiisp_common_dbg_print("ch%d user set isp cfg bin file %s", isp, channel_param->isp_cfg_bin_path);
                ret |= AWVideoInput_SetIspAttrCfg(isp, &isp_ctrl_attr);
            }
        }
        else
        {
            if (pContext->aiisp_enable)
            {
                memset(&isp_ctrl_attr, 0, sizeof(RTIspCtrlAttr));
                isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_AI_ISP;
                isp_ctrl_attr.isp_attr_cfg.ai_isp_en = get_isp_cfg_aiisp_en(pContext->npu_lut_enable, channel_param->config.mode);
                isp_ctrl_attr.isp_attr_cfg.ai_isp_update = 1;
                rt_awaiisp_common_dbg_print("ch%d set ai_isp_en:%d, ai_isp_update:%d", isp,
                    isp_ctrl_attr.isp_attr_cfg.ai_isp_en, isp_ctrl_attr.isp_attr_cfg.ai_isp_update);
                ret |= AWVideoInput_SetIspAttrCfg(isp, &isp_ctrl_attr);
            }

            if (pContext->aiisp_enable)
            {
                memset(&isp_ctrl_attr, 0, sizeof(RTIspCtrlAttr));
                isp_ctrl_attr.isp_attr_cfg.cfg_id = ISP_CTRL_IR_STATUS;
                isp_ctrl_attr.isp_attr_cfg.ir_status = get_isp_cfg_ir_status(channel_param->config.mode);
                rt_awaiisp_common_dbg_print("ch%d set isp ir status %d", isp, isp_ctrl_attr.isp_attr_cfg.ir_status);
                ret |= AWVideoInput_SetIspAttrCfg(isp, &isp_ctrl_attr);
            }
        }
    }

    end_time3 = get_sys_tick_ms();

    /* multi camera scenes must be start together */
    for (int i = 0; i < RT_AWAIISP_COMMON_NUM_MAX; i++)
    {
        rt_awaiisp_common_switch_channel_param *channel_param = &param->channel_param[i];
        if (0 == channel_param->enable)
            continue;

        int vipp = channel_param->vipp;
        rt_awaiisp_common_switch_case switch_case = channel_param->switch_case;
        enum set_bit_width bitwidth = (AWAIISP_MODE_NPU == channel_param->config.mode) ? B10_TO_B8 : B8_TO_B10;

        if (RT_AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case)
        {
            rt_awaiisp_common_ver_print("vipp %d set bitwidth %d for start", vipp, bitwidth);
            ret |= AWVideoInput_SetInputBitWidthStart(vipp, bitwidth);
        }
    }

    end_time4 = get_sys_tick_ms();

    rt_awaiisp_common_warn_print("cost %d ms, stop:%d, swtich:%d, drop:%d, ispcfg:%d, start:%d",
        end_time4 - start_time, end_time0 - start_time, end_time1 - end_time0, end_time2 - end_time1,
        end_time3 - end_time2, end_time4 - end_time3);

    return ret;
}
