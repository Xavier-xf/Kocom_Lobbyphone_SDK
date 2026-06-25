/*
* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
*
* Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
* the the People's Republic of China and other countries.
* All Allwinner Technology Co.,Ltd. trademarks are used with permission.
*
* DISCLAIMER
* THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
* IF YOU NEED TO INTEGRATE THIRD PARTY’S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
* IN ALLWINNERS’SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
* ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
* ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
* COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
* YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY’S TECHNOLOGY.
*
*
* THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
* PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
* WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
* THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
* OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
* IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
* NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
* OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include <hal_sem.h>
#include <hal_mem.h>
#include <hal_status.h>
#include <rtdef.h>
#include <log.h>
#include <sunxi_hal_common.h>

void hal_sem_init(hal_sem_t sem, unsigned int cnt)
{
	rt_err_t ret;

	sem->ptr = &sem->entry;
	ret = rt_sem_init(&sem->entry, "hal_sem", cnt, RT_IPC_FLAG_FIFO);

	hal_assert(ret == RT_EOK);
}

void hal_sem_deinit(hal_sem_t sem)
{
	rt_sem_detach(sem->ptr);
	sem->ptr = NULL;
}

hal_sem_t hal_sem_create(unsigned int cnt)
{
	hal_sem_t sem;

	sem = hal_malloc(sizeof(*sem));
	if (!sem)
		return NULL;

	hal_sem_init(sem, cnt);
	return sem;
}

int hal_sem_delete(hal_sem_t sem)
{
	hal_assert(sem != NULL);

	hal_sem_deinit(sem);

	return HAL_OK;
}

int hal_sem_getvalue(hal_sem_t sem, int *val)
{
	hal_assert(sem != NULL);
	hal_assert(val != NULL);

	rt_sem_control(sem->ptr, RT_IPC_CMD_GET_STATE, val);

	return HAL_OK;
}

int hal_sem_post(hal_sem_t sem)
{
	hal_assert(sem != NULL)

	rt_sem_release(sem->ptr);

	return HAL_OK;
}

int hal_sem_timedwait(hal_sem_t sem, unsigned long ticks)
{
	rt_err_t ret;

	hal_assert(sem != NULL)
	ret = rt_sem_take(sem->ptr, ticks);
	if (ret != RT_EOK) {
		// timeout.
		return HAL_TIMEOUT;
	}
	return HAL_OK;
}

int hal_sem_trywait(hal_sem_t sem)
{
	return hal_sem_timedwait(sem, 0);
}

int hal_sem_wait(hal_sem_t sem)
{
	return hal_sem_timedwait(sem, RT_WAITING_FOREVER);
}

int hal_sem_clear(hal_sem_t sem)
{
	rt_err_t ret;

	hal_assert(sem != NULL);
	ret = rt_sem_control(sem->ptr, RT_IPC_CMD_RESET, NULL);
	if (ret != RT_EOK) {
		__err("rt_sem_control fail\n");
		return HAL_ERROR;
	}
	return HAL_OK;
}
