#include "cpu_i.h"

#define L1_CACHE_BYTES			(64)

void exit(s32 i)
{
	printk("system exit\n");
	while (1);
}

u32 get_cycles(void)
{
#if 0
	u32 hi;
	u32 lo;

	asm volatile("csrr %0, 0xc01":"=r"(lo)::"memory");
	asm volatile("csrr %0, 0xc81":"=r"(hi)::"memory");

	return ((u64)hi << 32) | lo;
#endif

	u32 lo;

	//asm volatile("csrr %0, 0xc01":"=r"(lo)::"memory");
	lo = *(volatile u32 *)(0x08110000);

	return lo;
}

u32 get_time_us(void)
{
	return get_cycles() / 24000;
}

void udelay(u32 us)
{
	u32 cyc = get_cycles();
	u32 tmp;

	cyc += us * 24;

	do {
		tmp = get_cycles();
	} while (cyc < tmp);
}

void dcache_invalid(unsigned long start, unsigned long end)
{
	unsigned long i = start & ~(L1_CACHE_BYTES - 1);

	for (; i < end; i += L1_CACHE_BYTES)
		asm volatile("dcache.ipa %0\n"::"r"(i):"memory");
	asm volatile("fence");
}

void dcache_clean(unsigned long start, unsigned long end)
{
	unsigned long i = start & ~(L1_CACHE_BYTES - 1);

	for (; i < end; i += L1_CACHE_BYTES)
		asm volatile("dcache.cpa %0\n"::"r"(i):"memory");
	asm volatile("fence");
}

void dcache_invalid_all(void)
{
	asm volatile("dcache.iall\n":::"memory");
}

void dcache_clean_all(void)
{
	asm volatile("dcache.call\n":::"memory");
}
