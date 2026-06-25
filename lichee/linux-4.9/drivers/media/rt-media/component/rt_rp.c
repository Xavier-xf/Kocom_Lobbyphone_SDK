#define LOG_TAG "rt_rp"

#include <linux/rpmsg.h>

#include "rt_common.h"
#include "rt_rp.h"

struct rt_rp_ep ep_pool[RT_RP_EP_NUM] = {
	{ .index = 0, .channel = -1, .type = RT_MEDIA_RPMSG_EP_TYPE_AWAIISP, .name = RT_RP_AWAIISP_ISP0, .rpdev = NULL },
	{ .index = 1, .channel = -1, .type = RT_MEDIA_RPMSG_EP_TYPE_AWAIISP, .name = RT_RP_AWAIISP_ISP1, .rpdev = NULL },
	{ .index = 2, .channel = -1, .type = RT_MEDIA_RPMSG_EP_TYPE_AWAIISP, .name = RT_RP_AWAIISP_ISP2, .rpdev = NULL },
};

static int rt_rp_cb(struct rpmsg_device *rpdev, void *data, int len,
					void *priv, u32 src)
{
	int i;
	int find = 0;
	struct rt_rp_ep *ep = NULL;

	for (i = 0; i < RT_RP_EP_NUM; i++) {
		ep = &ep_pool[i];
		if (!strcmp(rpdev->id.name, ep->name)) {
			find = 1;
			ep->rt_rp_callback(ep, data, len);
			break;
		}
	}
	if (!find)
		RT_LOGE("rpmsg endpoint %s invalid!", rpdev->id.name);

	return 0;
}

static int rt_rp_probe(struct rpmsg_device *rpdev)
{
	int i;
	int find = 0;
	struct rt_rp_ep *ep = NULL;

	for (i = 0; i < RT_RP_EP_NUM; i++) {
		ep = &ep_pool[i];
		if (!strcmp(rpdev->id.name, ep->name)) {
			find = 1;
			ep->rpdev = (void *)rpdev;
			break;
		}
	}
	if (!find)
		RT_LOGE("rpmsg endpoint %s invalid!", rpdev->id.name);

	return 0;
}

static void rt_rp_remove(struct rpmsg_device *rpdev)
{
	int i;
	int find = 0;
	struct rt_rp_ep *ep = NULL;

	for (i = 0; i < RT_RP_EP_NUM; i++) {
		ep = &ep_pool[i];
		if (!strcmp(rpdev->id.name, ep->name)) {
			find = 1;
			ep->channel = -1;
			ep->rpdev = NULL;
			break;
		}
	}
	if (!find)
		RT_LOGE("rpmsg endpoint %s invalid!", rpdev->id.name);
}

struct rt_rp_ep *rt_rp_request_ep(int channel)
{
	struct rt_rp_ep *ep = NULL;

	if ((channel == CSI_SENSOR_0_VIPP_0) || (channel == CSI_SENSOR_0_VIPP_1)
		|| (channel == CSI_SENSOR_0_VIPP_2) || (channel == CSI_SENSOR_0_VIPP_3)) {
		ep = &ep_pool[0];
		if (ep->channel >= 0)
			return ep;
		ep->channel = channel;
	} else if ((channel == CSI_SENSOR_1_VIPP_0) || (channel == CSI_SENSOR_1_VIPP_1)
		|| (channel == CSI_SENSOR_1_VIPP_2) || (channel == CSI_SENSOR_1_VIPP_3)) {
		ep = &ep_pool[1];
		if (ep->channel >= 0)
			return ep;
		ep->channel = channel;
	} else if ((channel == CSI_SENSOR_2_VIPP_0) || (channel == CSI_SENSOR_2_VIPP_1)
		|| (channel == CSI_SENSOR_2_VIPP_2) || (channel == CSI_SENSOR_2_VIPP_3)) {
		ep = &ep_pool[2];
		if (ep->channel >= 0)
			return ep;
		ep->channel = channel;
	} else {
		RT_LOGE("invalid channel %d", channel);
	}

	return ep;
}
EXPORT_SYMBOL(rt_rp_request_ep);

void rt_rp_register_callback(struct rt_rp_ep *ep, void *callback)
{
	ep->rt_rp_callback = callback;
}
EXPORT_SYMBOL(rt_rp_register_callback);

int rt_rp_send_msg(struct rt_rp_ep *ep, void *buf, unsigned int len)
{
	if (!ep || !ep->rpdev)
		return -EFAULT;

	struct rpmsg_device *rpdev = (struct rpmsg_device *)ep->rpdev;
	struct rpmsg_endpoint *ept = (struct rpmsg_endpoint *)rpdev->ept;

	return rpmsg_send(ept, buf, len);
}
EXPORT_SYMBOL(rt_rp_send_msg);

struct rpmsg_device_id rt_rp_id_table[] = {
	{ .name = RT_RP_AWAIISP_ISP0 },
	{ .name = RT_RP_AWAIISP_ISP1 },
	{ .name = RT_RP_AWAIISP_ISP2 },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, rt_rp_id_table);

static struct rpmsg_driver rt_rp_driver = {
	.drv.name = KBUILD_MODNAME,
	.id_table = rt_rp_id_table,
	.probe = rt_rp_probe,
	.callback = rt_rp_cb,
	.remove = rt_rp_remove,
};
module_rpmsg_driver(rt_rp_driver);

