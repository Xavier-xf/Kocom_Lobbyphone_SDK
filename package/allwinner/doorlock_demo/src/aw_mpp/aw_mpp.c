#include "aw_mpp.h"

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "aw_mem.h"

static volatile int g_mpp_open_cnt = 0;
static MPP_SYS_CONF_S mpp_sys_config;

int mpp_init(void)
{
    int ret = 0;

    if (g_mpp_open_cnt == 0) {
        memset(&mpp_sys_config, 0, sizeof(MPP_SYS_CONF_S));
        mpp_sys_config.nAlignWidth = 32;
        AW_MPI_SYS_SetConf(&mpp_sys_config);
        ret = AW_MPI_SYS_Init();
        if (ret < 0) {
            DOORLOCK_ERR("AW_MPI_SYS_Init failed! ret = %d\n", ret);
            return ret;
        }
        mpp_mem_open();
    }

    g_mpp_open_cnt++;
    return ret;
}

int mpp_deinit(void)
{
    int ret = 0;

    if (g_mpp_open_cnt == 1) {
        mpp_mem_close();
        ret = AW_MPI_SYS_Exit();
        if (ret < 0) {
            DOORLOCK_ERR("AW_MPI_SYS_Exit failed! ret = %d\n", ret);
            return ret;
        }
    }

    if (g_mpp_open_cnt)
        g_mpp_open_cnt--;
    return ret;
}

#define MPP_MEM_TYPE_RAW 0
#define MPP_MEM_TYPE_ION 1
#define MPP_MEM_TYPE_SYS 2

#define MPP_MEM_TYPE MPP_MEM_TYPE_ION

int mpp_mem_open(void)
{
#if (MPP_MEM_TYPE == MPP_MEM_TYPE_ION)
    aw_mem_open();
#endif
    return 0;
}

int mpp_mem_close(void)
{
#if (MPP_MEM_TYPE == MPP_MEM_TYPE_ION)
    aw_mem_close();
#endif
    return 0;
}

int mpp_mem_malloc(mpp_mem_info_t *mpp_mem)
{
#if (MPP_MEM_TYPE == MPP_MEM_TYPE_ION)
    aw_mem_info_t tmp_mem;
    tmp_mem.mem_size = mpp_mem->max_size;
    tmp_mem.mem_cache = mpp_mem->cache;
    aw_mem_malloc(&tmp_mem);
    mpp_mem->phy_addr = tmp_mem.mem_phy;
    mpp_mem->vir_addr = tmp_mem.mem_vir;
#elif (MPP_MEM_TYPE == MPP_MEM_TYPE_SYS)
    AW_MPI_SYS_MmzAlloc_Cached(&mpp_mem->phy_addr, &mpp_mem->vir_addr, mpp_mem->max_size);
#else
    mpp_mem->vir_addr = malloc(mpp_mem->max_size);
#endif
    return 0;
}

int mpp_mem_free(mpp_mem_info_t *mpp_mem)
{
#if (MPP_MEM_TYPE == MPP_MEM_TYPE_ION)
    aw_mem_info_t tmp_mem;
    tmp_mem.mem_size = mpp_mem->max_size;
    tmp_mem.mem_phy = mpp_mem->phy_addr;
    tmp_mem.mem_vir = mpp_mem->vir_addr;
    aw_mem_free(&tmp_mem);
#elif (MPP_MEM_TYPE == MPP_MEM_TYPE_SYS)
    AW_MPI_SYS_MmzFree(mpp_mem->phy_addr, mpp_mem->vir_addr);
#else
    if (mpp_mem->vir_addr) {
        free(mpp_mem->vir_addr);
    }
#endif
    return 0;
}

static int prefetch_first_idle_frame(mpp_frm_manager_t *mpp_frm_manager,
                                    mpp_mem_data_t **frame)
{
    int ret = 0;

    pthread_mutex_lock(&mpp_frm_manager->frm_list_lock);
    if (!list_empty(&mpp_frm_manager->frm_list_idle)) {
        // if frm_list_idle has node, get the first one
        mpp_mem_data_t *first_node =
            list_first_entry(&mpp_frm_manager->frm_list_idle, mpp_mem_data_t, list);
        *frame = first_node;  // only prefetch, don't change status
    } else {
        *frame = NULL;
        // DOORLOCK_ERR("fatal error! no idle frame found\n");
        ret = -1;
    }
    pthread_mutex_unlock(&mpp_frm_manager->frm_list_lock);
    return ret;
}

