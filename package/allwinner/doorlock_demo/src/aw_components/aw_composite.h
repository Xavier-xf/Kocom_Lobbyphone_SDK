#ifndef __AW_COMPOSITE_H__
#define __AW_COMPOSITE_H__
#include <pthread.h>

#include "doorlock_common.h"
#include "aw_mem.h"
#include "aw_uacdev.h"
#include "aw_uvcout.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct composite_dev_s {
    uint16_t usb_vid;
    uint16_t usb_pid;
    int fast_connect;
    int uvc;
    int uac;
    uvc_out_t *uvc_out_info;
    uac_dev_t *uac_dev_info;
    uint32_t ctrl_interface_set_cur;
    uint32_t force_exit;
    int composite_dev_fd;
    pthread_t composite_dev_thread;
    int composite_dev_state;
    pthread_mutex_t composite_dev_lock;
} composite_dev_t;

int composite_dev_init(composite_dev_t *composite_dev_info);
int composite_dev_deinit(composite_dev_t *composite_dev_info);
int composite_dev_connect(composite_dev_t *composite_dev_info);
int composite_dev_disconnect(composite_dev_t *composite_dev_info);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
