// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2023-2026
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/gpio.h>

int get_group_bit_offset(enum pin_e port_group)
{
	switch (port_group) {
	case GPIO_GROUP_A:
	case GPIO_GROUP_C:
	case GPIO_GROUP_D:
	case GPIO_GROUP_E:
	case GPIO_GROUP_F:
	case GPIO_GROUP_G:
	case GPIO_GROUP_I:
		return port_group;
		break;
	default:
		return -1;
	}
    return -1;
}

#ifdef CONFIG_SUNXI_UBOOT_PMC_POWER_OFF
#define RTC_PMC_CTRL_EN_OFFSET    (0x210)
#define RTC_SW_CFG_OFFSET         (0x218)

int sunxi_platform_power_off(int status)
{
	uint val = readl(SUNXI_RTC_BASE + RTC_PMC_CTRL_EN_OFFSET);
	val &= ~(0x1 << 11);
	writel(val | (0x16aa << 16), SUNXI_RTC_BASE + RTC_PMC_CTRL_EN_OFFSET);

	val = readl(SUNXI_RTC_BASE + RTC_SW_CFG_OFFSET);
	val &= ~(0xffff << 16);
	writel(val | (0x16aa << 16), SUNXI_RTC_BASE + RTC_SW_CFG_OFFSET);
	val &= ~(0x3 << 1);
	val |= (0x1 << 0);
	writel(val | (0xaa16 << 16), SUNXI_RTC_BASE + RTC_SW_CFG_OFFSET);

	return 0;
}
#endif
