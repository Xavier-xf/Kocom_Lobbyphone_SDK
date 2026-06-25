
#ifndef __AW_MPPAO_H__
#define __AW_MPPAO_H__
#include <media/mm_comm_aio.h>
#include <media/mm_common.h>
#include <media/mpi_ao.h>
#include <media/mpi_sys.h>
#include <pthread.h>
#include <utils/media_common_aio.h>

#include "doorlock_common.h"
#include "aw_mpp.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct ao_audio_data_node_s {
    AUDIO_FRAME_S audio_frame;
} ao_audio_data_node_t;

typedef struct mpp_ao_s {
    uint32_t ao_dev;
    uint32_t ao_volume;
    aio_config_t aio_config;
    PAYLOAD_TYPE_E ao_type;// PT_PCM_AUDIO;
    AIO_ATTR_S ao_attr;
} mpp_ao_t;  //__attribute__ ((__packed__)

typedef struct mpp_ao_chn_s {
    AO_CHN ao_chn;
    MPP_CHN_S mpp_chn;
    uint32_t ao_frame_size;
    uint32_t ao_gain; /*Value range: -20 -> 20 */

    AO_VQE_CONFIG_S vqe_attr;
    void (*user_callback)(struct mpp_ao_chn_s *mpp_ao_chn_info);
    void *(*user_thread)(void *arg);  // only when ai_bind is 0
    pthread_t thread;
    pthread_attr_t *thread_attr;
    uint32_t thread_exit;

    // frame data len = channel_cnt*bit_width/8*ao_frame_size;
    AUDIO_FRAME_S ao_frame;
    mpp_ao_t *mpp_ao;
} mpp_ao_chn_t;

typedef void *(*ao_thread_func)(void *arg);

int mpp_ao_init(mpp_ao_t *mpp_ao_info);
int mpp_ao_deinit(mpp_ao_t *mpp_ao_info);
int mpp_ao_chn_deinit(mpp_ao_chn_t *mpp_ao_chn_info);
int mpp_ao_chn_init(mpp_ao_chn_t *mpp_ao_chn_info);
int mpp_ao_chn_start(mpp_ao_chn_t *mpp_ao_chn_info);
int mpp_ao_chn_stop(mpp_ao_chn_t *mpp_ao_chn_info);
int mpp_ao_chn_thread_init(mpp_ao_chn_t *mpp_ao_info);
int mpp_ao_chn_thread_deinit(mpp_ao_chn_t *mpp_ao_info);
int mpp_send_ao_audio_frame(mpp_ao_chn_t *mpp_ao_chn_info, AUDIO_FRAME_S *audio_frame, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
