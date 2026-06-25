#ifndef __CPU_H__
#define __CPU_H__

#if defined(CONFIG_SUNXI_NCAT_V2)
#include <arch/cpu_ncat_v2.h>
#else
#error "Unsupported plat"
#endif

/* cache operation fucntion */
void dcache_invalid(unsigned long start, unsigned long end);
void dcache_clean(unsigned long start, unsigned long end);
void dcache_invalid_all(void);
void dcache_clean_all(void);

u32 get_cycles(void);
u32 get_time_us(void);
void udelay(u32 us);

void exit(int i);

#endif /* __CPU_H__ */
