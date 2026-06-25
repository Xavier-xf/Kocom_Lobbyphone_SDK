
#ifndef __AW_MPP_H__
#define __AW_MPP_H__
#include <adecoder.h>
#include <cdx_list.h>
#include <media/mm_comm_aio.h>
#include <media/mm_comm_vi.h>
#include <media/mpi_sys.h>
#include <utils/media_common_aio.h>
#include <pthread.h>

#include "doorlock_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mpp_wave_header_s {
    int riff_id;
    int riff_sz;
    int riff_fmt;
    int fmt_id;
    int fmt_sz;
    short audio_fmt;
    short num_chn;
    int sample_rate;
    int byte_rate;
    short block_align;
    short bits_per_sample;
    int data_id;
    int data_sz;
} mpp_wave_header_t;

typedef struct aio_config_s {
    uint32_t frame_size;  // in frames
    uint32_t channel_cnt;
    uint32_t sample_rate;
    uint32_t bit_width;
    int ai_aec_en;
    int aec_delay_ms;
    int ai_ans_en;
    int ai_ans_mode;
    int ai_agc_en;
    int ai_agc_gain;  // gain_level : 0,1,2; the higher the level is,the the gain will more power
} aio_config_t;

typedef struct mpp_mem_info_s {
    unsigned int width;
    unsigned int height;
    unsigned int v4l2_format;
    unsigned int max_size;
    unsigned int cur_size;
    unsigned char *vir_addr;
    unsigned int phy_addr;
    bool cache;
} mpp_mem_info_t;

typedef struct mpp_mem_data_s {
    uint32_t mem_id;
    mpp_mem_info_t mpp_mem;
    struct list_head list;
} mpp_mem_data_t;

typedef struct mpp_frm_manager_s {
    struct list_head frm_list_idle;
    struct list_head frm_list_using;
    pthread_mutex_t frm_list_lock;
    uint32_t frm_node_memsize;
    uint32_t frm_node_cnt;
    int (*prefetch_first_idle_frame)(struct mpp_frm_manager_s *mpp_frm_manager,
                                     mpp_mem_data_t **frame);
    int (*first_idle_to_using_frame)(struct mpp_frm_manager_s *mpp_frm_manager,
                                     mpp_mem_data_t *frame);
    int (*prefetch_first_using_frame)(struct mpp_frm_manager_s *mpp_frm_manager,
                                      mpp_mem_data_t **frame);
    int (*first_using_to_idle_frame)(struct mpp_frm_manager_s *mpp_frm_manager,
                                     mpp_mem_data_t *frame);
    int (*release_using_frame)(struct mpp_frm_manager_s *mpp_frm_manager, unsigned int frame_id);
} mpp_frm_manager_t;


int mpp_init(void);
int mpp_deinit(void);
int mpp_mem_open(void);
int mpp_mem_close(void);
int mpp_mem_malloc(mpp_mem_info_t *pMemInfo);
int mpp_mem_free(mpp_mem_info_t *pMemInfo);
int mpp_frm_manager_init(mpp_frm_manager_t *mpp_frm_manager);
int mpp_frm_manager_deinit(mpp_frm_manager_t *mpp_frm_manager);
int mpp_config_audio_frame(unsigned int frame_id, AUDIO_FRAME_S *audio_frame, int sample_rate,
                           int bit_width, int ch_cnt);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
