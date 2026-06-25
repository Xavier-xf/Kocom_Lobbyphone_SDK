#include <mpi_isp.h>
#include <mpi_vi.h>
#include "awaiisp_common.h"

//#define AWAIISP_COMMON_DUMP_TDM_DATA

#ifdef AWAIISP_COMMON_DUMP_TDM_DATA
#define AWAIISP_COMMON_DUMP_TDM_FRAME_INTERVAL    (10)
#define AWAIISP_COMMON_DUMP_TDM_FRAME_CNT_MIN     (50)
#define AWAIISP_COMMON_DUMP_TDM_FRAME_CNT_MAX     (51)
#endif

/* print level */
#define awaiisp_common_err_print(fmt, arg...)      printf("[AWAIISP_COMMON_ERR]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define awaiisp_common_warn_print(fmt, arg...)     printf("[AWAIISP_COMMON_WRN]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define awaiisp_common_dbg_print(fmt, arg...)      //printf("[AWAIISP_COMMON_DBG]%s:%d: " fmt "\n", __FUNCTION__, __LINE__, ##arg)
#define awaiisp_common_ver_print(fmt, arg...)      //printf("[AWAIISP_COMMON_VER]%s:%u: " fmt "\n", __FUNCTION__, __LINE__, ##arg)


#define AWAIISP_COMMON_GAMMA_TYPE (3)

typedef struct awaiisp_common_context {
    int isp_id;
    unsigned int tdm_frm_head_len;
    unsigned int tdm_frm_fill_len;
    int npu_lut_enable;
    int aiisp_enable;
    int isp_ai_isp_en;
} awaiisp_common_context;

static awaiisp_common_context gAiIspCommonContext[AWAIISP_COMMON_NUM_MAX];

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
            ai_isp_en = AWAIISP_COMMON_GAMMA_TYPE;
        else
            ai_isp_en = 0;
    }
    return ai_isp_en;
}

static unsigned char get_isp_cfg_ir_status(awaiisp_mode mode)
{
    unsigned char ir_status = 0;
    if (AWAIISP_MODE_NPU == mode)
        ir_status = IR_NORMAL_CFG;
    else
        ir_status = NORMAL_CFG;
    return ir_status;
}

static struct awaiisp_common_context *awaiisp_get_common_context(int isp)
{
    if (isp >= AWAIISP_COMMON_NUM_MAX)
    {
        awaiisp_common_err_print("fatal error! invalid ch %d >= %d", isp, AWAIISP_COMMON_NUM_MAX);
        return NULL;
    }
    return &gAiIspCommonContext[isp];
}

