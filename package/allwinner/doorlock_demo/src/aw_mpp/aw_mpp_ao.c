#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

#include "aw_mpp_ao.h"

static void *send_ao_frame_thread(void *arg)
{
    DOORLOCK_DBG("enter ===>\n");
    mpp_ao_chn_t *mpp_ao_chn_info = (mpp_ao_chn_t *)arg;
    mpp_ao_t *mpp_ao_info = mpp_ao_chn_info->mpp_ao;

    if (mpp_ao_chn_info == NULL) {
        DOORLOCK_ERR("arg == NULL\n");
        return NULL;
    }
    if (mpp_ao_info == NULL) {
        DOORLOCK_ERR("mpp_ao == NULL\n");
        return NULL;
    }

    while (mpp_ao_chn_info->thread_exit == 0) {
        if (mpp_ao_chn_info->user_callback != NULL) {
            mpp_ao_chn_info->user_callback(mpp_ao_chn_info);
        }
        usleep(5 * 1000);
    }
    DOORLOCK_DBG("exit <===\n");
}

static ERRORTYPE mpp_ao_callback_wrapper(void *cookie, MPP_CHN_S *chn, MPP_EVENT_TYPE event,
                                      void *event_data)
{
    ERRORTYPE ret = SUCCESS;
    if (chn == NULL) {
        DOORLOCK_ERR("chn == NULL");
        return ret;
    }
    if (MOD_ID_AO == chn->mModId) {
        //DOORLOCK_INFO("event[0x%x] from mModId[0x%x], mDevId[0x%x], mChnId[0x%x]!\n", \
                    event, chn->mModId, chn->mDevId, chn->mChnId);
        // DOORLOCK_INFO("AO chnId[%d]\n", chn->mChnId);
        switch (event) {
        case MPP_EVENT_RELEASE_AUDIO_BUFFER: {
            break;
        }
        case MPP_EVENT_NOTIFY_EOF: {
            DOORLOCK_INFO("AO channel notify APP that play complete!\n");
            // sem_post(&gEofSemaphore);
            break;
        }
        default: {
            // postEventFromNative(this, event, 0, 0, event_data);
            DOORLOCK_ERR("unknown event[0x%x] from mModId[0x%x], mDevId[0x%x], mChnId[0x%x]!\n",
                        event, chn->mModId, chn->mDevId, chn->mChnId);
            ret = ERR_AO_ILLEGAL_PARAM;
            break;
        }
        }
    } else {
        DOORLOCK_ERR("fatal error! why modId[0x%x] != [0x%x] ?\n", chn->mModId, MOD_ID_AO);
        ret = FAILURE;
    }
    return ret;
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

int mpp_ao_init(mpp_ao_t *mpp_ao_info)
{
    DOORLOCK_DBG("enter ===>\n");
    memset(&mpp_ao_info->ao_attr, 0, sizeof(AIO_ATTR_S));
    mpp_ao_info->ao_dev = 0;
    mpp_aio_donfig_attr(&mpp_ao_info->ao_attr, &mpp_ao_info->aio_config);
    AW_MPI_AO_SetPubAttr(mpp_ao_info->ao_dev, &mpp_ao_info->ao_attr);
    AW_MPI_AO_SetDevVolume(mpp_ao_info->ao_dev, mpp_ao_info->ao_volume);
    uint32_t volume_val = 0;
    AW_MPI_AO_GetDevVolume(mpp_ao_info->ao_dev, &volume_val);
    DOORLOCK_INFO("ao_volume write %d, read %d\n", mpp_ao_info->ao_volume, volume_val);
    DOORLOCK_DBG("exit <===\n");
}

int mpp_ao_deinit(mpp_ao_t *mpp_ao_info)
{
    int ret = 0;
    DOORLOCK_DBG("enter ===>\n");
    return ret;
}

#if 0
static void *ao_audio_data_node_malloc(int data_size)
{
    static int frame_id = 0;
    DOORLOCK_DBG("enter ===>\n");

    ao_audio_data_node_t *audio_data_node =
        (ao_audio_data_node_t *)malloc(sizeof(ao_audio_data_node_t));
    if (audio_data_node == NULL) {
        DOORLOCK_ERR("malloc %d failed\n", sizeof(ao_audio_data_node_t));
        return NULL;
    }
    memset(audio_data_node, 0, sizeof(ao_audio_data_node_t));
    audio_data_node->audio_frame.mId = frame_id++;
    if (data_size == 0) {
        DOORLOCK_WARN("ao_audio_data_node_malloc data_size = 0\n");
    } else {
        audio_data_node->audio_frame.mpAddr = malloc(data_size);
        audio_data_node->audio_frame.mLen = data_size;
    }
    return audio_data_node;
    DOORLOCK_DBG("exit <===\n");
}