static int first_idle_to_using_frame(mpp_frm_manager_t *mpp_frm_manager,
                                    mpp_mem_data_t *frame)
{
    int ret = 0;

    pthread_mutex_lock(&mpp_frm_manager->frm_list_lock);
    mpp_mem_data_t *first_node =
        list_first_entry_or_null(&mpp_frm_manager->frm_list_idle, mpp_mem_data_t, list);

    if (first_node) {
        if (first_node == frame) {
            list_move_tail(&first_node->list, &mpp_frm_manager->frm_list_using);
        } else {
            // DOORLOCK_ERR("fatal error! node is not match [%p]!=[%p]\n", frame, first_node);
            ret = -1;
        }
    } else {
        // DOORLOCK_ERR("fatal error! idle list is empty\n");
        ret = -1;
    }
    pthread_mutex_unlock(&mpp_frm_manager->frm_list_lock);
    return ret;
}

static int prefetch_first_using_frame(mpp_frm_manager_t *mpp_frm_manager,
                                                mpp_mem_data_t **frame)
{
    int ret = 0;

    pthread_mutex_lock(&mpp_frm_manager->frm_list_lock);
    if (!list_empty(&mpp_frm_manager->frm_list_using)) {
        // if frm_list_using has node, get the first one
        mpp_mem_data_t *first_node =
            list_first_entry(&mpp_frm_manager->frm_list_using, mpp_mem_data_t, list);
        *frame = first_node;  // only prefetch, don't change status
    } else {
        *frame = NULL;
        // DOORLOCK_ERR("fatal error! no using frame found\n");
        ret = -1;
    }
    pthread_mutex_unlock(&mpp_frm_manager->frm_list_lock);
    return ret;
}

static int first_using_to_idle_frame(mpp_frm_manager_t *mpp_frm_manager,
                                              mpp_mem_data_t *frame)
{
    int ret = 0;

    pthread_mutex_lock(&mpp_frm_manager->frm_list_lock);
    mpp_mem_data_t *first_node =
        list_first_entry_or_null(&mpp_frm_manager->frm_list_using, mpp_mem_data_t, list);
    if (first_node) {
        if (first_node == frame) {
            list_move_tail(&first_node->list, &mpp_frm_manager->frm_list_idle);
        } else {
            // DOORLOCK_ERR("fatal error! node is not match [%p]!=[%p]\n", frame, first_node);
            ret = -1;
        }
    } else {
        // DOORLOCK_ERR("fatal error! using list is empty\n");
        ret = -1;
    }
    pthread_mutex_unlock(&mpp_frm_manager->frm_list_lock);
    return ret;
}

static int release_using_frame(mpp_frm_manager_t *mpp_frm_manager,
                                          unsigned int frame_id)
{
    int ret = 0;
    int find_flag = 0;
    mpp_mem_data_t *entry, *tmp;

    pthread_mutex_lock(&mpp_frm_manager->frm_list_lock);
    list_for_each_entry_safe(entry, tmp, &mpp_frm_manager->frm_list_using, list)
    {
        if (entry->mem_id == frame_id) {
            list_move_tail(&entry->list, &mpp_frm_manager->frm_list_idle);
            // DOORLOCK_INFO("mem_id [%d] is find\n", frame_id);
            find_flag = 1;
            break;
        }
    }
    if (0 == find_flag) {
        // DOORLOCK_ERR("fatal error! mem_id [%d] is not find\n", frame_id);
        ret = -1;
    }
    pthread_mutex_unlock(&mpp_frm_manager->frm_list_lock);
    return ret;
}

