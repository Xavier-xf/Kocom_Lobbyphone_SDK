/*
 * Allwinner SoCs hdmi2.0 driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include "irq.h"

typedef struct irq_vector {
	irq_sources_t source;
	unsigned int stat_reg;
	unsigned int mute_reg;
} irq_vector_t;

static irq_vector_t irq_vec[] = {
	{AUDIO_PACKETS,	IH_FC_STAT0, IH_MUTE_FC_STAT0},
	{OTHER_PACKETS,	IH_FC_STAT1, IH_MUTE_FC_STAT1},
	{PACKETS_OVERFLOW, IH_FC_STAT2,	IH_MUTE_FC_STAT2},
	{AUDIO_SAMPLER,	IH_AS_STAT0, IH_MUTE_AS_STAT0},
	{PHY, IH_PHY_STAT0, IH_MUTE_PHY_STAT0},
	{I2C_DDC, IH_I2CM_STAT0, IH_MUTE_I2CM_STAT0},
	{CEC, IH_CEC_STAT0, IH_MUTE_CEC_STAT0},
	{VIDEO_PACKETIZER, IH_VP_STAT0,	IH_MUTE_VP_STAT0},
	{I2C_PHY, IH_I2CMPHY_STAT0, IH_MUTE_I2CMPHY_STAT0},
	{AUDIO_DMA, IH_AHBDMAAUD_STAT0, IH_MUTE_AHBDMAAUD_STAT0},
	{0, 0, 0},
};

/*******************************************************************
 * Mute IRQ miscellaneous
 */
int irq_mute_source(hdmi_tx_dev_t *dev, irq_sources_t irq_source)
{
	int i = 0;

	for (i = 0; irq_vec[i].source != 0; i++) {
		if (irq_vec[i].source == irq_source) {
			dev_write(dev, irq_vec[i].mute_reg,  0xff);
			return TRUE;
		}
	}
	hdmi_err("Error:IRQ source [%d] is not supported\n", irq_source);
	return FALSE;
}

/*******************************************************************
 * Unmute IRQ miscellaneous
 */
int irq_unmute_source(hdmi_tx_dev_t *dev, irq_sources_t irq_source)
{
	int i = 0;

	for (i = 0; irq_vec[i].source != 0; i++) {
		if (irq_vec[i].source == irq_source) {
			VIDEO_INF("IRQ write unmute: irq[%d] mask[%d]\n",
							irq_source, 0x0);
			dev_write(dev, irq_vec[i].mute_reg,  0x00);
			return TRUE;
		}
	}
	hdmi_err("Error:IRQ source [%d] is supported\n", irq_source);
	return FALSE;
}

void irq_mute(hdmi_tx_dev_t *dev)
{
	LOG_TRACE();
	dev_write(dev, IH_MUTE,  0x3);
}

void irq_unmute(hdmi_tx_dev_t *dev)
{
	LOG_TRACE();
	dev_write(dev, IH_MUTE,  0x0);
}

void irq_mask_all(hdmi_tx_dev_t *dev)
{
	LOG_TRACE();
	irq_mute(dev);
	irq_mute_source(dev, AUDIO_PACKETS);
	irq_mute_source(dev, OTHER_PACKETS);
	irq_mute_source(dev, PACKETS_OVERFLOW);
	irq_mute_source(dev, AUDIO_SAMPLER);
	irq_mute_source(dev, PHY);
	irq_mute_source(dev, I2C_DDC);
	irq_mute_source(dev, CEC);
	irq_mute_source(dev, VIDEO_PACKETIZER);
	irq_mute_source(dev, I2C_PHY);
	irq_mute_source(dev, AUDIO_DMA);
}

void irq_hpd_sense_enable(hdmi_tx_dev_t *dev, u8 enable)
{
	LOG_TRACE();

	if (enable) {
		i2cddc_fast_mode(dev, 0);

		/* Enable HDMI TX PHY HPD Detector */
		phy_enable_hpd_sense(dev);
	} else {
		phy_disable_hpd_sense(dev);
	}
}

