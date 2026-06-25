#ifndef __MSG_NETLINK_H__
#define __MSG_NETLINK_H__

#include "log_core.h"
#include "os_net_utils.h"
#include "os_net_thread.h"
#include "os_net_sem.h"

#define NLMSG_PROTO 28

#define OS_NET_TASK_STACK_SIZE  (4096)
#define OS_NET_TASK_PRIO        (0)

typedef int (*recv_cb_t)(char *data,uint32_t len);

typedef struct {
	os_net_thread_t thread;
    os_net_sem_t sem;
	recv_cb_t recv_cb;
#define TX_RX_MAX_LEN 1503
	char *rx;
	int fd;
	os_net_thread_pid_t rx_pid;
} nlmsg_priv_t;


nlmsg_priv_t *nlmsg_init(recv_cb_t cb);
os_net_status_t nlmsg_deinit(nlmsg_priv_t *priv);
os_net_status_t nlmsg_send(nlmsg_priv_t *priv, char *data, uint32_t len);
#endif
