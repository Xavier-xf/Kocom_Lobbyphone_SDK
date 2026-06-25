#include "aw_mem.h"

#include <AW_VideoInput_API.h>
#include <asm/types.h>
#include <errno.h>
#include <linux/stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <ion_mem_alloc.h>

static struct SunxiMemOpsS *mem_ops;
static volatile int g_mem_open_cnt = 0;

int aw_mem_open(void)
{
    if (g_mem_open_cnt == 0) {
        AWVideoInput_Init();
    }

    g_mem_open_cnt++;

    return 0;
}

int aw_mem_close(void)
{
    if (g_mem_open_cnt == 1) {
        AWVideoInput_DeInit();
    }

    if (g_mem_open_cnt) g_mem_open_cnt--;

    return 0;
}

int aw_mem_malloc(aw_mem_info_t *mem_info)
{
    int fd = -1, ret = 0;

    mem_ops = GetMemAdapterOpsS();
    ret = SunxiMemOpen(mem_ops);
    if (ret < 0) {
        DOORLOCK_ERR("ION Open failed\n");
        return ret;
    }

    mem_info->mem_vir = SunxiMemPalloc(mem_ops, (uint32_t)mem_info->mem_size);
    if (mem_info->mem_vir == NULL) {
        DOORLOCK_ERR("ION Malloc failed\n");
        return -1;
    }

    mem_info->mem_phy = (uint32_t)SunxiMemGetPhysicAddressCpu(mem_ops, mem_info->mem_vir);

    fd = SunxiMemGetBufferFd(mem_ops, mem_info->mem_vir);
    if (fd < 0) {
        DOORLOCK_ERR("ION get dmabuf-fd failed\n");
        return -1;
    }

    DOORLOCK_DBG("ION_IOC_ALLOC succes, dmabuf-fd = %d, size = %d\n", fd, mem_info->mem_size);

    return fd;
}

int aw_mem_free(aw_mem_info_t *mem_info)
{
    SunxiMemPfree(mem_ops, mem_info->mem_vir);
    SunxiMemClose(mem_ops);

    return 0;
}
static int frm_manager_prefetch_first_idle_frame(frm_manager_t *frm_manager, frame_mem_t **frame)
{
    int ret = 0;

    pthread_mutex_lock(&frm_manager->frm_list_lock);
    if (!list_empty(&frm_manager->frm_list_idle)) {
        // if frm_list_idle has node, get the first one
        frame_mem_t *first_node = list_first_entry(&frm_manager->frm_list_idle, frame_mem_t, list);
        *frame = first_node;  // only prefetch, don't change status
    } else {
        *frame = NULL;
        // DOORLOCK_ERR("fatal error! no idle frame found\n");
        ret = -1;
    }
    pthread_mutex_unlock(&frm_manager->frm_list_lock);
    return ret;
}

static int frm_manager_first_idle_to_using_frame(frm_manager_t *frm_manager, frame_mem_t *frame)
{
    int ret = 0;

    pthread_mutex_lock(&frm_manager->frm_list_lock);
    frame_mem_t *first_node =
        list_first_entry(&frm_manager->frm_list_idle, frame_mem_t, list);  // or_null
    if (first_node) {
        if (first_node == frame) {
            list_move_tail(&first_node->list, &frm_manager->frm_list_using);
        } else {
            // DOORLOCK_ERR("fatal error! node is not match [%p]!=[%p]\n", frame,
            // first_node);
            ret = -1;
        }
    } else {
        // DOORLOCK_ERR("fatal error! idle list is empty\n");
        ret = -1;
    }
    pthread_mutex_unlock(&frm_manager->frm_list_lock);
    return ret;
}

static int frm_manager_prefetch_first_using_frame(frm_manager_t *frm_manager, frame_mem_t **frame)
{
    int ret = 0;

    pthread_mutex_lock(&frm_manager->frm_list_lock);
    if (!list_empty(&frm_manager->frm_list_using)) {
        // if frm_list_using has node, get the first one
        frame_mem_t *first_node = list_first_entry(&frm_manager->frm_list_using, frame_mem_t, list);
        *frame = first_node;  // only prefetch, don't change status
    } else {
        *frame = NULL;
        // DOORLOCK_ERR("fatal error! no using frame found\n");
        ret = -1;
    }
    pthread_mutex_unlock(&frm_manager->frm_list_lock);
    return ret;
}

static int frm_manager_first_using_to_idle_frame(frm_manager_t *frm_manager, frame_mem_t *frame)
{
    int ret = 0;

    pthread_mutex_lock(&frm_manager->frm_list_lock);
    frame_mem_t *first_node =
        list_first_entry(&frm_manager->frm_list_using, frame_mem_t, list);  // or_null
    if (first_node) {
        if (first_node == frame) {
            list_move_tail(&first_node->list, &frm_manager->frm_list_idle);
        } else {
            // DOORLOCK_ERR("fatal error! node is not match [%p]!=[%p]\n", frame,
            // first_node);
            ret = -1;
        }
    } else {
        // DOORLOCK_ERR("fatal error! using list is empty\n");
        ret = -1;
    }
    pthread_mutex_unlock(&frm_manager->frm_list_lock);
    return ret;
}

