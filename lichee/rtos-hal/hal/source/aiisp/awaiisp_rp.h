#ifndef __AWAIISP_RP_H__
#define __AWAIISP_RP_H__

#include <openamp/sunxi_helper/openamp.h>

#define RT_RP_EP_NUM			1
#define RT_RP_AWAIISP_ISP0		"sunxi,rt_media-awaiisp0"
#define RT_RP_AWAIISP_ISP1		"sunxi,rt_media-awaiisp1"
#define RT_RP_AWAIISP_ISP2		"sunxi,rt_media-awaiisp2"

enum rt_rp_msg_type {
	RT_RP_MSG_AWAIISP_SET_COMMON_PARAM = 0,
	RT_RP_MSG_AWAIISP_GET_COMMON_PARAM,
	RT_RP_MSG_AWAIISP_SET_SWITCH_PARAM,
	RT_RP_MSG_AWAIISP_GET_SWITCH_PARAM,
	RT_RP_MSG_AWAIISP_NOTIFY_INFO,
	RT_RP_MSG_AWAIISP_NOTIFY_WARNING,
	RT_RP_MSG_AWAIISP_NOTIFY_ERROR,
};

struct rt_rp_msg {
	unsigned int length;
	unsigned int type;
};

typedef struct RT_AWAIISP_CONFIG {
	int width;
	int height;
	int tdm_rxbuf_cnt;
	int npu_init_buf_size;
	int ion_mem_open;
	int mode;
	int unprepared_aiisp_resources_advance;
	int npu_ref_buf_reduce_enable;
	int reserve0;
	int reserve1;
} RT_AWAIISP_CONFIG;

struct rt_rp_awaiisp_common_param {
	unsigned int length;
	int type;
	int isp;
	int enable;
	RT_AWAIISP_CONFIG config;
};

typedef struct RT_AWAIISP_SWITCH_CONFIG {
	int mode;
	int timeout;
	int release_aiisp_resources;
} RT_AWAIISP_SWITCH_CONFIG;

struct rt_rp_awaiisp_switch_param {
	unsigned int length;
	int type;
	int isp;
	RT_AWAIISP_SWITCH_CONFIG config;
};

struct rt_rp_awaiisp_notify_msg {
	unsigned int length;
	unsigned int type;
	char msg[256];
};

enum rt_rp_ep_type {
	RT_MEDIA_RPMSG_EP_TYPE_AWAIISP = 0,
	RT_MEDIA_RPMSG_EP_TYPE_RESERVED,
};

struct rt_rp_ep {
	int index;
	int channel;
	enum rt_rp_ep_type type;
	char name[RPMSG_NAME_SIZE];
	void *rpdev;
	void (*rt_rp_callback)(struct rt_rp_ep *ep, void *data, unsigned int len);
};

struct awaiisp_test_context {
	int proc_status;
	struct rpmsg_endpoint *ept;
};
// struct awaiisp_test_context awaiisp_test_context;

struct rt_rp_ep *rt_rp_request_ep(int channel);
void rt_rp_register_callback(struct rt_rp_ep *ep, void *callback);
int rt_rp_send_msg(struct rt_rp_ep *ep, void *buf, unsigned int len);

void awaiisp_test_proc(struct awaiisp_test_context *awaiisp_test_context);
#endif