static int ao_audio_data_node_free(void *node)
{
    DOORLOCK_DBG("enter ===>\n");
    ao_audio_data_node_t *audio_data_node = node;

    if (audio_data_node == NULL) {
        DOORLOCK_ERR("node == NULL\n");
        return -1;
    }
    if (audio_data_node->audio_frame.mpAddr) {
        free(audio_data_node->audio_frame.mpAddr);
        audio_data_node->audio_frame.mLen = 0;
        audio_data_node->audio_frame.mpAddr = NULL;
    } else {
        DOORLOCK_ERR("pData == NULL\n");
    }
    free(audio_data_node);
    audio_data_node = NULL;
    DOORLOCK_DBG("exit <===\n");
    return 0;
}
#endif
int mpp_ao_chn_init(mpp_ao_chn_t *mpp_ao_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    int ret = 0;
    mpp_ao_t *mpp_ao = mpp_ao_chn_info->mpp_ao;

    if (mpp_ao == NULL) {
        DOORLOCK_ERR("mpp_ao == NULL\n");
        return -1;
    }
    int ao_chn = mpp_ao_chn_info->ao_chn;
    // AW_MPI_AO_SetPubAttr(mpp_ao->ao_dev, &mpp_ao->ao_attr);
    // sem_init(&gEofSemaphore, 0, 0);
#if 1
    while (ao_chn < AIO_MAX_CHN_NUM) {
        ret = AW_MPI_AO_CreateChn(mpp_ao->ao_dev, ao_chn);
        if (SUCCESS == ret) {
            DOORLOCK_DBG("AW_MPI_AO_CreateChn [%d] success\n", ao_chn);
            break;
        } else if (ERR_AO_EXIST == ret) {
            DOORLOCK_DBG("AW_MPI_AO_CreateChn [%d] exist, find next...\n", ao_chn);
            ao_chn++;
        } else if (ERR_AO_NOT_ENABLED == ret) {
            DOORLOCK_ERR("audio_hw_ai not started!\n");
            break;
        } else {
            DOORLOCK_ERR("AW_MPI_AO_CreateChn [%d] fail 0x%x\n", ao_chn, ret);
            break;
        }
    }
#else
    ret = AW_MPI_AO_CreateChn(mpp_ao->ao_dev, ao_chn);
#endif
    if (SUCCESS != ret) {
        DOORLOCK_ERR("AW_MPI_AO_CreateChn fail  0x%x\n", ret);
        goto _exit;
    }
    mpp_ao_chn_info->ao_chn = ao_chn;

    mpp_ao_chn_info->mpp_chn.mModId = MOD_ID_AO;
    mpp_ao_chn_info->mpp_chn.mDevId = 0;
    mpp_ao_chn_info->mpp_chn.mChnId = mpp_ao_chn_info->ao_chn;
#if 0
    mpp_ao_chn_info->dlink_list_manager.mUserNodeMalloc = ao_audio_data_node_malloc;
    mpp_ao_chn_info->dlink_list_manager.mUserNodeFree = ao_audio_data_node_free;
#endif
    MPPCallbackInfo cb_info;
    cb_info.cookie = (void *)mpp_ao_chn_info;
    cb_info.callback = (MPPCallbackFuncType)&mpp_ao_callback_wrapper;
    AW_MPI_AO_RegisterCallback(mpp_ao->ao_dev, mpp_ao_chn_info->ao_chn, &cb_info);

_exit:
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int mpp_ao_chn_deinit(mpp_ao_chn_t *mpp_ao_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    mpp_ao_t *mpp_ao = mpp_ao_chn_info->mpp_ao;

    if (mpp_ao == NULL) {
        DOORLOCK_ERR("mpp_ao == NULL\n");
        return -1;
    }
    AW_MPI_AO_DestroyChn(mpp_ao->ao_dev, mpp_ao_chn_info->ao_chn);

    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int mpp_ao_chn_start(mpp_ao_chn_t *mpp_ao_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    mpp_ao_t *mpp_ao = mpp_ao_chn_info->mpp_ao;

    if (mpp_ao == NULL) {
        DOORLOCK_ERR("mpp_ao == NULL\n");
        return -1;
    }
    AW_MPI_AO_StartChn(mpp_ao->ao_dev, mpp_ao_chn_info->ao_chn);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int mpp_ao_chn_stop(mpp_ao_chn_t *mpp_ao_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    mpp_ao_t *mpp_ao = mpp_ao_chn_info->mpp_ao;

    if (mpp_ao == NULL) {
        DOORLOCK_ERR("mpp_ao == NULL\n");
        return -1;
    }

    AW_MPI_AO_SetStreamEof(mpp_ao->ao_dev, mpp_ao_chn_info->ao_chn, 1, 1);
    AW_MPI_AO_StopChn(mpp_ao->ao_dev, mpp_ao_chn_info->ao_chn);
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int mpp_ao_chn_thread_init(mpp_ao_chn_t *mpp_ao_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    int ret = 0;
    mpp_ao_t *mpp_ao_info = mpp_ao_chn_info->mpp_ao;

    if (mpp_ao_info == NULL) {
        DOORLOCK_ERR("mpp_ao == NULL\n");
        return -1;
    }
    // if(mpp_ao_chn_info->bind == 0)
    {
        ao_thread_func ao_thread_func;
        if (mpp_ao_chn_info->user_thread == NULL) {
            ao_thread_func = send_ao_frame_thread;
        } else {
            ao_thread_func = mpp_ao_chn_info->user_thread;
        }
        mpp_ao_chn_info->thread_exit = 0;
        ret = pthread_create(&mpp_ao_chn_info->thread, mpp_ao_chn_info->thread_attr, ao_thread_func,
                             (void *)mpp_ao_chn_info);
        if (ret != 0) {
            DOORLOCK_ERR("pthread_create fail 0x%x(%s), Dev[%d], Chn[%d].\n", ret, strerror(ret),
                        mpp_ao_info->ao_dev, mpp_ao_chn_info->ao_chn);
            mpp_ao_chn_info->thread = 0;
        } else {
            char thread_name[50];
            memset(thread_name, 0, sizeof(thread_name));
            sprintf(thread_name, "AoChnThread-%d_%d", mpp_ao_info->ao_dev, mpp_ao_chn_info->ao_chn);
            pthread_setname_np(mpp_ao_chn_info->thread, thread_name);
        }
    }
    DOORLOCK_DBG("exit <===\n");
    return ret;
}

int mpp_ao_chn_thread_deinit(mpp_ao_chn_t *mpp_ao_chn_info)
{
    DOORLOCK_DBG("enter ===>\n");
    mpp_ao_t *mpp_ao_info = mpp_ao_chn_info->mpp_ao;

    if (mpp_ao_info == NULL) {
        DOORLOCK_ERR("mpp_ao == NULL\n");
        return -1;
    }
    // if(mpp_ao_chn_info->bind == 0)
    {
        // if(mpp_ao_chn_info->user_thread == NULL)
        {
            mpp_ao_chn_info->thread_exit = 1;
            if (mpp_ao_chn_info->thread != 0) {
                pthread_join(mpp_ao_chn_info->thread, NULL);
            }
        }
    }
    DOORLOCK_DBG("exit <===\n");
    return 0;
}

int mpp_send_ao_audio_frame(mpp_ao_chn_t *mpp_ao_chn_info, AUDIO_FRAME_S *audio_frame, uint32_t timeout_ms)
{
    int ret = 0;
    mpp_ao_t *mpp_ao = mpp_ao_chn_info->mpp_ao;

    if (mpp_ao == NULL) {
        DOORLOCK_ERR("mpp_ao == NULL\n");
        return -1;
    }
    if (timeout_ms) {
        ret = AW_MPI_AO_SendFrame(mpp_ao->ao_dev, mpp_ao_chn_info->ao_chn, audio_frame, timeout_ms);
    } else {
        ret = AW_MPI_AO_SendFrameSync(mpp_ao->ao_dev, mpp_ao_chn_info->ao_chn, audio_frame);
    }
    return ret;
}
