#ifndef __PERSION_DETECT_H__
#define __PERSION_DETECT_H__

#include "rt_aiservice_pdet.h"

struct persion_detect_callback {
    int (*set_osd)(void *osd);
    int (*set_orl)(void *orl);
    void (*notify)(Awnn_Result_t *result);
};

struct persion_detect_config {
    int attach_debug_osd;
    struct persion_detect_callback callback;
};

int persion_detect_init(struct persion_detect_config *config);
void persion_detect_destroy(void);
void persion_detect_start(void);
int persion_detect_result_get(Awnn_Result_t *result);
void persion_detect_start_async(void);
void persion_detect_stop_async(void);

#endif
