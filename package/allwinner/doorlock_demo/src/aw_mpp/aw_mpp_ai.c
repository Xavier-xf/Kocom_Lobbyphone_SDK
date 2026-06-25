#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include "aw_mpp_ai.h"

static void *get_ai_frame_thread(void *arg)
{
    int ret = 0;
    mpp_ai_chn_s *mpp_ai_chn_info = (mpp_ai_chn_s *)arg;
    mpp_ai_s *mpp_ai_info = mpp_ai_chn_info->mpp_ai;

    DOORLOCK_DBG("enter ===>\n");

    while (mpp_ai_chn_info->thread_exit == 0) {
        ret = AW_MPI_AI_GetFrame(mpp_ai_info->ai_dev, mpp_ai_chn_info->ai_chn,
                                 &mpp_ai_chn_info->frame_info, NULL, 500);
        if (SUCCESS == ret) {
            if (mpp_ai_chn_info->user_callback != NULL) {
                mpp_ai_chn_info->user_callback(mpp_ai_chn_info, &mpp_ai_chn_info->frame_info);
            } else {
                DOORLOCK_ERR("user_callback = NULL\n");
            }
            ret = AW_MPI_AI_ReleaseFrame(mpp_ai_info->ai_dev, mpp_ai_chn_info->ai_chn,
                                         &mpp_ai_chn_info->frame_info, NULL);
            if (SUCCESS != ret) {
                DOORLOCK_ERR("AW_MPI_AI_ReleaseFrame fail 0x%x\n", ret);
            }
        } else {
            DOORLOCK_ERR("AW_MPI_AI_GetFrame fail 0x%x\n", ret);
            // break;
        }
        usleep(2 * 1000);
    }
    DOORLOCK_DBG("exit <===\n");
}

static int mpp_aio_donfig_attr(AIO_ATTR_S *aio_attr, aio_config_t *aio_config)
{
    aio_attr->mChnCnt = aio_config->channel_cnt;
    aio_attr->enSamplerate = map_SampleRate_to_AUDIO_SAMPLE_RATE_E(
        aio_config->sample_rate);  //(AUDIO_SAMPLE_RATE_E)aio_config->sample_rate;
    aio_attr->enBitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(
        aio_config->bit_width);  //(AUDIO_BIT_WIDTH_E)(aio_config->bit_width/8-1);
    aio_attr->u32FrmNum = 2;  // 2~MAX_PCM_FRAME_NUM

    if (aio_config->bit_width != 8) {
        aio_attr->aec_delay_ms = aio_config->aec_delay_ms;
        aio_attr->ai_aec_en = aio_config->ai_aec_en;
        aio_attr->ai_ans_en = aio_config->ai_ans_en;
        aio_attr->ai_ans_mode = aio_config->ai_ans_mode;
    }
    // if(aio_config->ai_aec_en)
    {
        if (aio_config->frame_size == 0) {
            aio_attr->mPtNumPerFrm = 320;  // alsa_interface.c PERIOD_SIZE
            // aio_attr->mPtNumPerFrm = 1024;
        } else {
            aio_attr->mPtNumPerFrm = aio_config->frame_size;
        }
    }

    aio_attr->ai_agc_en = aio_config->ai_agc_en;
    if (aio_attr->ai_agc_en) {
        aio_attr->ai_agc_cfg.fSample_rate = aio_attr->enSamplerate;
        aio_attr->ai_agc_cfg.iBytePerSample = aio_config->bit_width / 8;
        aio_attr->ai_agc_cfg.iChannel = aio_config->channel_cnt;
        if (aio_config->frame_size == 0) {
            aio_attr->ai_agc_cfg.iSample_len = 1024;
        } else {
            aio_attr->ai_agc_cfg.iSample_len = aio_config->frame_size;
        }
        aio_attr->ai_agc_cfg.iGain_level = aio_config->ai_agc_gain;
    }
}

