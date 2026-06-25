/*
 * MFD core driver for the X-Powers' Power Management ICs
 *
 * sunxi_pmc typically comprises an adaptive USB-Compatible PWM charger, BUCK DC-DC
 * converters, LDOs, multiple 12-bit ADCs of voltage, current and temperature
 * as well as configurable GPIOs.
 *
 * This file contains the interface independent core functions.
 *
 * Copyright (C) 2014 Carlo Caione
 *
 * Author: Carlo Caione <carlo@caione.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/sunxi_pmc.h>
#include <linux/mfd/core.h>
#include <linux/of_device.h>
#include <linux/acpi.h>

static const char *const pmc_model_names[] = {
	"PMC",
};

static const struct regmap_config pmc_regmap_config = {
	.reg_bits	= 32,
	.val_bits	= 32,
	.reg_stride	= 4,
	.max_register	= PMC_ADDR_EXT,
	.use_single_rw = true,
	.cache_type	= REGCACHE_NONE,
};

/*------------------*/
#define INIT_REGMAP_IRQ(_variant, _irq, _off, _mask)			\
	[_variant##_IRQ_##_irq] = { .reg_offset = (_off), .mask = BIT(_mask) }

static const struct regmap_irq pmc_regmap_irqs[] = {
	INIT_REGMAP_IRQ(PMC, PWRON_INT_NEG,	  0, PMC_IRQ_PWRON_INT_NEG),
	INIT_REGMAP_IRQ(PMC, PWRON_INT_POS,	  0, PMC_IRQ_PWRON_INT_POS),
	INIT_REGMAP_IRQ(PMC, PWRON_INT_SLVL,	  0, PMC_IRQ_PWRON_INT_SLVL),
	INIT_REGMAP_IRQ(PMC, PWRON_INT_LLVL,	  0, PMC_IRQ_PWRON_INT_LLVL),
	INIT_REGMAP_IRQ(PMC, PWRON_INT_SUPER_KEY, 0, PMC_IRQ_PWRON_INT_SUPER_KEY),
	INIT_REGMAP_IRQ(PMC, IRQ_INT_POS,	  0, PMC_IRQ_IRQ_INT_POS),
	INIT_REGMAP_IRQ(PMC, IRQ_INT_NEG,	  0, PMC_IRQ_IRQ_INT_NEG),
	INIT_REGMAP_IRQ(PMC, PWR_VBUS_IN_INT,	  0, PMC_IRQ_PWR_VBUS_IN_INT),
	INIT_REGMAP_IRQ(PMC, PWR_VBUS_OUT_INT,	  0, PMC_IRQ_PWR_VBUS_OUT_INT),
};

static const struct regmap_irq_chip pmc_regmap_irq_chip = {
	.name			= "pmc_irq_chip",
	.status_base		= PWRON_INT_EN_REG,
	.ack_base		= PWRON_INT_EN_REG,
	.mask_base		= PWRON_INT_EN_REG,
	.mask_invert		= true,
	.ack_invert		= true,
	.init_ack_masked	= true,
	.irqs			= pmc_regmap_irqs,
	.num_irqs		= ARRAY_SIZE(pmc_regmap_irqs),
	.num_regs		= 1,
};
/*--------------------*/
static struct resource pmc_pek_resources[] = {
	DEFINE_RES_IRQ_NAMED(PMC_IRQ_PWRON_INT_NEG, "PEK_DBF"),
	DEFINE_RES_IRQ_NAMED(PMC_IRQ_PWRON_INT_POS, "PEK_DBR"),
};

static struct resource pmc_power_supply_resources[] = {
	DEFINE_RES_IRQ_NAMED(PMC_IRQ_PWR_VBUS_IN_INT, "usb in"),
	DEFINE_RES_IRQ_NAMED(PMC_IRQ_PWR_VBUS_OUT_INT, "usb out"),
};

#define PMC_DCDC1	"dcdc1"
#define PMC_DCDC2	"dcdc2"
#define PMC_DCDC3	"dcdc3"
#define PMC_LDOA	"ldoa"
#define PMC_ALDO	"aldo"
#define PMC_RTCLDO	"rtcldo"
#define PMC_LDO33	"ldo33"
#define PMC_LDO09	"ldo09"

static struct mfd_cell pmc_cells[] = {
	{
		/* match drivers/regulator/sunxi_pmc-regulator.c */
		.name = "pmc-regulator",
	},

	{
		.name = "pmc-pek",
		.of_compatible = "x-powers,pmc-pek",
		.resources = pmc_pek_resources,
		.num_resources = ARRAY_SIZE(pmc_pek_resources),

	},
	{
		.name = "pmc-usb-power-supply",
		.of_compatible = "x-powers,pmc-usb-power-supply",
		.resources = pmc_power_supply_resources,
		.num_resources = ARRAY_SIZE(pmc_power_supply_resources),
	},
	{
		.of_compatible = "xpower-vregulator,dcdc1",
		.name = "reg-virt-consumer",
		.id = 1,
		.platform_data = PMC_DCDC1,
		.pdata_size = sizeof(PMC_DCDC1),

	},
	{
		.of_compatible = "xpower-vregulator,dcdc2",
		.name = "reg-virt-consumer",
		.id = 2,
		.platform_data = PMC_DCDC2,
		.pdata_size = sizeof(PMC_DCDC2),
	},
	{
		.of_compatible = "xpower-vregulator,dcdc3",
		.name = "reg-virt-consumer",
		.id = 3,
		.platform_data = PMC_DCDC3,
		.pdata_size = sizeof(PMC_DCDC3),
	},
	{
		.of_compatible = "xpower-vregulator,ldoa",
		.name = "reg-virt-consumer",
		.id = 4,
		.platform_data = PMC_LDOA,
		.pdata_size = sizeof(PMC_LDOA),
	},
	{
		.of_compatible = "xpower-vregulator,aldo",
		.name = "reg-virt-consumer",
		.id = 5,
		.platform_data = PMC_ALDO,
		.pdata_size = sizeof(PMC_ALDO),
	},
	{
		.of_compatible = "xpower-vregulator,rtcldo",
		.name = "reg-virt-consumer",
		.id = 6,
		.platform_data = PMC_RTCLDO,
		.pdata_size = sizeof(PMC_RTCLDO),
	},
	{
		.of_compatible = "xpower-vregulator,ldo33",
		.name = "reg-virt-consumer",
		.id = 7,
		.platform_data = PMC_LDO33,
		.pdata_size = sizeof(PMC_LDO33),
	},
	{
		.of_compatible = "xpower-vregulator,ldo09",
		.name = "reg-virt-consumer",
		.id = 8,
		.platform_data = PMC_LDO09,
		.pdata_size = sizeof(PMC_LDO09),
	},

};

/*---------- PMC Config ------------*/
struct sunxi_pmc_config {
	uint32_t pmc_irq_boot_en;
	uint32_t pmc_irq_en;
	uint32_t pmc_alarm_waleup_en;
	uint32_t pmc_on2off_extra_en;
	uint32_t pmc_vbus_wakeup_en;
	uint32_t pmc_vbus_control_en;
	uint32_t pmc_boot_seq;
	uint32_t pmc_hw_clk_gating;
};

#define PMC_OF_PROP_READ(name, def_value)                              \
	do {                                                               \
		if (of_property_read_u32(node, #name, &config->name)) {        \
			config->name = def_value;                                  \
			pr_err("PMC: config %s fail, set val = %d\n", #name, config->name); \
		} else {                                                       \
			pr_err("PMC: config %s val = %d\n", #name, config->name);  \
		}                                                              \
	} while (0)

static int sunxi_pmc_config_dt_parse(struct device_node *node, struct sunxi_pmc_config *config)
{
	if (!of_device_is_available(node)) {
		pr_err("%s: failed\n", __func__);
		return -1;
	}

	PMC_OF_PROP_READ(pmc_irq_boot_en, 0);
	PMC_OF_PROP_READ(pmc_irq_en, 0);
	PMC_OF_PROP_READ(pmc_alarm_waleup_en, 0);
	PMC_OF_PROP_READ(pmc_on2off_extra_en, 0);
	PMC_OF_PROP_READ(pmc_vbus_control_en, 1);
	PMC_OF_PROP_READ(pmc_vbus_wakeup_en, 1);
	PMC_OF_PROP_READ(pmc_boot_seq, 0);
	PMC_OF_PROP_READ(pmc_hw_clk_gating, 0);

	return 0;
}

static void sunxi_pmc_ctrl_reg_set(struct sunxi_pmc_dev *sunxi_pmc,
				u32 bit, u32 data)
{
	struct regmap *regmap = sunxi_pmc->regmap;
	u32 val;

	if (data) {
		regmap_read(regmap, PMC_CTRL_EN_REG, &val);
		val |= (0x16AA << 16);
		val |= (1 << bit);
		regmap_write(regmap, PMC_CTRL_EN_REG, val);
	} else {
		regmap_read(regmap, PMC_CTRL_EN_REG, &val);
		val |= (0x16AA << 16);
		val &= ~(1 << bit);
		regmap_write(regmap, PMC_CTRL_EN_REG, val);
	}
}

static void sunxi_pmc_cw_cfg_reg_set(struct sunxi_pmc_dev *sunxi_pmc,
				struct sunxi_pmc_config *config)
{
	struct regmap *regmap = sunxi_pmc->regmap;
	u32 val;

	if (config->pmc_boot_seq < 6) {
		/* set boot seq */
		regmap_read(regmap, SW_CFG_REG, &val);
		val &= ~((0x7 << 9) | (0xffff << 16));
		val |= ((config->pmc_boot_seq) << 9);
		regmap_write(regmap, SW_CFG_REG, val | 0x16AA << 16);
		val &= ~(0xffff << 16);
		val |= (0xAA16 << 16);
		regmap_write(regmap, SW_CFG_REG, val);
	}

	if (config->pmc_hw_clk_gating) {
		/* set boot seq */
		regmap_read(regmap, SW_CFG_REG, &val);
		val &= ~((0x1 << 15) | (0xffff << 16));
		regmap_write(regmap, SW_CFG_REG, val | 0x16AA << 16);
		val &= ~(0xffff << 16);
		val |= (0xAA16 << 16);
		regmap_write(regmap, SW_CFG_REG, val);
	}
}

static void sunxi_pmc_config_set(struct sunxi_pmc_dev *sunxi_pmc,
				struct sunxi_pmc_config *config)
{
	/* Set IRQ en */
	sunxi_pmc_ctrl_reg_set(sunxi_pmc, 0, config->pmc_irq_en);

	/* Set IRQ boot en */
	sunxi_pmc_ctrl_reg_set(sunxi_pmc, 1, config->pmc_irq_boot_en);

	/* Set alarm wakeup en */
	sunxi_pmc_ctrl_reg_set(sunxi_pmc, 2, config->pmc_alarm_waleup_en);

	/* Set on off extra en */
	sunxi_pmc_ctrl_reg_set(sunxi_pmc, 5, config->pmc_on2off_extra_en);

	/* Set vbus ctrol en */
	sunxi_pmc_ctrl_reg_set(sunxi_pmc, 11, config->pmc_vbus_control_en);

	/* Set on pmc_vbus_wakeup_en */
	sunxi_pmc_ctrl_reg_set(sunxi_pmc, 15, config->pmc_vbus_wakeup_en);
}

static void sunxi_pmc_dts_config_set(struct sunxi_pmc_dev *sunxi_pmc)
{
	struct device *dev = sunxi_pmc->dev;
	struct sunxi_pmc_config config = {0};

	sunxi_pmc_config_dt_parse(dev->of_node, &config);
	sunxi_pmc_config_set(sunxi_pmc, &config);
	sunxi_pmc_cw_cfg_reg_set(sunxi_pmc, &config);
}

static struct sunxi_pmc_dev *sunxi_pmc_pm_power_off;
static void sunxi_pmc_power_off(void)
{
	/* Give capacitors etc. time to drain to avoid kernel panic msg. */
	msleep(500);
}

int sunxi_pmc_match_device(struct sunxi_pmc_dev *sunxi_pmc)
{
	struct device *dev = sunxi_pmc->dev;
	const struct acpi_device_id *acpi_id;
	const struct of_device_id *of_id;

	if (dev->of_node) {
		of_id = of_match_device(dev->driver->of_match_table, dev);
		if (!of_id) {
			dev_err(dev, "Unable to match OF ID\n");
			return -ENODEV;
		}
		sunxi_pmc->variant = (long)of_id->data;
	} else {
		acpi_id = acpi_match_device(dev->driver->acpi_match_table, dev);
		if (!acpi_id || !acpi_id->driver_data) {
			dev_err(dev, "Unable to match ACPI ID and data\n");
			return -ENODEV;
		}
		sunxi_pmc->variant = (long)acpi_id->driver_data;
	}

	switch (sunxi_pmc->variant) {
	case PMC_V1_ID:
		sunxi_pmc->nr_cells = ARRAY_SIZE(pmc_cells);
		sunxi_pmc->cells = pmc_cells;
		sunxi_pmc->regmap_cfg = &pmc_regmap_config;
		sunxi_pmc->regmap_irq_chip = &pmc_regmap_irq_chip;
		break;
/*-------------------*/
	default:
		dev_err(dev, "unsupported sunxi_pmc ID %lu\n", sunxi_pmc->variant);
		return -EINVAL;
	}
	dev_info(dev, "sunxi_pmc variant %s found\n",
		 pmc_model_names[sunxi_pmc->variant]);

	return 0;
}
EXPORT_SYMBOL(sunxi_pmc_match_device);


int pmc_debug_mask;

static ssize_t debugmask_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	int val, err;

	err = kstrtoint(buf, 16, &val);
	if (err)
		return err;

	pmc_debug_mask = val;

	return count;
}

static ssize_t debugmask_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	char *s = buf;
	char *end = (char *)((ptrdiff_t)buf + (ptrdiff_t)PAGE_SIZE);

	s += scnprintf(s, end - s, "%s\n", "1: SPLY 2: REGU 4: INT 8: CHG");
	s += scnprintf(s, end - s, "debug_mask=%d\n", pmc_debug_mask);

	return s - buf;
}

static u32 pmc_reg_addr;

static ssize_t pmc_reg_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	u32 val;

	regmap_read(sunxi_pmc_pm_power_off->regmap, pmc_reg_addr, &val);
	return sprintf(buf, "REG[0x%x]=0x%x\n",
				pmc_reg_addr, val);
}

static ssize_t pmc_reg_store(struct class *class,
				struct class_attribute *attr,
				const char *buf, size_t count)
{
	s32 tmp;
	u32 val;
	int err;

	err = kstrtoint(buf, 16, &tmp);
	if (err)
		return err;

	if (tmp < 256) {
		pmc_reg_addr = tmp;
	} else {
		val = tmp & 0x00FF;
		pmc_reg_addr = (tmp >> 8) & 0x00FF;
		regmap_write(sunxi_pmc_pm_power_off->regmap, pmc_reg_addr, val);
	}

	return count;
}

static ssize_t pmc_powon_status_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	u32 val;
	char *reason;

	regmap_read(sunxi_pmc_pm_power_off->regmap, PMC_STATUS_REG, &val);
	val &= 0xF;

	switch (val) {
	case 0://RESET_STATUS
		reason = "RESET_STATUS";
		break;
	case 1://POWERON_BOOT
		reason = "POWERON_BOOT";
		break;
	case 2://VBUS_BOOT
		reason = "VBUS_BOOT";
		break;
	case 3://IRQ_BOOT
		reason = "IRQ_BOOT";
		break;
	case 4://ALARM_BOOT
		reason = "ALARM_BOOT";
		break;
	case 5://REBOOT
		reason = "REBOOT";
		break;
	case 6://POWERON_WAKE
		reason = "POWERON_WAKE";
		break;
	case 7://VBUS_WAKE
		reason = "VBUS_WAKE";
		break;
	case 8://IRQ_WAKE
		reason = "IRQ_WAKE";
		break;
	case 9://ALARM_WAKE
		reason = "ALARM_WAKE";
		break;
	case 10://USB_WAKE
		reason = "USB_WAKE";
		break;
	}

	return sprintf(buf, "%s\n", reason);
}

