
/*
* Copyright (c) 2008-2016 Allwinner Technology Co. Ltd.
* All rights reserved.
*
* File : CdcSysinfo.c
* Description :
* History :
*   Author  : xyliu <xyliu@allwinnertech.com>
*   Date    : 2016/04/13
*   Comment :
*
*
*/

#ifdef CONFIG_VIDEO_RT_MEDIA
#include <linux/slab.h>

#else
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#endif

#include "cdc_log.h"
#include "CdcUtil.h"

int procInfo_initMem(struct ve_channel_proc_info *proc_info_mgr)
{
    if(proc_info_mgr == NULL)
    {
        loge("param error");
        return -1;
    }

    if(proc_info_mgr->base_info_data == NULL)
    {
        proc_info_mgr->base_info_data = MALLOC(BASE_PROC_BUF_LEN);
        if(proc_info_mgr->base_info_data == NULL)
        {
            loge("malloc failed, size = %d", BASE_PROC_BUF_LEN);
            return -1;
        }
        proc_info_mgr->advance_info_data = MALLOC(BASE_PROC_BUF_LEN);
        if(proc_info_mgr->advance_info_data == NULL)
        {
            loge("malloc failed, size = %d", BASE_PROC_BUF_LEN);
            return -1;
        }
    }
    return 0;
}

void procInfo_DestroyMem(struct ve_channel_proc_info *proc_info_mgr)
{
    if(proc_info_mgr == NULL)
    {
        loge("param error");
        return ;
    }

    if(proc_info_mgr->base_info_data)
    {
        FREE(proc_info_mgr->base_info_data);
        proc_info_mgr->base_info_data = NULL;
    }

    if(proc_info_mgr->advance_info_data)
    {
        FREE(proc_info_mgr->advance_info_data);
        proc_info_mgr->advance_info_data = NULL;
    }
}

int ComputeLbcParameter(ve_lbc_in_param* in_param, ve_lbc_out_param* out_param)
{
    unsigned int is_lossy  = 1;
	unsigned int seg_rc_en = 1;
    unsigned int lbc_align = 256;
    unsigned int seg_w     = 16;
    unsigned int seg_h     = 8;
    unsigned int bitdepth  = 8;
    unsigned int cmp_ratio = 683; //*1x:1024; 1.2x:854; 1.5x: 683; 2x:512; 2.5x: 410
	unsigned int frm_w_align = ALIGN_XXB(seg_w, in_param->nWidht);
	//When ref LBC compressing, it also fills the upper and lower borders with 4 lines
	//16aligh,then plus 4,final 8aligh
    unsigned int frm_h_align = ALIGN_XXB(8, ALIGN_XXB(16, in_param->nHeight)+4);
	unsigned int seg_tar_bits = 0;
	unsigned int segline_tar_bits = 0;
	unsigned int seg_num = frm_w_align/seg_w;
    unsigned int buffer_size = 0;
    unsigned int bs_buffer_size = 0;
    unsigned int info_buffer_size = 0;

	if(in_param->eLbcMode == LBC_MODE_DISABLE)
        return 0;
    else if(in_param->eLbcMode == LBC_MODE_1_0X)
        cmp_ratio = 1024;
	else if(in_param->eLbcMode == LBC_MODE_1_5X)//* 1.5x
		cmp_ratio = 683;
	else if(in_param->eLbcMode == LBC_MODE_2_0X)//* 2x
		cmp_ratio = 512;
	else if(in_param->eLbcMode == LBC_MODE_2_5X)//* 2.5x
		cmp_ratio = 410;
	else if(in_param->eLbcMode == LBC_MODE_NO_LOSSY)//* is no lossy
		is_lossy = 0;

	if(is_lossy == 0)
	{
		unsigned int c_busrt_num_lenth = 8;
		unsigned int align_c = 8;
		unsigned int mb_num = 6;
		unsigned int sb_num = 2;
		unsigned int lossless_add_bits = c_busrt_num_lenth + align_c  - 1 + 2*mb_num*sb_num + 8*mb_num*sb_num;
		seg_tar_bits = ((seg_w*seg_h*bitdepth*3/2 + lossless_add_bits + lbc_align - 1)/lbc_align)*lbc_align;
		segline_tar_bits = seg_num*seg_tar_bits;
	}
	else
	{
		seg_tar_bits = ((seg_w*seg_h*bitdepth*cmp_ratio*3/2/1024)/lbc_align)*lbc_align;
		if(seg_rc_en == 1)
		{
			segline_tar_bits = (frm_w_align*seg_h*bitdepth*3/2*cmp_ratio/1024);
			segline_tar_bits = ALIGN_XXB(lbc_align,segline_tar_bits);
		}
		else
		{
			segline_tar_bits = seg_num*seg_tar_bits;
		}
	}

    bs_buffer_size = segline_tar_bits* frm_h_align/seg_h;
    info_buffer_size =  (((frm_w_align / seg_w + 16 - 1) / 16) * 16 * (frm_h_align/ seg_h))*2;
    info_buffer_size = ALIGN_XXB(4096, info_buffer_size);

    logv("info_buffer_size = %d, frm_w_align = %d, seg_w = %d, frm_h_align = %d, seg_h = %d",
        info_buffer_size, frm_w_align, seg_w, frm_h_align, seg_h);

    buffer_size= (bs_buffer_size+7) / 8 + info_buffer_size;

    int ext_size = 8*1024;
    buffer_size = buffer_size + ext_size;
    buffer_size = ALIGN_XXB(4096, buffer_size);
    logv("ext_size = %d", ext_size);

    logv("buffer_size = %d, bs_buffer_size = %d, info_buffer_size = %d",
           buffer_size,  bs_buffer_size, info_buffer_size);

    out_param->header_size      = info_buffer_size;
    out_param->buffer_size      = buffer_size;
    out_param->seg_tar_bits     = seg_tar_bits;
    out_param->segline_tar_bits = segline_tar_bits;
    out_param->is_lossy = is_lossy;
    out_param->seg_rc_en = seg_rc_en;

    logv("in: w&h = %d,%d, lbcMode = %d",
         frm_w_align, frm_h_align, in_param->eLbcMode);
    logv("out: bufSize = %d, seg_bits = 0x%x, segline_bits = 0x%x, is_lossy = %d, seg_rc_en = %d",
         out_param->buffer_size, out_param->seg_tar_bits, out_param->segline_tar_bits,
         out_param->is_lossy, out_param->seg_rc_en);

	return 0;
}

