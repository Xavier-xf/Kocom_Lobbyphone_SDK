/*
*********************************************************************************************************
*                                                AR100 SYSTEM
*                                     AR100 Software System Develop Kits
*                                                 cpu module
*
*                                    (c) Copyright 2012-2016, Sunny China
*                                             All Rights Reserved
*
* File    : time_ticks.c
* By      : Sunny
* Version : v1.0
* Date    : 2012-7-9
* Descript: cpu time ticks.
* Update  : date                auther      ver     notes
*           2012-7-9 14:20:22   Sunny       1.0     Create this file.
*********************************************************************************************************
*/

//#include "cpu_i.h"
#include "include.h"

static HANDLE htimer;
volatile static u32 time_ticks;

static struct softtimer *softtimer[SOFTTIMER_NUMBER];
static u32 softtimer_cnt;
extern volatile u32 timer_lock;

void start_softtimer(struct softtimer *timer)
{
	timer->expires = timer->cycle;
	timer->start = SOFTTIMER_ON;
}

void stop_softtimer(struct softtimer *timer)
{
	timer->expires = 0;
	timer->start = SOFTTIMER_OFF;
}

s32 add_softtimer(struct softtimer *timer)
{
	if (timer_lock)
		return -EACCES;

	if (softtimer_cnt >= SOFTTIMER_NUMBER)
		return -EINVAL;

	softtimer[softtimer_cnt] = timer;
	softtimer_cnt++;

	return OK;
}

static s32 timer_tick_server(void *parg)
{
	u32 i;

	time_ticks++;
	for (i = 0; (i < SOFTTIMER_NUMBER) && softtimer[i]; i++) {
		if (softtimer[i]->start == SOFTTIMER_OFF)
			continue;
		if (softtimer[i]->expires > 0) {
			softtimer[i]->expires--;
		} else if (softtimer[i]->expires == 0) {
			softtimer[i]->expires = softtimer[i]->cycle;
			if (softtimer[i]->cb)
				(softtimer[i]->cb) (softtimer[i]->arg);
		}
	}

	return OK;
}

u32 current_time_tick(void)
{
	return time_ticks;
}

u32 msec_to_ticks(u32 ms)
{
	return (TICK_PER_SEC * ms) / 1000;
}

s32 time_ticks_init(void)
{
	/*startup period timer for system timer tick */
	htimer = timer_request(timer_tick_server, NULL);
	if (htimer) {
		u32 period;

		/*start sysem time tick. period base on ms. */
		time_ticks = 0;
		period = 1000 / TICK_PER_SEC;
		timer_start(htimer, period, TIMER_MODE_PERIOD);

		INF("setup timer server succeeded\n");
		return OK;
	}
	/*request timer failed */
	return -EFAIL;
}