static ssize_t pmc_powoff_status_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	u32 val;
	char *reason;

	regmap_read(sunxi_pmc_pm_power_off->regmap, PMC_STATUS_REG, &val);
	val = (val >> 4) & 0x7;

	switch (val) {
	case 0://FIRST_POWEROFF
		reason = "FIRST_POWEROFF";
		break;
	case 1://LONGPRESS POWEROFF
		reason = "LONGPRESS_POWEROFF";
		break;
	case 2://LONGPRESS_REBOOT
		reason = "LONGPRESS_REBOOT";
		break;
	case 3://WDT_REBOOT
		reason = "WDT_REBOOT";
		break;
	case 4://POWER_DROP
		reason = "ABNORMAL_POWER_LOSS";
		break;
	case 5://SOFT_POWEROFF
		reason = "SOFT_POWEROFF";
		break;
	case 6://SOFT_REBOOT
		reason = "SOFT_REBOOT";
		break;
	}

	return sprintf(buf, "%s\n", reason);
}

static ssize_t pmc_status_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	u32 val;
	regmap_read(sunxi_pmc_pm_power_off->regmap, PMC_BYP_ST_REG, &val);
	val &= 0x1;

	if (val) {
		return sprintf(buf, "PMC Enable\n");
	} else {
		return sprintf(buf, "PMC Bypass\n");
	}
}

