#include "awaiisp_rp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hal_timer.h>
#include "awaiisp_common.h"

static void print_awaiisp_common_config(struct RT_AWAIISP_CONFIG *config)
{
	awaiisp_common_dbg_print("size %dx%d tdm buffer %d ion %d mode %d unprepared %d ref %d\n",
			config->width, config->height, config->tdm_rxbuf_cnt,
			config->ion_mem_open, config->mode, config->unprepared_aiisp_resources_advance, config->npu_ref_buf_reduce_enable);
}

static void print_awaiisp_switch_config(struct RT_AWAIISP_SWITCH_CONFIG *config)
{
	awaiisp_common_dbg_print(KERN_EMERG "mode %d timeout %d release_aiisp_resource %d\n",
        config->mode, config->timeout, config->release_aiisp_resources);
}

static int awaiisp_parser_rpmsg_switch_config(RT_AWAIISP_SWITCH_CONFIG *rpmsg_switch_config, awaiisp_switch_param *param)
{
    if (!rpmsg_switch_config) {
        awaiisp_common_err_print("rpmsg_switch_config is NULL!!!!\n");
        return -1;
    }

    param->mode = rpmsg_switch_config->mode;
    param->timeout = rpmsg_switch_config->timeout;
    param->release_aiisp_resources = rpmsg_switch_config->release_aiisp_resources;

	awaiisp_common_dbg_print("mode = %d, timeout = %d, release_aiisp_resources = %d,\n",
			param->mode,
			param->timeout,
            param->release_aiisp_resources);

    return 0;
}

static int awaiisp_parser_rpmsg_common_config(RT_AWAIISP_CONFIG *rpmsg_common_config, awaiisp_common_config_param *param)
{
    if (!rpmsg_common_config) {
        awaiisp_common_err_print("rpmsg_common_config is NULL!!!!\n");
        return -1;
    }

    param->config.width = rpmsg_common_config->width;
    param->config.height = rpmsg_common_config->height;
    param->config.tdm_rxbuf_cnt = 5;
    param->config.npu_init_buf_size = rpmsg_common_config->npu_init_buf_size;
    param->config.ion_mem_open = rpmsg_common_config->npu_init_buf_size;
    param->config.mode = rpmsg_common_config->mode;
    param->config.unprepared_aiisp_resources_advance = rpmsg_common_config->unprepared_aiisp_resources_advance;
    param->config.npu_ref_buf_reduce_enable = 0;

	awaiisp_common_dbg_print("%dx%d tdm_buf = %d, ion = %d, mode = %d, unprepared = %d, ref = %d\n",
			param->config.width, param->config.height,param->config.tdm_rxbuf_cnt,
			param->config.ion_mem_open, param->config.mode,
            param->config.unprepared_aiisp_resources_advance, param->config.npu_ref_buf_reduce_enable);

    return 0;
}

static int awaiisp_rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
				size_t len, uint32_t src, void *priv)
{
	struct rt_rp_msg *msg = (struct rt_rp_msg *)data;

	if (!data) {
		awaiisp_common_err_print("callback data is null!\n");
		return -1;
	}

	switch (msg->type) {
		case RT_RP_MSG_AWAIISP_SET_COMMON_PARAM: {

			awaiisp_common_dbg_print("RT_RP_MSG_AWAIISP_SET_COMMON_PARAM\n");

			struct rt_rp_awaiisp_common_param *param = (struct rt_rp_awaiisp_common_param *)data;
			print_awaiisp_common_config(&param->config);
			awaiisp_common_config_param common_param;
			memset(&common_param, 0, sizeof(awaiisp_common_config_param));
			awaiisp_parser_rpmsg_common_config(&param->config, &common_param);
			param->isp = 0;
			if (param->enable) {
				awaiisp_common_enable(param->isp, &common_param);
			} else {
				awaiisp_common_disable(param->isp);

				struct rt_rp_awaiisp_notify_msg msg;
				memset(&msg, 0, sizeof(msg));
				msg.length = sizeof(msg);
				msg.type   = RT_RP_MSG_AWAIISP_NOTIFY_INFO;
				strncpy(msg.msg, "awaiisp_common_disable_done", sizeof(msg.msg));
				int ret = openamp_rpmsg_send(ept, (void *)&msg, sizeof(msg));
				if (ret != sizeof(msg))
					awaiisp_common_err_print("Filed to send data\n");
			}
			break;
		};
		case RT_RP_MSG_AWAIISP_SET_SWITCH_PARAM: {
			struct rt_rp_awaiisp_switch_param *param= (struct rt_rp_awaiisp_switch_param *)data;
			print_awaiisp_switch_config(&param->config);

			awaiisp_common_switch_param switch_param;
			memset(&switch_param, 0, sizeof(awaiisp_common_switch_param));
			awaiisp_parser_rpmsg_switch_config(&param->config, &switch_param.config);
			param->isp = 0;
			// awaiisp_common_switch_mode(param->isp, &switch_param);
			break;
		};
		default: {
			awaiisp_common_err_print("invalid msg type %d\n", msg->type);
			break;
		}
	};

	return 0;
}

static void awaiisp_rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	struct rpmsg_demo_private *demo_priv = ept->priv;

	awaiisp_common_dbg_print("Remote endpoint is destroyed\n");
}

void awaiisp_test_proc(struct awaiisp_test_context *awaiisp_test_context)
{
	awaiisp_common_dbg_print("===== start awaiisp test proc =====\n");
	if (openamp_init() != 0) {
		awaiisp_common_err_print(KERN_EMERG "Failed to init openamp framework\r\n");
		return;
	}

	awaiisp_common_dbg_print("awaiisp test proc start\n");
	awaiisp_test_context->ept = openamp_ept_open(RT_RP_AWAIISP_ISP0, 0, RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
							awaiisp_test_context, awaiisp_rpmsg_ept_callback, awaiisp_rpmsg_unbind_callback);

	while (1) {
		if (awaiisp_test_context->proc_status == -1) {
			awaiisp_common_err_print("recv stop signal!\n");
			break;
		}

		// struct rt_rp_awaiisp_notify_msg msg;
		// memset(&msg, 0, sizeof(msg));
		// msg.length = sizeof(msg);
		// msg.type   = RT_RP_MSG_AWAIISP_NOTIFY_ERROR;
		// strncpy(msg.msg, "wait tdm buffer timeout!", sizeof(msg.msg));

		// int ret = openamp_rpmsg_send(awaiisp_test_context->ept, (void *)&msg, sizeof(msg));
		// if (ret != sizeof(msg))
		// 	awaiisp_common_err_print("Filed to send data\n");

		hal_msleep(1000);
	}

	openamp_ept_close(awaiisp_test_context->ept);
}
