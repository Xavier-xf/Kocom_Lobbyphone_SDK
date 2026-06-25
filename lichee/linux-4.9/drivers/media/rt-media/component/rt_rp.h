#ifndef __RT_RP_H__
#define __RT_RP_H__

#include <linux/rpmsg.h>
#include "uapi_rt_media.h"

#define RT_RP_EP_NUM			3
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

struct rt_rp_awaiisp_common_param {
	unsigned int length;
	int type;
	int isp;
	int enable;
	RT_AWAIISP_CONFIG config;
};

struct rt_rp_awaiisp_switch_param {
	unsigned int length;
	int type;
	int isp;
	RT_AWAIISP_SWITCH_CONFIG config;
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

struct rt_rp_ep *rt_rp_request_ep(int channel);
void rt_rp_register_callback(struct rt_rp_ep *ep, void *callback);
int rt_rp_send_msg(struct rt_rp_ep *ep, void *buf, unsigned int len);

#endif
