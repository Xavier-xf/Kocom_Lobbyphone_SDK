#ifdef __linux__
#include <mpi_isp.h>
#include <mpi_vi.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <hal_timer.h>
#include <machine/endian.h>
#include <sys/prctl.h>
#include "awaiisp_rp.h"
#endif
#include "awaiisp_common.h"

#ifdef __linux__
//#define AWAIISP_COMMON_DUMP_TDM_DATA
#ifdef AWAIISP_COMMON_DUMP_TDM_DATA
#define AWAIISP_COMMON_DUMP_TDM_FRAME_INTERVAL    (10)
#define AWAIISP_COMMON_DUMP_TDM_FRAME_CNT_MIN     (50)
#define AWAIISP_COMMON_DUMP_TDM_FRAME_CNT_MAX     (51)
#endif
#endif

#define AWAIISP_COMMON_NUM_MAX  (4)
#define AWAIISP_COMMON_GAMMA_TYPE (3)

typedef struct awaiisp_common_context {
    int isp_id;
    unsigned int tdm_frm_head_len;
    unsigned int tdm_frm_fill_len;
    int npu_lut_enable;
    int aiisp_enable;
    int rt_memheap_init_flag;
} awaiisp_common_context;

#ifndef __linux__
struct vin_isp_tdm_event_status {
	__u8 dev_id;
	void *iommu_buf;
	__u32 buf_size;
	__u8 buf_id;
	__u32 head_len;
	__u32 fill_len;
};
#endif

static awaiisp_common_context gAiIspCommonContext[AWAIISP_COMMON_NUM_MAX];

