#ifndef NPU_RPMSG_H
#define NPU_RPMSG_H

#define DEBUG_RPMSG_SEND 0
#define DEBUG_RPMSG_TIME 0

int npu_preview_enhancer_rpmsg_init(void);

int rpmsg_deinterlace_remote_call(void *data, int len);

#endif  //NPU_RPMSG_H