#define FORCE_PRINTF_PEROID_CNT (0)

void create_stat_time(StaticTImeInfo *pTimeInfo, int64_t nBigTime, int64_t big_time_step, char *name)
{
	if (name)
		strncpy(pTimeInfo->stat_name, name, sizeof(pTimeInfo->stat_name) - 1);
	pTimeInfo->nBigTime = nBigTime;
	pTimeInfo->big_time_step = big_time_step;
}

void set_start_time(StaticTImeInfo *pTimeInfo)
{
    pTimeInfo->startTime = getCurrentTime();
}

static void set_end_time(StaticTImeInfo *pTimeInfo)
{
    pTimeInfo->cnt++;
    pTimeInfo->endTime = getCurrentTime();
}

static void compute_stat_time(StaticTImeInfo *pTimeInfo)
{
    int i = 0, cnt = 0;
	if (pTimeInfo->user_cnt) cnt = pTimeInfo->user_cnt; else cnt = pTimeInfo->cnt;
    if(pTimeInfo->startTime == 0 || cnt == 0) {
		logw("startTime is 0");
        return ;
	}
    int64_t diff = pTimeInfo->endTime - pTimeInfo->startTime;
	pTimeInfo->cur_gap_time = diff;
    pTimeInfo->totalTime += diff;
    pTimeInfo->avrTime = pTimeInfo->totalTime/cnt;

    if(diff > pTimeInfo->maxtime) {
        pTimeInfo->maxtime = diff;
		pTimeInfo->print_max_occur = 1;
	}

	if (pTimeInfo->mintime == 0 || diff < pTimeInfo->mintime)
		pTimeInfo->mintime = diff;


    if(pTimeInfo->nBigTime && diff >= pTimeInfo->nBigTime)
    {
        pTimeInfo->bigCnt[0]++;
        for(i = 1; i < CDC_STATCI_TIME_INFO_MAX_BIG_TIME_NUM; i++)
        {
            if(diff >=(pTimeInfo->nBigTime + pTimeInfo->big_time_step*(i)) )
                pTimeInfo->bigCnt[i]++;
        }
    }
}

void set_end_time_and_compute(StaticTImeInfo *pTimeInfo)
{
	set_end_time(pTimeInfo);
	compute_stat_time(pTimeInfo);
}

