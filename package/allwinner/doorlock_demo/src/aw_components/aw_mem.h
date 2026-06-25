#ifndef __AW_MEM_H__
#define __AW_MEM_H__

#include "doorlock_common.h"
#include <cdx_list.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

typedef struct aw_mem_info_s {
	unsigned int mem_size;
	unsigned char *mem_vir;
	unsigned int mem_phy;
	bool mem_cache;
} aw_mem_info_t;

int aw_mem_open(void);
int aw_mem_close(void);
int aw_mem_malloc(aw_mem_info_t *mem_info);
int aw_mem_free(aw_mem_info_t *mem_info);

typedef struct frame_mem_s {
    uint32_t mem_id;
    uint32_t cur_mem_size;
    aw_mem_info_t mem_info;
    struct list_head list;
} frame_mem_t;

typedef struct frm_manager_s {
    struct list_head frm_list_idle;
    struct list_head frm_list_using;
    pthread_mutex_t frm_list_lock;
    uint32_t frm_node_memsize;
    uint32_t frm_node_cnt;
    int (*prefetch_first_idle_frame)(struct frm_manager_s *frm_manager, frame_mem_t** frame);
    int (*first_idle_to_using_frame)(struct frm_manager_s *frm_manager, frame_mem_t *frame);
    int (*prefetch_first_using_frame)(struct frm_manager_s *frm_manager, frame_mem_t** frame);
    int (*first_using_to_idle_frame)(struct frm_manager_s *frm_manager, frame_mem_t *frame);
    int (*clear_all_using_frame)(struct frm_manager_s *frm_manager);
    int (*release_using_frame)(struct frm_manager_s *frm_manager, unsigned int nFrameId);
}frm_manager_t;

int frm_manager_clear_all_using_frame(frm_manager_t *frm_manager);
int frm_manager_init(frm_manager_t *frm_manager);
int frm_manager_deinit(frm_manager_t *frm_manager);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif //  #ifndef __QG_MEM_H__