int mpp_frm_manager_init(mpp_frm_manager_t *mpp_frm_manager)
{
    DOORLOCK_INFO("=============:\n");
    pthread_mutex_init(&mpp_frm_manager->frm_list_lock, NULL);
    pthread_mutex_lock(&mpp_frm_manager->frm_list_lock);

    INIT_LIST_HEAD(&mpp_frm_manager->frm_list_using);
    INIT_LIST_HEAD(&mpp_frm_manager->frm_list_idle);
    mpp_frm_manager->prefetch_first_idle_frame = prefetch_first_idle_frame;
    mpp_frm_manager->first_idle_to_using_frame = first_idle_to_using_frame;
    mpp_frm_manager->prefetch_first_using_frame = prefetch_first_using_frame;
    mpp_frm_manager->first_using_to_idle_frame = first_using_to_idle_frame;
    mpp_frm_manager->release_using_frame = release_using_frame;  // Callback

    if (mpp_frm_manager->frm_node_memsize > 0) {
        for (int i = 0; i < mpp_frm_manager->frm_node_cnt; i++) {
            mpp_mem_data_t *buf_tmp = malloc(sizeof(mpp_mem_data_t));
            if (buf_tmp == NULL) {
                DOORLOCK_ERR("malloc %d error\n", sizeof(mpp_mem_data_t));
                break;
            }
            buf_tmp->mem_id = i;
            buf_tmp->mpp_mem.max_size = mpp_frm_manager->frm_node_memsize;
            DOORLOCK_INFO("[%d] mpp_mem_malloc len = %d\n", i, mpp_frm_manager->frm_node_memsize);
            mpp_mem_malloc(&buf_tmp->mpp_mem);
            list_add_tail(&buf_tmp->list, &mpp_frm_manager->frm_list_idle);
        }
    }
    pthread_mutex_unlock(&mpp_frm_manager->frm_list_lock);
    return 0;
}

int mpp_frm_manager_deinit(mpp_frm_manager_t *mpp_frm_manager)
{
    DOORLOCK_INFO("=============:\n");
    pthread_mutex_lock(&mpp_frm_manager->frm_list_lock);
    if (mpp_frm_manager->frm_node_memsize > 0) {
        mpp_mem_data_t *buf_tmp;
        mpp_mem_data_t *buf_tmp_next;
        int cnt = 0;
        // maybe we should wait until release all(using empty)??

        list_for_each_entry_safe(buf_tmp, buf_tmp_next, &mpp_frm_manager->frm_list_using, list)
        {
            list_del(&buf_tmp->list);
            DOORLOCK_INFO("[%d] mpp_mem_free\n", buf_tmp->mem_id);
            mpp_mem_free(&buf_tmp->mpp_mem);
            free(buf_tmp);
        }
        list_for_each_entry_safe(buf_tmp, buf_tmp_next, &mpp_frm_manager->frm_list_idle, list)
        {
            list_del(&buf_tmp->list);
            DOORLOCK_INFO("[%d] mpp_mem_free\n", buf_tmp->mem_id);
            mpp_mem_free(&buf_tmp->mpp_mem);
            free(buf_tmp);
        }
    }
    pthread_mutex_unlock(&mpp_frm_manager->frm_list_lock);
    pthread_mutex_destroy(&mpp_frm_manager->frm_list_lock);
    return 0;
}

int mpp_config_audio_frame(unsigned int frame_id, AUDIO_FRAME_S *audio_frame, int sample_rate,
                        int bit_width, int ch_cnt)
{
    AUDIO_SAMPLE_RATE_E rate =
        map_SampleRate_to_AUDIO_SAMPLE_RATE_E(sample_rate);                      // sample_rate/1;
    AUDIO_BIT_WIDTH_E bitwidth = map_BitWidth_to_AUDIO_BIT_WIDTH_E(bit_width);  // bit_width/8 - 1;
    AUDIO_SOUND_MODE_E sound_mode = (ch_cnt == 1) ? AUDIO_SOUND_MODE_MONO : AUDIO_SOUND_MODE_STEREO;
    ;  // ch_cnt - 1;
    memset(audio_frame, 0, sizeof(AUDIO_FRAME_S));
    audio_frame->mSamplerate = rate;
    audio_frame->mBitwidth = bitwidth;
    audio_frame->mSoundmode = sound_mode;
    return 0;
}