void print_stat_info(StaticTImeInfo *pTimeInfo, int peroid)
{
    int cnt = 0;
	if (pTimeInfo->user_cnt) cnt = pTimeInfo->user_cnt; else cnt = pTimeInfo->cnt;
	int64_t nBigTimeStep = pTimeInfo->big_time_step;
	int64_t fps_int = 0;
	int64_t fps_dot = 0;
	if (pTimeInfo->avrTime) {
		fps_int = US_PER_S/pTimeInfo->avrTime;
		if (pTimeInfo->avrTime/US_PER_MS)
			fps_dot = MS_PER_S%(pTimeInfo->avrTime/US_PER_MS);
	}
    if (cnt%peroid == 0 || pTimeInfo->print_max_occur) {
        logw("%s: cost_time %lld, avrTime = %lld, avr_fps %lld.%lld, maxTime = %lld, min_time = %lld, cnt = %d, peroidCnt = %d",
             pTimeInfo->stat_name, pTimeInfo->cur_gap_time, pTimeInfo->avrTime, fps_int, fps_dot,
			 pTimeInfo->maxtime, pTimeInfo->mintime,
             cnt, peroid);
		if (pTimeInfo->nBigTime)
        logv("%s: bigCnt[%lld] = %lld, bigCnt[%lld] = %lld, bigCnt[%lld] = %lld, bigCnt[%lld] = %lld, bigCnt[%lld] = %lld",
                     pTimeInfo->stat_name,
                     pTimeInfo->nBigTime,                  pTimeInfo->bigCnt[0],
                     pTimeInfo->nBigTime + nBigTimeStep,   pTimeInfo->bigCnt[1],
                     pTimeInfo->nBigTime + nBigTimeStep*2, pTimeInfo->bigCnt[2],
                     pTimeInfo->nBigTime + nBigTimeStep*3, pTimeInfo->bigCnt[3],
                     pTimeInfo->nBigTime + nBigTimeStep*4, pTimeInfo->bigCnt[4]);
		pTimeInfo->print_max_occur = 0;
    }
}

void set_end_time_do_all(StaticTImeInfo *pTimeInfo, int peroid)
{
	if (pTimeInfo->startTime == 0)
		return ;
	set_end_time_and_compute(pTimeInfo);
	print_stat_info(pTimeInfo, peroid);
}

void stat_time_interval(StaticTImeInfo *pTimeInfo, int64_t nBigTime, int64_t big_time_step, int peroid, char *name)
{
	if (pTimeInfo->startTime == 0) {
		create_stat_time(pTimeInfo, nBigTime, big_time_step, name);
	}
	set_end_time_do_all(pTimeInfo, peroid);
	set_start_time(pTimeInfo);
}

void Stat_CompulteTimeDiff(StaticTImeInfo *pTimeInfo,
                                        StaticTImeInfo *pTimeInfo1, StaticTImeInfo *pTimeInfo2,
                                        int64_t nBigTimeBase /* us*/,
                                        int64_t nBigTimeStep /* us*/,
                                        int printfPeroidCnt, int nPrivate, const char* name)
{
    int i = 0, stat_bigtime = 0;
	if (nBigTimeBase) stat_bigtime = 1;
    if(pTimeInfo1->cnt != pTimeInfo2->cnt)
        logv("***[%d] cnt not the same: %lld, %lld",nPrivate, pTimeInfo1->cnt, pTimeInfo2->cnt);

    int64_t diff2 = pTimeInfo2->endTime - pTimeInfo2->startTime;
    int64_t diff1 = pTimeInfo1->endTime - pTimeInfo1->startTime;
    int64_t diff = diff2 - diff1;

    pTimeInfo->totalTime += diff;
    pTimeInfo->cnt++;
    pTimeInfo->avrTime = pTimeInfo->totalTime/pTimeInfo->cnt;
    pTimeInfo->nBigTime = nBigTimeBase;

    if(diff > pTimeInfo->maxtime)
        pTimeInfo->maxtime = diff;

    if(diff >= pTimeInfo->nBigTime && stat_bigtime)
    {
        pTimeInfo->bigCnt[0]++;
        for(i = 1; i < CDC_STATCI_TIME_INFO_MAX_BIG_TIME_NUM; i++)
        {
            if(diff >=(pTimeInfo->nBigTime + nBigTimeStep*(i)) )
                pTimeInfo->bigCnt[i]++;
        }
    }

    if(FORCE_PRINTF_PEROID_CNT != 0)
        printfPeroidCnt = FORCE_PRINTF_PEROID_CNT;

    if(pTimeInfo->cnt%printfPeroidCnt == 0)
    {
        logd("*[%d], %s: curTime = %lld, avrTime = %lld, maxTime = %lld, cnt = %lld, peroidCnt = %d",
             nPrivate, name, diff, pTimeInfo->avrTime, pTimeInfo->maxtime,
             pTimeInfo->cnt, printfPeroidCnt);
		if (stat_bigtime)
        logd("*[%d], %s: bigCnt[%lld] = %lld, bigCnt[%lld] = %lld, bigCnt[%lld] = %lld, bigCnt[%lld] = %lld, bigCnt[%lld] = %lld",
                     nPrivate, name,
                     pTimeInfo->nBigTime,                  pTimeInfo->bigCnt[0],
                     pTimeInfo->nBigTime + nBigTimeStep,   pTimeInfo->bigCnt[1],
                     pTimeInfo->nBigTime + nBigTimeStep*2, pTimeInfo->bigCnt[2],
                     pTimeInfo->nBigTime + nBigTimeStep*3, pTimeInfo->bigCnt[3],
                     pTimeInfo->nBigTime + nBigTimeStep*4, pTimeInfo->bigCnt[4]);
    }

}