static int frm_manager_release_using_frame(frm_manager_t *frm_manager, unsigned int frame_id)
{
    int ret = 0;

    pthread_mutex_lock(&frm_manager->frm_list_lock);
    int find_flag = 0;
    frame_mem_t *entry, *tmp;

    list_for_each_entry_safe(entry, tmp, &frm_manager->frm_list_using, list)
    {
        if (entry->mem_id == frame_id) {
            list_move_tail(&entry->list, &frm_manager->frm_list_idle);
            // DOORLOCK_DBG("mem_id [%d] is find\n", frame_id);
            find_flag = 1;
            break;
        }
    }
    if (0 == find_flag) {
        // DOORLOCK_ERR("fatal error! mem_id [%d] is not find\n", frame_id);
        ret = -1;
    }
    pthread_mutex_unlock(&frm_manager->frm_list_lock);
    return ret;
}

int frm_manager_clear_all_using_frame(frm_manager_t *frm_manager)
{
    int ret = 0;
    frame_mem_t *entry, *tmp;

    pthread_mutex_lock(&frm_manager->frm_list_lock);
    list_for_each_entry_safe(entry, tmp, &frm_manager->frm_list_using, list)
    {
        list_move_tail(&entry->list, &frm_manager->frm_list_idle);
    }
    pthread_mutex_unlock(&frm_manager->frm_list_lock);
    return 0;
}

int frm_manager_init(frm_manager_t *frm_manager)
{
    INIT_LIST_HEAD(&frm_manager->frm_list_using);
    INIT_LIST_HEAD(&frm_manager->frm_list_idle);
    frm_manager->prefetch_first_idle_frame = frm_manager_prefetch_first_idle_frame;
    frm_manager->first_idle_to_using_frame = frm_manager_first_idle_to_using_frame;
    frm_manager->prefetch_first_using_frame = frm_manager_prefetch_first_using_frame;
    frm_manager->first_using_to_idle_frame = frm_manager_first_using_to_idle_frame;
    frm_manager->clear_all_using_frame = frm_manager_clear_all_using_frame;
    frm_manager->release_using_frame = frm_manager_release_using_frame;  // Callback
    DOORLOCK_DBG("=====\n");
    pthread_mutex_init(&frm_manager->frm_list_lock, NULL);
    if (frm_manager->frm_node_memsize > 0) {
        for (int i = 0; i < frm_manager->frm_node_cnt; i++) {
            frame_mem_t *buf_tmp = malloc(sizeof(frame_mem_t));
            if (buf_tmp == NULL) {
                DOORLOCK_ERR("malloc %d error\n", sizeof(frame_mem_t));
                return -1;
            }
            buf_tmp->mem_id = i;
            buf_tmp->mem_info.mem_cache = 0;
            buf_tmp->mem_info.mem_size = frm_manager->frm_node_memsize;
            DOORLOCK_DBG("[%d] aw_mem_malloc len = %d\n", i, frm_manager->frm_node_memsize);
            aw_mem_malloc(&buf_tmp->mem_info);
            list_add_tail(&buf_tmp->list, &frm_manager->frm_list_idle);
        }
    }
    return 0;
}

int frm_manager_deinit(frm_manager_t *frm_manager)
{
    DOORLOCK_DBG("=====\n");
    pthread_mutex_lock(&frm_manager->frm_list_lock);
    if (frm_manager->frm_node_memsize > 0) {
        frame_mem_t *buf_tmp;
        frame_mem_t *buf_tmp_next;
        int cnt = 0;
        // maybe we should wait until release all(using empty)??
        list_for_each_entry_safe(buf_tmp, buf_tmp_next, &frm_manager->frm_list_using, list)
        {
            list_del(&buf_tmp->list);
            DOORLOCK_DBG("[%d] aw_mem_free\n", buf_tmp->mem_id);
            aw_mem_free(&buf_tmp->mem_info);
            free(buf_tmp);
        }
        list_for_each_entry_safe(buf_tmp, buf_tmp_next, &frm_manager->frm_list_idle, list)
        {
            list_del(&buf_tmp->list);
            DOORLOCK_DBG("[%d] aw_mem_free\n", buf_tmp->mem_id);
            aw_mem_free(&buf_tmp->mem_info);
            free(buf_tmp);
        }
    }
    pthread_mutex_unlock(&frm_manager->frm_list_lock);
    pthread_mutex_destroy(&frm_manager->frm_list_lock);
    return 0;
}