#ifdef AWAIISP_COMMON_DUMP_TDM_DATA
void awaiisp_dump_tdm_data_once_callback(int isp, int buf_id, unsigned int buf_size, char *flag, int mode, int frm_cnt)
{
    struct awaiisp_common_context *pContext = awaiisp_get_common_context(isp);

    //if (AWAIISP_COMMON_DUMP_TDM_FRAME_CNT_MIN <= frm_cnt && frm_cnt <= AWAIISP_COMMON_DUMP_TDM_FRAME_CNT_MAX)
    if (0 == (frm_cnt % AWAIISP_COMMON_DUMP_TDM_FRAME_INTERVAL))
    {
        if (AWAIISP_MODE_NPU == mode)
        {
            struct vin_isp_tdm_data data;
            memset(&data, 0, sizeof(struct vin_isp_tdm_data));
            data.buf = malloc(buf_size);
            if (data.buf == NULL)
            {
                awaiisp_common_err_print("ch%d malloc size is %d error!", isp, buf_size);
                return;
            }
            data.buf_size = buf_size;
            data.req_buf_id = buf_id;
            AW_MPI_ISP_GetTdmData(isp, &data);
            if (flag)
            {
                char fdstr[128];
                FILE *fp = NULL;
                snprintf(fdstr, 128, "/mnt/extsd/tdm_test/%d_%s.bin", frm_cnt, flag);
                fp = fopen(fdstr, "w");
                fwrite(data.buf + pContext->tdm_frm_fill_len + pContext->tdm_frm_head_len, data.buf_size - pContext->tdm_frm_fill_len - pContext->tdm_frm_head_len, 1, fp);
                awaiisp_common_dbg_print("ch%d save file %d_%s.bin success", isp, frm_cnt, flag);
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

void awaiisp_return_tdm_buffer_callback(struct vin_isp_tdm_event_status *status)
{
    int id = status->dev_id;
    struct awaiisp_common_context *pContext = awaiisp_get_common_context(id);
    pContext->isp_id = id;
    pContext->tdm_frm_fill_len = status->fill_len;
    pContext->tdm_frm_head_len = status->head_len;
    AW_MPI_ISP_ReturnTdmBuf(id, status);
}

int awaiisp_common_set_ulimit_fd(int num)
{
    return awaiisp_set_ulimit_fd(num);
}

int awaiisp_common_enable(int isp, awaiisp_common_config_param *param)
{
    int ret = 0;
    int load_isp_cfg_bin = 0;

    struct awaiisp_common_context *pContext = awaiisp_get_common_context(isp);
    if (1 == pContext->aiisp_enable)
    {
        awaiisp_common_warn_print("ch%d aiisp repeatedly enable, please check it.", isp);
        return 0;
    }

    memset(pContext, 0, sizeof(awaiisp_common_context));

    pContext->aiisp_enable = 1;

    param->config.ion_mem_open = 1;

    if ((param->isp_cfg_bin_path) && (strlen(param->isp_cfg_bin_path) != 0))
    {
        if (0 != access(param->isp_cfg_bin_path, F_OK))
        {
            awaiisp_common_warn_print("fatal error! ch%d isp_cfg_bin_path %s is not exist, disable load_isp_cfg_bin", isp, param->isp_cfg_bin_path);
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
        awaiisp_common_dbg_print("ch%d user disable load_isp_cfg_bin", isp);
    }

    if (strlen(param->config.lut_model_file) != 0)
    {
        pContext->npu_lut_enable = 1;
    }
    else
    {
        pContext->npu_lut_enable = 0;
        awaiisp_common_dbg_print("ch%d user disable npu lut", isp);
    }

    ret |= awaiisp_open(isp, &param->config);

    ret |= awaiisp_start(isp);

    ret |= awaiisp_register_return_tdm_buffer_callback(isp, &awaiisp_return_tdm_buffer_callback);

#ifdef AWAIISP_COMMON_DUMP_TDM_DATA
    ret |= awaiisp_register_dump_tdm_data_once_callback(isp, &awaiisp_dump_tdm_data_once_callback);
    awaiisp_set_dump_g2d_data_once_path(isp, "/mnt/extsd/tdm_test");
#endif

#if (MPPCFG_SUPPORT_FASTBOOT == 0)
    isp_ai_isp_info ai_isp_info;
    memset(&ai_isp_info, 0, sizeof(isp_ai_isp_info));
    ai_isp_info.ai_isp_en = get_isp_cfg_aiisp_en(pContext->npu_lut_enable, param->config.mode);
    ai_isp_info.ai_isp_update = 0;
    ret |= AW_MPI_ISP_SetAiIsp(isp, &ai_isp_info);

    if (load_isp_cfg_bin)
    {
        awaiisp_common_dbg_print("ch%d user set isp cfg bin file %s", isp, param->isp_cfg_bin_path);
        ISP_CFG_BIN_MODE mode_flag = ai_isp_info.ai_isp_en ? LINEAR_AIISP_CFG : LINEAR_COLOR_CFG;
        awaiisp_common_dbg_print("ch%d user set isp cfg %d", isp, mode_flag);
        ret |= AW_MPI_ISP_ReadIspBin(isp, mode_flag, param->isp_cfg_bin_path);
    }
#else
    isp_ai_isp_info ai_isp_info;
    memset(&ai_isp_info, 0, sizeof(isp_ai_isp_info));
    ai_isp_info.ai_isp_en = get_isp_cfg_aiisp_en(pContext->npu_lut_enable, param->config.mode);
    if (pContext->isp_ai_isp_en != ai_isp_info.ai_isp_en)
    {
        ai_isp_info.ai_isp_update = 1;
    }
    pContext->isp_ai_isp_en = ai_isp_info.ai_isp_en;
    ret |= AW_MPI_ISP_SetAiIsp(isp, &ai_isp_info);

    ISP_CFG_MODE cfg_mode = get_isp_cfg_ir_status(param->config.mode);
    awaiisp_common_dbg_print("ch%d user set isp cfg mode %d", isp, cfg_mode);
    ret |= AW_MPI_ISP_SwitchIspConfig(isp, cfg_mode);
#endif

    ret |= AW_MPI_ISP_RegisterTdmBufDoneCallback(isp, &awaiisp_tdm_buffer_process_callback);

    return ret;
}

int awaiisp_common_disable(int isp)
{
    int ret = 0;
    struct awaiisp_common_context *pContext = awaiisp_get_common_context(isp);

    if (0 == pContext->aiisp_enable)
    {
        awaiisp_common_warn_print("ch%d aiisp is not enable or repeatedly disable.", isp);
        return 0;
    }

    ret |= AW_MPI_ISP_RegisterTdmBufDoneCallback(isp, NULL);

    ret |= awaiisp_stop(isp);

    ret |= awaiisp_close(isp);

    pContext->aiisp_enable = 0;

    return ret;
}

int awaiisp_common_switch_mode(awaiisp_common_switch_param *param)
{
    int start_time = get_sys_tick_ms();
    int end_time0 = 0, end_time1 = 0, end_time2 = 0, end_time3 = 0, end_time4 = 0;
    int ret = 0;
    int load_isp_cfg_bin = 0;
    int total_aiisp_enable_cnt = 0;

    if (NULL == param)
    {
        awaiisp_common_err_print("fatal error! param is NULL!");
        return -1;
    }

    /* multi camera scenes must be stopped together */
    for (int i = 0; i < AWAIISP_COMMON_NUM_MAX; i++)
    {
        awaiisp_common_switch_channel_param *channel_param = &param->channel_param[i];
        if (0 == channel_param->enable)
            continue;

        int isp = channel_param->isp;
        struct awaiisp_common_context *pContext = awaiisp_get_common_context(isp);
        total_aiisp_enable_cnt += pContext->aiisp_enable;

        int vipp = channel_param->vipp;
        awaiisp_common_switch_case switch_case = channel_param->switch_case;
        enum set_bit_width bitwidth = (AWAIISP_MODE_NPU == channel_param->config.mode) ? B10_TO_B8 : B8_TO_B10;

        if (AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case)
        {
            awaiisp_common_ver_print("vipp %d set bitwidth %d for stop", vipp, bitwidth);
            ret |= AW_MPI_VI_SetInputBitWidthStop(vipp, bitwidth);
        }
    }

    if (0 == total_aiisp_enable_cnt)
    {
        awaiisp_common_warn_print("aiisp all channels are not enable, please check.");
        return -1;
    }

    end_time0 = get_sys_tick_ms();

    for (int i = 0; i < AWAIISP_COMMON_NUM_MAX; i++)
    {
        awaiisp_common_switch_channel_param *channel_param = &param->channel_param[i];
        if (0 == channel_param->enable)
            continue;

        int isp = channel_param->isp;
        int vipp = channel_param->vipp;
        awaiisp_common_switch_case switch_case = channel_param->switch_case;
        struct awaiisp_common_context *pContext = awaiisp_get_common_context(isp);

        if (pContext->aiisp_enable)
        {
            channel_param->config.switch_fast = (AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case) ? 1 : 0;
            ret |= awaiisp_switch_mode(isp, &channel_param->config);
        }

        end_time1 = get_sys_tick_ms();

        if (AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case)
        {
            unsigned int drop_num = channel_param->drop_frame_num;
            if (0 < drop_num)
            {
                awaiisp_common_dbg_print("vipp%d set drop_num %d", vipp, drop_num);
                ret |= AW_MPI_VI_SetDropFrame(vipp, drop_num);
            }
        }

        end_time2 = get_sys_tick_ms();

        if ((channel_param->isp_cfg_bin_path) && (strlen(channel_param->isp_cfg_bin_path) != 0))
        {
            if (0 != access(channel_param->isp_cfg_bin_path, F_OK))
            {
                awaiisp_common_warn_print("fatal error! ch%d isp_cfg_bin_path %s is not exist, disable load_isp_cfg_bin",
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
            awaiisp_common_dbg_print("ch%d user disable load_isp_cfg_bin", isp);
        }

#if (MPPCFG_SUPPORT_FASTBOOT == 0)
        if (pContext->aiisp_enable)
        {
            isp_ai_isp_info ai_isp_info;
            memset(&ai_isp_info, 0, sizeof(isp_ai_isp_info));
            ai_isp_info.ai_isp_en = get_isp_cfg_aiisp_en(pContext->npu_lut_enable, channel_param->config.mode);
            ai_isp_info.ai_isp_update = 0;
            ret |= AW_MPI_ISP_SetAiIsp(isp, &ai_isp_info);

            if (load_isp_cfg_bin)
            {
                ISP_CFG_BIN_MODE mode_flag = ai_isp_info.ai_isp_en ? LINEAR_AIISP_CFG : LINEAR_COLOR_CFG;
                awaiisp_common_dbg_print("ch%d user set isp cfg %d", isp, mode_flag);
                ret |= AW_MPI_ISP_ReadIspBin(isp, mode_flag, channel_param->isp_cfg_bin_path);
            }
        }
#else
        if (pContext->aiisp_enable)
        {
            isp_ai_isp_info ai_isp_info;
            memset(&ai_isp_info, 0, sizeof(isp_ai_isp_info));
            ai_isp_info.ai_isp_en = get_isp_cfg_aiisp_en(pContext->npu_lut_enable, channel_param->config.mode);
            if (pContext->isp_ai_isp_en != ai_isp_info.ai_isp_en)
            {
                ai_isp_info.ai_isp_update = 1;
            }
            pContext->isp_ai_isp_en = ai_isp_info.ai_isp_en;
            ret |= AW_MPI_ISP_SetAiIsp(isp, &ai_isp_info);

            ISP_CFG_MODE cfg_mode = get_isp_cfg_ir_status(channel_param->config.mode);
            awaiisp_common_dbg_print("ch%d user set isp cfg mode %d", isp, cfg_mode);
            ret |= AW_MPI_ISP_SwitchIspConfig(isp, cfg_mode);
        }
#endif
    }

    end_time3 = get_sys_tick_ms();

    /* multi camera scenes must be start together */
    for (int i = 0; i < AWAIISP_COMMON_NUM_MAX; i++)
    {
        awaiisp_common_switch_channel_param *channel_param = &param->channel_param[i];
        if (0 == channel_param->enable)
            continue;

        int vipp = channel_param->vipp;
        awaiisp_common_switch_case switch_case = channel_param->switch_case;
        enum set_bit_width bitwidth = (AWAIISP_MODE_NPU == channel_param->config.mode) ? B10_TO_B8 : B8_TO_B10;

        if (AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case)
        {
            awaiisp_common_ver_print("vipp %d set bitwidth %d for start", vipp, bitwidth);
            ret |= AW_MPI_VI_SetInputBitWidthStart(vipp, bitwidth);
        }
    }

    end_time4 = get_sys_tick_ms();

    awaiisp_common_warn_print("cost %d ms, stop:%d, swtich:%d, drop:%d, ispcfg:%d, start:%d",
        end_time4 - start_time, end_time0 - start_time, end_time1 - end_time0, end_time2 - end_time1,
        end_time3 - end_time2, end_time4 - end_time3);

    return ret;
}