int mpp_ai_init(mpp_ai_s *mpp_ai_info)
{
    int ret = 0;
    DOORLOCK_DBG("enter ===>\n");
    mpp_aio_donfig_attr(&mpp_ai_info->ai_attr, &mpp_ai_info->aio_config);

    AW_MPI_AI_SetPubAttr(mpp_ai_info->ai_dev, &mpp_ai_info->ai_attr);

    uint32_t volume_val = mpp_ai_info->ai_volume;
    AW_MPI_AI_SetDevVolume(mpp_ai_info->ai_dev, volume_val);

    volume_val = 0;
    AW_MPI_AI_GetDevVolume(mpp_ai_info->ai_dev, &volume_val);

    DOORLOCK_INFO("volume_val write %d, read %d\n", mpp_ai_info->ai_volume, volume_val);

    AW_MPI_AI_Enable(mpp_ai_info->ai_dev);  // embedded in AW_MPI_AI_CreateChn ???
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int mpp_ai_deinit(mpp_ai_s *mpp_ai_info)
{
    int ret = 0;
    DOORLOCK_DBG("enter ===>\n");
#if 1
    AW_MPI_AI_Disable(mpp_ai_info->ai_dev);  // embedded in AW_MPI_AI_DestroyChn ???
#endif
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int mpp_ai_chn_init(mpp_ai_chn_s *mpp_ai_chn_info)
{
    int ret = 0;
    int ai_chn = mpp_ai_chn_info->ai_chn;
    mpp_ai_s *mpp_ai_info = mpp_ai_chn_info->mpp_ai;

    if (mpp_ai_info == NULL) {
        DOORLOCK_ERR("mpp_ai == NULL\n");
        return -1;
    }
    DOORLOCK_DBG("enter ===>\n");
    if (mpp_ai_info == NULL) {
        DOORLOCK_ERR("mpp_ai_info == NULL\n");
        goto _exit;
    }
    usleep(10 * 1000);
#if 1
    while (ai_chn < AIO_MAX_CHN_NUM) {
        ret = AW_MPI_AI_CreateChn(mpp_ai_info->ai_dev, ai_chn);
        if (SUCCESS == ret) {
            DOORLOCK_DBG("AW_MPI_AI_CreateChn [%d] success\n", ai_chn);
            break;
        } else if (ERR_AI_EXIST == ret) {
            DOORLOCK_DBG("AW_MPI_AI_CreateChn [%d] exist, find next...\n", ai_chn);
            ai_chn++;
        } else if (ERR_AI_NOT_ENABLED == ret) {
            DOORLOCK_ERR("audio_hw_ai not started!\n");
            break;
        } else {
            DOORLOCK_ERR("AW_MPI_AI_CreateChn [%d] fail 0x%x\n", ai_chn, ret);
            break;
        }
    }
#else
    ret = AW_MPI_AI_CreateChn(mpp_ai_info->ai_dev, ai_chn);
#endif

    if (SUCCESS != ret) {
        DOORLOCK_ERR("AW_MPI_AI_CreateChn fail  0x%x\n", ret);
        goto _exit;
    }
    mpp_ai_chn_info->ai_chn = ai_chn;

    mpp_ai_chn_info->mpp_chn.mModId = MOD_ID_AI;
    mpp_ai_chn_info->mpp_chn.mDevId = 0;
    mpp_ai_chn_info->mpp_chn.mChnId = mpp_ai_chn_info->ai_chn;

    if (mpp_ai_chn_info->bind == 0) {
        mpp_frm_manager_init(&mpp_ai_chn_info->ai_frm_manager);
    }

#if 0
	AW_MPI_AI_GetVqeAttr(mpp_ai_info->ai_dev, mpp_ai_chn_info->ai_chn, &mpp_ai_info->vqe_attr);
    if(mpp_ai_chn_info->ai_gain){
        mpp_ai_chn_info->vqe_attr.bGainOpen = 1;
        mpp_ai_chn_info->vqe_attr.stGainCfg.s8GainValue = mpp_ai_chn_info->ai_gain;
        mpp_ai_chn_info->vqe_attr.enWorkstate = VQE_WORKSTATE_COMMON;//VQE_WORKSTATE_MUSIC;//VQE_WORKSTATE_NOISY;//
        mpp_ai_chn_info->vqe_attr.s32WorkSampleRate = mpp_ai_info->sample_rate;
        mpp_ai_chn_info->vqe_attr.s32FrameSample = 1024;//VQE frame lenght: 80~4096
    }else{
		mpp_ai_chn_info->vqe_attr.stGainCfg.s8GainValue = 0;
        mpp_ai_chn_info->vqe_attr.bGainOpen = 0;
    }
    AW_MPI_AI_SetVqeAttr(mpp_ai_info->ai_dev, mpp_ai_chn_info->ai_chn, &mpp_ai_chn_info->vqe_attr);
    AW_MPI_AI_EnableVqe(mpp_ai_info->ai_dev, mpp_ai_chn_info->ai_chn);
#endif

    AW_MPI_AI_EnableChn(mpp_ai_info->ai_dev, ai_chn);
_exit:
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int mpp_ai_chn_deinit(mpp_ai_chn_s *mpp_ai_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    mpp_ai_s *mpp_ai_info = mpp_ai_chn_info->mpp_ai;
    if (mpp_ai_info == NULL) {
        DOORLOCK_ERR("mpp_ai == NULL\n");
        return -1;
    }
    int ai_chn = mpp_ai_chn_info->ai_chn;

    AW_MPI_AI_DisableChn(mpp_ai_info->ai_dev, ai_chn);
    if (mpp_ai_chn_info->bind == 0) {
        mpp_frm_manager_deinit(&mpp_ai_chn_info->ai_frm_manager);
    }

    AW_MPI_AI_DestroyChn(mpp_ai_info->ai_dev, ai_chn);

    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int mpp_ai_chn_start(mpp_ai_chn_s *mpp_ai_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    mpp_ai_s *mpp_ai_info = mpp_ai_chn_info->mpp_ai;
    if (mpp_ai_info == NULL) {
        DOORLOCK_ERR("mpp_ai == NULL\n");
        return -1;
    }

    int ai_chn = mpp_ai_chn_info->ai_chn;
    int ret = AW_MPI_AI_EnableChn(mpp_ai_info->ai_dev, ai_chn);
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int mpp_ai_chn_stop(mpp_ai_chn_s *mpp_ai_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    mpp_ai_s *mpp_ai_info = mpp_ai_chn_info->mpp_ai;
    if (mpp_ai_info == NULL) {
        DOORLOCK_ERR("mpp_ai == NULL\n");
        return -1;
    }

    int ai_chn = mpp_ai_chn_info->ai_chn;
    int ret = AW_MPI_AI_DisableChn(mpp_ai_info->ai_dev, ai_chn);
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int mpp_ai_chn_thread_init(mpp_ai_chn_s *mpp_ai_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    int ret = 0;
    mpp_ai_s *mpp_ai_info = mpp_ai_chn_info->mpp_ai;
    if (mpp_ai_info == NULL) {
        DOORLOCK_ERR("mpp_ai == NULL\n");
        return -1;
    }

    if (mpp_ai_chn_info->bind == 0) {
        ai_thread_func ai_thread_func;
        if (mpp_ai_chn_info->user_thread == NULL) {
            ai_thread_func = get_ai_frame_thread;
        } else {
            ai_thread_func = mpp_ai_chn_info->user_thread;
        }
        mpp_ai_chn_info->thread_exit = 0;
        ret = pthread_create(&mpp_ai_chn_info->thread, mpp_ai_chn_info->thread_attr, ai_thread_func,
                             (void *)mpp_ai_chn_info);
        if (ret != 0) {
            DOORLOCK_ERR("pthread_create fail 0x%x(%s), Dev[%d], Chn[%d].\n", ret, strerror(ret),
                        mpp_ai_info->ai_dev, mpp_ai_chn_info->ai_chn);
            mpp_ai_chn_info->thread = 0;
        } else {
            char thread_name[50];
            memset(thread_name, 0, sizeof(thread_name));
            sprintf(thread_name, "mpp_ai_chn_thread-%d_%d", mpp_ai_info->ai_dev, mpp_ai_chn_info->ai_chn);
            pthread_setname_np(mpp_ai_chn_info->thread, thread_name);
        }
    }
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int mpp_ai_chn_thread_deInit(mpp_ai_chn_s *mpp_ai_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    mpp_ai_s *mpp_ai_info = mpp_ai_chn_info->mpp_ai;
    if (mpp_ai_info == NULL) {
        DOORLOCK_ERR("mpp_ai == NULL\n");
        return -1;
    }
    if (mpp_ai_chn_info->bind == 0) {
        // if(mpp_ai_chn_info->user_thread == NULL)
        {
            mpp_ai_chn_info->thread_exit = 1;
            if (mpp_ai_chn_info->thread != 0) {
                pthread_join(mpp_ai_chn_info->thread, NULL);
            }
        }
    }
    DOORLOCK_DBG("exit <===\n");
    return 0;
}