static unsigned int getSysTickMs(void)
{
    unsigned int ms = 0;
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ms = tv.tv_sec*1000 + tv.tv_usec/1000;
    return ms;
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

#ifdef __linux__
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
#endif

void awaiisp_return_tdm_buffer_callback(struct vin_isp_tdm_event_status *status)
{
    int id = status->dev_id;
    struct awaiisp_common_context *pContext = awaiisp_get_common_context(id);
    pContext->isp_id = id;
    pContext->tdm_frm_fill_len = status->fill_len;
    pContext->tdm_frm_head_len = status->head_len;

#ifdef __linux__
    AW_MPI_ISP_ReturnTdmBuf(id, status);
#else
    isp_return_tdmbuffer(id, status->buf_id);
#endif
}

int awaiisp_common_set_ulimit_fd(int num)
{
    return awaiisp_set_ulimit_fd(num);
}

#ifndef __linux__
void awaiisp_common_rt_memheap_init(int isp)
{
    struct awaiisp_common_context *pContext = awaiisp_get_common_context(isp);
    int i;

    if (pContext->rt_memheap_init_flag) {
        awaiisp_common_dbg_print("It will not do rt_memheap_init anymore\n");
        return;
    }

    awaiisp_common_dbg_print("rt_memheap_init aiisp_demo_mempool\n");
    simple_iommu_map_region(0x48200000, AIISP_MEMRESERVE_DTS, AIISP_MEMRESERVE_SIZE);
	rt_memheap_init(&aiisp_demo_mempool, "aiisp_demo-mempool", (void *)AIISP_MEMRESERVE, AIISP_MEMRESERVE_SIZE);
    /* finish rt_memheap_init and update all channel's flag */
    for (i = 0; i < AWAIISP_COMMON_NUM_MAX; i++) {
        gAiIspCommonContext[i].rt_memheap_init_flag = 1;
    }
}
#endif

int awaiisp_common_enable(int isp, awaiisp_common_config_param *param)
{
    int ret = 0;

    struct awaiisp_common_context *pContext = awaiisp_get_common_context(isp);
    if (1 == pContext->aiisp_enable)
    {
        awaiisp_common_warn_print("ch%d aiisp repeatedly enable, please check it.", isp);
        return 0;
    }

    if (!pContext->rt_memheap_init_flag) {
        awaiisp_common_warn_print("ch%d rt_memheap_init_flag = %d, it will init it", isp, pContext->rt_memheap_init_flag);
        awaiisp_common_rt_memheap_init(isp);
    }

    awaiisp_common_dbg_print("isp%d ready to enable aiisp\n", isp);

    memset(pContext, 0, sizeof(awaiisp_common_context));

    pContext->aiisp_enable = 1;

#ifdef __linux__
    param->config.ion_mem_open = 1;
#else
    param->config.ion_mem_open = 0;
#endif

    ret |= awaiisp_open(isp, &param->config);

    ret |= awaiisp_start(isp);

    ret |= awaiisp_register_return_tdm_buffer_callback(isp, &awaiisp_return_tdm_buffer_callback);

#ifdef __linux__
#ifdef AWAIISP_COMMON_DUMP_TDM_DATA
    ret |= awaiisp_register_dump_tdm_data_once_callback(isp, &awaiisp_dump_tdm_data_once_callback);
    awaiisp_set_dump_g2d_data_once_path(isp, "/mnt/extsd/tdm_test");
#endif

    isp_ai_isp_info ai_isp_info;
    memset(&ai_isp_info, 0, sizeof(isp_ai_isp_info));
    if (strlen(param->config.lut_model_file) != 0)
    {
        ai_isp_info.ai_isp_en = AWAIISP_COMMON_GAMMA_TYPE;
        pContext->npu_lut_enable = 1;
    }
    else
    {
        awaiisp_common_dbg_print("ch%d user set lut nbg file path is NULL, set isp ai_isp_en=0", isp);
        ai_isp_info.ai_isp_en = 0;
        pContext->npu_lut_enable = 0;
    }
    ret |= AW_MPI_ISP_SetAiIsp(isp, &ai_isp_info);

    if (param->isp_cfg_bin_path && strlen(param->isp_cfg_bin_path) != 0)
    {
        if (0 != access(param->isp_cfg_bin_path, F_OK))
        {
            awaiisp_common_err_print("fatal error! ch%d isp_cfg_bin_path %s is not exist!", isp, param->isp_cfg_bin_path);
        }
        awaiisp_common_dbg_print("ch%d user set isp cfg bin file %s", isp, param->isp_cfg_bin_path);
        ISP_CFG_BIN_MODE mode_flag = LINEAR_COLOR_CFG;
        ret |= AW_MPI_ISP_ReadIspBin(isp, mode_flag, param->isp_cfg_bin_path);
    }
#endif

#ifdef __linux__
    ret |= AW_MPI_ISP_RegisterTdmBufDoneCallback(isp, &awaiisp_tdm_buffer_process_callback);
#else
    isp_register_tdmbuffer_done_callback(isp, &awaiisp_tdm_buffer_process_callback);
#endif

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

    awaiisp_common_dbg_print("isp%d ready to disable aiisp\n", isp);

#ifdef __linux__
    ret |= AW_MPI_ISP_RegisterTdmBufDoneCallback(isp, NULL);
#else
    isp_register_tdmbuffer_done_callback(isp, &awaiisp_tdm_buffer_process_callback);
#endif
    ret |= awaiisp_stop(isp);

    ret |= awaiisp_close(isp);

    pContext->aiisp_enable = 0;

    int i;
    for (i = 0; i < AWAIISP_COMMON_NUM_MAX; i++) {
        gAiIspCommonContext[i].rt_memheap_init_flag = 1;
        awaiisp_common_dbg_print("awaiisp_common_disable gAiIspCommonContext[%d].rt_memheap_init_flag = %d\n", i, gAiIspCommonContext[i].rt_memheap_init_flag);
    }

    return ret;
}

int awaiisp_common_switch_mode(int isp, awaiisp_common_switch_param *param)
{
    int start_time = getSysTickMs();
    int end_time0 = 0, end_time1 = 0, end_time2 = 0, end_time3 = 0, end_time4 = 0, end_time5 = 0;
    int ret = 0;
    struct awaiisp_common_context *pContext = awaiisp_get_common_context(isp);
    if (0 == pContext->aiisp_enable)
    {
        awaiisp_common_warn_print("ch%d aiisp is not enable, please check it.", isp);
        return -1;
    }

#ifdef __linux__
    if ((NULL == param->isp_cfg_bin_path) || (param->isp_cfg_bin_path && strlen(param->isp_cfg_bin_path) == 0))
    {
        awaiisp_common_err_print("fatal error! ch%d invalid isp cfg bin path %p", isp, param->isp_cfg_bin_path);
        return -1;
    }

    awaiisp_common_switch_case switch_case = param->switch_case;
#endif

    ret |= awaiisp_switch_mode(isp, &param->config);

    end_time0 = getSysTickMs();

#ifdef __linux__

    int nVipp = param->vipp;

    enum set_bit_width bitwidth = (AWAIISP_MODE_NPU == param->config.mode) ? B10_TO_B8 : B8_TO_B10;

    if (AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case)
    {
        awaiisp_common_ver_print("vipp%d set bitwidth %d for stop", nVipp, bitwidth);
        ret |= AW_MPI_VI_SetInputBitWidthStop(nVipp, bitwidth);
    }

    end_time1 = getSysTickMs();

    if (AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case)
    {
        unsigned int drop_num = param->drop_frame_num;
        awaiisp_common_dbg_print("vipp%d set drop_num %d", nVipp, drop_num);
        ret |= AW_MPI_VI_SetDropFrame(nVipp, drop_num);
    }

    end_time2 = getSysTickMs();

    isp_ai_isp_info ai_isp_info;
    memset(&ai_isp_info, 0, sizeof(isp_ai_isp_info));
    if (0 == pContext->npu_lut_enable)
    {
        ai_isp_info.ai_isp_en = 0;
        awaiisp_common_dbg_print("ch%d user disable npu lut, so set ai_isp_en = 0", isp);
    }
    else
    {
        if (AWAIISP_MODE_NPU == param->config.mode || AWAIISP_MODE_NORMAL_GAMMA == param->config.mode)
            ai_isp_info.ai_isp_en = AWAIISP_COMMON_GAMMA_TYPE;
        else
            ai_isp_info.ai_isp_en = 0;
    }
    ret |= AW_MPI_ISP_SetAiIsp(isp, &ai_isp_info);

    end_time3 = getSysTickMs();

    ISP_CFG_BIN_MODE mode_flag = LINEAR_COLOR_CFG;
    ret |= AW_MPI_ISP_ReadIspBin(isp, mode_flag, param->isp_cfg_bin_path);

    end_time4 = getSysTickMs();

    if (AWAIISP_COMMON_SWITCH_CASE_AIISP_8BIT_DAY_10BIT == switch_case)
    {
        awaiisp_common_ver_print("vipp%d set bitwidth %d for start", nVipp, bitwidth);
        ret |= AW_MPI_VI_SetInputBitWidthStart(nVipp, bitwidth);
    }
#endif

    end_time5 = getSysTickMs();

    awaiisp_common_dbg_print("isp%d cost time(ms), switch:%d, stop:%d, drop:%d, setaiispen:%d, ispbin:%d, start:%d", isp,
        end_time0 - start_time, end_time1 - end_time0, end_time2 - end_time1,
        end_time3 - end_time2, end_time4 - end_time3, end_time5 - end_time4);

    return ret;
}

int awaiisp_common_rpmsg_init(void)
{
    struct awaiisp_test_context awaiisp_test_context;

    awaiisp_common_dbg_print("awaiisp_common_rpmsg_init\n");
    memset(&awaiisp_test_context, 0, sizeof(awaiisp_test_context));
    awaiisp_test_proc((void *)&awaiisp_test_context);

    return 0;
}

int awaiisp_common_start(int isp, short int width, short int height)
{
    awaiisp_common_dbg_print("isp%d %dx%d ready to do awaiisp_common_enable\n", isp, width, height);
    awaiisp_common_rt_memheap_init(isp);
	awaiisp_common_config_param awaiisp_common_param;
	memset(&awaiisp_common_param, 0, sizeof(awaiisp_common_config_param));
	awaiisp_common_param.config.width = width;
	awaiisp_common_param.config.height = height;
	awaiisp_common_param.config.tdm_rxbuf_cnt = 5;
	awaiisp_common_param.config.npu_init_buf_size = 0;
	awaiisp_common_param.config.ion_mem_open = 0;
	awaiisp_common_param.config.unprepared_aiisp_resources_advance = 0;
	awaiisp_common_param.config.npu_ref_buf_reduce_enable = 0;
	awaiisp_common_enable(isp, &awaiisp_common_param);
    return 0;
}