static ssize_t pmc_vbus_status_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	u32 val;
	regmap_read(sunxi_pmc_pm_power_off->regmap, PMC_STATUS_REG, &val);
	if (val & (1 << 8)) {
		return sprintf(buf, "VBUS IN\n");
	} else {
		return sprintf(buf, "NO VBUS\n");
	}
}

static struct class_attribute pmc_class_attrs[] = {
	__ATTR(powon_status,  S_IRUGO, pmc_powon_status_show, NULL),
	__ATTR(powoff_status, S_IRUGO, pmc_powoff_status_show, NULL),
	__ATTR(pmc_status,    S_IRUGO, pmc_status_show, NULL),
	__ATTR(vbus_status,   S_IRUGO, pmc_vbus_status_show, NULL),
	__ATTR(pmc_reg,       S_IRUGO|S_IWUSR, pmc_reg_show,   pmc_reg_store),
	__ATTR(debug_mask,    S_IRUGO|S_IWUSR, debugmask_show, debugmask_store),
	__ATTR_NULL
};

struct class pmc_class = {
	.name = "pmc",
	.class_attrs = pmc_class_attrs,
};

static int pmc_sysfs_init(void)
{
	int status;

	status = class_register(&pmc_class);
	if (status < 0)
		pr_err("%s,%d err, status:%d\n", __func__, __LINE__, status);

	return status;
}

