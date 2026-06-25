#include <stdint.h>
#include <time.h>
#include <os_net_utils.h>
#include <os_net_sem.h>

os_net_status_t os_net_sem_create(os_net_sem_t *sem, uint32_t init_count, uint32_t max_count)
{
    if (sem_init(sem, 0, init_count) != 0)
        return OS_NET_STATUS_NOMEM;
    return OS_NET_STATUS_OK;
}

os_net_status_t os_net_sem_delete(os_net_sem_t *sem)
{
    if (sem_destroy(sem) != 0) {
        return OS_NET_STATUS_FAILED;
    }
    return OS_NET_STATUS_OK;
}

os_net_status_t os_net_sem_wait(os_net_sem_t *sem, os_net_time_t wait_ms)
{
	struct timespec ts;
	if(wait_ms == 0 || wait_ms == OS_NET_WAIT_FOREVER) {
		if(sem_wait(sem) != 0)
			return OS_NET_STATUS_FAILED;
		return OS_NET_STATUS_OK;
	}

	if(clock_gettime(CLOCK_REALTIME, &ts) < 0) {
		return OS_NET_STATUS_FAILED;
	}

	ts.tv_sec += wait_ms / 1000;
	ts.tv_nsec += wait_ms % 1000 * 1000;

	if (sem_timedwait(sem, (const struct timespec *) &ts) != 0)
		return OS_NET_STATUS_FAILED;

    return OS_NET_STATUS_OK;
}

os_net_status_t os_net_sem_release(os_net_sem_t *sem)
{
    if (sem_post(sem) != 0) {
        return OS_NET_STATUS_FAILED;
    }
    return OS_NET_STATUS_OK;
}
