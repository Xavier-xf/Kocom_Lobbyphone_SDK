
#ifndef __AW_MPPAI_H__
#define __AW_MPPAI_H__
#include <media/mm_comm_aio.h>
#include <media/mpi_ai.h>
#include <media/mpi_sys.h>
#include <pthread.h>
#include <utils/media_common_aio.h>

#include "doorlock_common.h"
#include "aw_mpp.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct mpp_ai_s {
    uint32_t ai_dev;
    uint32_t ai_volume;
    aio_config_t aio_config;
    AIO_ATTR_S ai_attr;
} mpp_ai_s;

typedef struct mpp_ai_chn_s {
    mpp_ai_s *mpp_ai;
    uint32_t ai_chn;
    MPP_CHN_S mpp_chn;

    uint32_t ai_gain;
    AI_VQE_CONFIG_S vqe_attr;
    uint32_t pcm_size;
    uint32_t frame_cnt;
    uint32_t bind;
    AUDIO_FRAME_S frame_info;
    pthread_t thread;
    pthread_attr_t *thread_attr;
    uint32_t thread_exit;
    void (*user_callback)(struct mpp_ai_chn_s *mpp_ai_chn_info, AUDIO_FRAME_S *frame_info);
    void *(*user_thread)(void *arg);  // only when ai_bind is 0

    mpp_frm_manager_t ai_frm_manager;
} mpp_ai_chn_s;  //__attribute__ ((__packed__)

typedef void *(*ai_thread_func)(void *arg);

int mpp_ai_init(mpp_ai_s *mpp_ai_info);
int mpp_ai_deinit(mpp_ai_s *mpp_ai_info);
int mpp_ai_chn_init(mpp_ai_chn_s *mpp_ai_chn_info);
int mpp_ai_chn_deinit(mpp_ai_chn_s *mpp_ai_chn_info);
int mpp_ai_chn_thread_init(mpp_ai_chn_s *mpp_ai_info);
int mpp_ai_chn_thread_deInit(mpp_ai_chn_s *mpp_ai_info);
int mpp_ai_chn_start(mpp_ai_chn_s *mpp_ai_chn_info);
int mpp_ai_chn_stop(mpp_ai_chn_s *mpp_ai_chn_info);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
