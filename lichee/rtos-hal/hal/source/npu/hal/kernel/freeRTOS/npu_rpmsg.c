#include <hal_osal.h>
#include <hal_sem.h>
#include <hal_cache.h>
#include <hal_msgbox.h>
#include <hal_queue.h>
#include <aw_list.h>
#include <openamp/sunxi_helper/openamp.h>
#include <sys/time.h>

#include <gc_vip_kernel.h>
#include "npu_rpmsg.h"

#define RPMSG_SERVICE_NAME		"sunxi,npu0_rpmsg"

struct rpmsg_npu_private {
	struct rpmsg_endpoint *ept;
};

struct rpmsg_npu_private npu_private;

#define NPU_QUEUE_NUM 2
struct npu_queue_data {
	unsigned int command;
	void *data;
	unsigned int pid;
	unsigned int tid;
};
struct npu_workqueue {
	unsigned int pid;
	hal_queue_t npu_call_queue;
	hal_sem_t npu_call_sem;
	int ret;
	void *handle;
	struct list_head node;
};
LIST_HEAD(g_npu_workqueue);

static void npu_call_thread(void *param)
{
	struct npu_workqueue *wq = (struct npu_workqueue *)param;
	struct npu_queue_data qdata;
	int ret;
	while(true) {
		ret = hal_queue_recv(wq->npu_call_queue, &qdata, -1);
		if (ret != 0) {
			PRINTK_E("hal_queue_recv error queue: %p, pid: %x\n", wq->npu_call_queue, wq->pid);
			continue;
		}
		int len = hal_queue_len(wq->npu_call_queue);
		PRINTK_D("Received msg(%d) pid[%x, %x] command[%d], data:%p\n", len, qdata.pid, qdata.tid, qdata.command, qdata.data);

		wq->ret = gckvip_kernel_call_for_rpmsg(qdata.command, qdata.data);
		hal_sem_post(wq->npu_call_sem);

		if (qdata.command == KERNEL_CMD_AW_GCKVIP_DESTROY) {
			break;
		}
	}
}

static int rpmsg_ept_callback(struct rpmsg_endpoint *ept, void *data,
		size_t len, uint32_t src, void *priv)
{
	int ret = -1;
	struct npu_workqueue* wq;
	unsigned int *msg = (unsigned int *)data;
	struct npu_queue_data qdata;
	qdata.command = msg[0];
	qdata.data = (void *)msg[1];
	qdata.pid = msg[2];
	qdata.tid = msg[3];
	char thread_name[32];
	PRINTK_D("call command[%d], data[%p]\n", qdata.command, qdata.data);
	if (qdata.command == KERNEL_CMD_INIT) {
		gckvip_os_snprint(thread_name, 32, "rpmsg_%08x\0", qdata.pid);
		wq = hal_malloc(sizeof(struct npu_workqueue));
		wq->pid = qdata.pid;
		wq->npu_call_queue = hal_queue_create(thread_name, sizeof(struct npu_queue_data), NPU_QUEUE_NUM);
		wq->npu_call_sem = hal_sem_create(0);
		wq->handle = kthread_create((void *)npu_call_thread, wq, thread_name, 4096, 8);
		if (wq->handle != NULL) {
			kthread_start(wq->handle);
		}
		list_add_tail(&wq->node, &g_npu_workqueue);
	}
	list_for_each_entry(wq, &g_npu_workqueue, node) {
		if (wq->pid == qdata.pid) {
			hal_queue_send(wq->npu_call_queue, &qdata);
			// int len = hal_queue_len(wq->npu_call_queue);
			// PRINTK_D("Send msg(%d) pid[%x, %x] command[%d], data:%p\n", len, qdata.pid, qdata.tid, qdata.command, qdata.data);
			hal_sem_wait(wq->npu_call_sem);
			ret = wq->ret;
			if (qdata.command == KERNEL_CMD_AW_GCKVIP_DESTROY) {
				list_del(&wq->node);
				// TODO join thread or ensure thread destroy
				wq->handle = NULL;
				hal_sem_delete(wq->npu_call_sem);
				hal_queue_delete(wq->npu_call_queue);
				hal_free(wq);
			}
			break;
		}
	}
	PRINTK_D("command[%d], ret[%d]\n", qdata.command, ret);

	return ret;
}

/* melis release rpmsg call. */
static void rpmsg_unbind_callback(struct rpmsg_endpoint *ept)
{
	//struct rpmsg_demo_private *demo_priv = ept->priv;

	PRINTK_D("Remote endpoint is destroyed\n");
}

/* melis send data to linux. */
int rpmsg_deinterlace_remote_call(void *data, int len)
{
	int ret = 0;

	if (!npu_private.ept) {
		PRINTK_D("linux no ok trying to resigter \n");
		if (!npu_private.ept)
			return 0;
	}

	ret = openamp_rpmsg_send(npu_private.ept, data, len);
	if (ret < len) {
		PRINTK_D("%s rpmsg sent fail, cmd is %d\r\n", __func__, *((unsigned int *)data + 0));
		ret = -1;
	}

	PRINTK_D("%s ret=%d\n", __func__, ret);
	return ret;
}


/* rpmsg init, init behind melis openamp_init. */
int npu_preview_enhancer_rpmsg_init(void)
{
	struct rpmsg_endpoint *ept;

	PRINTK_D("init deinterlace rpmsg \n");

	if (npu_private.ept != NULL) {
		PRINTK_D("rpmsg already init \n");
		return -1;
	}

	ept = openamp_ept_open(RPMSG_SERVICE_NAME, 0, RPMSG_ADDR_ANY, RPMSG_ADDR_ANY,
					&npu_private, rpmsg_ept_callback, rpmsg_unbind_callback);
	if (ept == NULL) {
		PRINTK_D("Failed to Create Endpoint\r\n");
		return -1;
	}

	npu_private.ept = ept;

	PRINTK_D("%s ok\r\n", __FUNCTION__);
	return 0;
}

static int on_npu_rpmsg_init(void *param)
{
    int ret;
    openamp_init(); //wait rpmsg init
    PRINTK_D("openamp_init finish!\n");
    ret = npu_preview_enhancer_rpmsg_init();
    if (ret != 0) {
        PRINTK_E("NPU: failed to init rpmsg.\n");
        goto exit;
    }

exit:
	return ret;
}

int npu_rpmsg_init(void)
{

    void *handle = kthread_create((void *)on_npu_rpmsg_init, NULL, "NPU_RPMSG_THREAD", 65536, 8);
    if (handle != NULL) {
        kthread_start(handle);
    }
	return 0;

}
