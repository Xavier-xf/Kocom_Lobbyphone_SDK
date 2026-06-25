/*
 * (C) Copyright 2013-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 */
/*
 * COM1 NS16550 support
 * originally from linux source (arch/powerpc/boot/ns16550.c)
 * modified to use CONFIG_SYS_ISA_MEM and new defines
 */

#include <include.h>

int vin_gpio_set_status(u32 pin, unsigned int status)
{

	if (status == 1) {
		sunxi_gpio_set_pull(pin, GPIO_PULL_UP);
		sunxi_gpio_set_cfgpin(pin, GPIO_DIRECTION_OUTPUT);
		sunxi_gpio_set_data(pin, GPIO_DATA_LOW);
	} else if (status == 0) {
		sunxi_gpio_set_pull(pin, GPIO_PULL_DOWN);
		sunxi_gpio_set_cfgpin(pin, GPIO_DIRECTION_INPUT);
		sunxi_gpio_set_data(pin, GPIO_DATA_LOW);
	}

	return 0;
}

int vin_gpio_write(u32 pin, unsigned int out_value)
{
	if (out_value) {
		sunxi_gpio_set_data(pin, GPIO_DATA_HIGH);
	} else {
		sunxi_gpio_set_data(pin, GPIO_DATA_LOW);
	}
	
	return 0;
}

int sensor_write_array(unsigned long twi_base, u8 chip, int alen, struct regval_list *regs, int array_size)
{
	int ret = 0, i = 0;	

	if (regs == NULL)
		return -1;

	printf("twi_base:0x%lx, chip:0x%x, alen = %d, array_size = %d\n", twi_base, chip, alen, array_size);

	while (i < array_size) {
		if (regs->addr == REG_DLY) {
			udelay(regs->data);
		} else {
			ret = twi_write(twi_base, (chip >> 1), regs->addr, alen, &(regs->data), 1);
			if (ret < 0) {
				return -1;
			}
		}
		i++;
		regs++;
	}
	
	return 0;
}

int vin_set_mclk(int id, unsigned int on_off)
{
	u32 tmp, csi_reg, pin, func;

	if(id == 0) {
		csi_reg = SUNXI_CCM_BASE + CSI0_MASTER_CLK_REG_OFFSET;
		pin = CSI0_MASTER_CLK_PIN;
		func = CSI0_MASTER_CLK_PIN_FUNC;
	} else if(id == 1) {
		csi_reg = SUNXI_CCM_BASE + CSI1_MASTER_CLK_REG_OFFSET;
		pin = CSI1_MASTER_CLK_PIN;
		func = CSI1_MASTER_CLK_PIN_FUNC;
	} else if(id == 2) {
		csi_reg = SUNXI_CCM_BASE + CSI2_MASTER_CLK_REG_OFFSET;
		pin = CSI2_MASTER_CLK_PIN;
		func = CSI2_MASTER_CLK_PIN_FUNC;
	} else {
		printf("invalid mclk id!\n");
		return -1;
	}

	if(on_off) {
		sunxi_gpio_set_cfgpin(pin, func);
		tmp = readl(csi_reg);
		if((tmp & (0x1 << 31)) == 0)
			tmp |= (0x1 << 31);
	} else {
		sunxi_gpio_set_cfgpin(pin, CSI2_MASTER_CLK_PIN_DISABLED);
		tmp = readl(csi_reg);
		if(tmp & (0x1 << 31))
			tmp &= ~(0x1 << 31);
	}
	writel(tmp, csi_reg);

	return 0;
}