int sunxi_pmc_device_probe(struct sunxi_pmc_dev *sunxi_pmc)
{
	int ret;

	/*
	 * on some board ex. qaqc test board, there's no interrupt for sunxi_pmc
	 */
	if (sunxi_pmc->irq) {
		ret = regmap_add_irq_chip(sunxi_pmc->regmap, sunxi_pmc->irq,
					  IRQF_ONESHOT | IRQF_SHARED, -1,
					  sunxi_pmc->regmap_irq_chip,
					  &sunxi_pmc->regmap_irqc);
		if (ret) {
			dev_err(sunxi_pmc->dev, "failed to add irq chip: %d\n", ret);
			return ret;
		}
	}

	if (sunxi_pmc->dts_parse)
		sunxi_pmc->dts_parse(sunxi_pmc);

	ret = mfd_add_devices(sunxi_pmc->dev, 0, sunxi_pmc->cells,
			      sunxi_pmc->nr_cells, NULL, 0, NULL);

	if (ret) {
		dev_err(sunxi_pmc->dev, "failed to add MFD devices: %d\n", ret);

		if (sunxi_pmc->irq)
			regmap_del_irq_chip(sunxi_pmc->irq, sunxi_pmc->regmap_irqc);

		return ret;
	}

	sunxi_pmc_pm_power_off = sunxi_pmc;
	pmc_sysfs_init();
	if (!pm_power_off) {
		pm_power_off = sunxi_pmc_power_off;
	}

	sunxi_pmc_dts_config_set(sunxi_pmc);

	dev_info(sunxi_pmc->dev, "sunxi_pmc driver loaded\n");

	return 0;
}
EXPORT_SYMBOL(sunxi_pmc_device_probe);

int sunxi_pmc_device_remove(struct sunxi_pmc_dev *sunxi_pmc)
{
	if (sunxi_pmc == sunxi_pmc_pm_power_off) {
		sunxi_pmc_pm_power_off = NULL;
		pm_power_off = NULL;
	}

	mfd_remove_devices(sunxi_pmc->dev);

	if (sunxi_pmc->irq)
		regmap_del_irq_chip(sunxi_pmc->irq, sunxi_pmc->regmap_irqc);

	return 0;
}
EXPORT_SYMBOL(sunxi_pmc_device_remove);

MODULE_DESCRIPTION("PMIC MFD core driver for sunxi_pmc");
MODULE_AUTHOR("Carlo Caione <carlo@caione.org>");
MODULE_LICENSE("GPL");
