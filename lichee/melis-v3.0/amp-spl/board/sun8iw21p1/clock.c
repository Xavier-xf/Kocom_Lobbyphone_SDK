/*
 * (C) Copyright 2018-2020
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * wangwei <wangwei@allwinnertech.com>
 *
 */

#include <include.h>

#define CCMU_UART_BGR_REG                  (SUNXI_CCM_BASE + 0x90C)
#define CCM_UART_RST_OFFSET                (16)
#define CCM_UART_GATING_OFFSET             (0)

void sunxi_clock_init_uart(int port)
{
	u32 i, reg;

	/* reset */
	reg = readl(CCMU_UART_BGR_REG);
	reg &= ~(1<<(CCM_UART_RST_OFFSET + port));
	writel(reg, CCMU_UART_BGR_REG);
	for (i = 0; i < 100; i++)
		;
	reg |= (1 << (CCM_UART_RST_OFFSET + port));
	writel(reg, CCMU_UART_BGR_REG);
	/* gate */
	reg = readl(CCMU_UART_BGR_REG);
	reg &= ~(1<<(CCM_UART_GATING_OFFSET + port));
	writel(reg, CCMU_UART_BGR_REG);
	for (i = 0; i < 100; i++)
		;
	reg |= (1 << (CCM_UART_GATING_OFFSET + port));
	writel(reg, CCMU_UART_BGR_REG);
}
