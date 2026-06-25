/*
 * Allwinner SoCs hdmi2.0 driver.
 *
 * Copyright (C) 2016 Allwinner.
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */


#ifndef LOG_H_
#define LOG_H_
#include "../../config.h"

#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/pwm.h>
#include <asm/div64.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_iommu.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/compat.h>

#define TRUE true
#define FALSE false

extern u32 hdmi_printf;

typedef enum {
	SNPS_ERROR = 0,
	SNPS_WARN,
	SNPS_NOTICE,
	SNPS_DEBUG,
	SNPS_TRACE,
	SNPS_ASSERT
} log_t;

#ifdef hdmi_inf
#undef hdmi_inf
#endif
#define hdmi_inf(fmt, args...)                 \
	do {                                       \
		pr_info("hdmi20: "fmt, ##args);        \
	} while (0)

#ifdef hdmi_wrn
#undef hdmi_wrn
#endif
#define hdmi_wrn(fmt, args...)                  \
	do {                                        \
		pr_info("hdmi20: [warn] "fmt, ##args);  \
	} while (0)

#ifdef hdmi_err
#undef hdmi_err
#endif
#define hdmi_err(fmt, args...)                   \
	do {                                         \
		pr_info("hdmi20: [error] "fmt, ##args);  \
	} while (0)

#define VIDEO_INF(fmt, args...)                                            \
	do {                                                                   \
		if ((hdmi_printf == 1) || (hdmi_printf == 4) || (hdmi_printf > 6)) \
			pr_info("hdmi20: [video] "fmt, ##args);                        \
	} while (0)

#define EDID_INF(fmt, args...)                                             \
	do {                                                                   \
		if ((hdmi_printf == 2) || (hdmi_printf == 4) || (hdmi_printf > 6)) \
			pr_info("hdmi20: [edid] "fmt, ##args);                         \
	} while (0)

#define AUDIO_INF(fmt, args...)                                            \
	do {                                                                   \
		if ((hdmi_printf == 3) || (hdmi_printf == 4) || (hdmi_printf > 6)) \
			pr_info("hdmi20: [audio] "fmt, ##args);                        \
	} while (0)

#define CEC_INF(fmt, args...)                        \
	do {                                             \
		if ((hdmi_printf > 6) || (hdmi_printf == 5)) \
			pr_info("hdmi20: [cec] "fmt, ##args);    \
	} while (0)

#define HDCP_INF(fmt, args...)                     \
	do {                                           \
		if (hdmi_printf > 5)                       \
			pr_info("hdmi20: [hdcp] "fmt, ##args); \
	} while (0)

#define LOG_TRACE()                                    \
	do {                                               \
		if (hdmi_printf > 7)                           \
			pr_info("hdmi20: [trace] %s\n", __func__); \
	} while (0)

#define LOG_TRACE1(a)                                         \
	do {                                                      \
		if (hdmi_printf > 7)                                  \
			pr_info("hdmi20: [trace] %s: %d\n", __func__, a); \
	} while (0)

#define LOG_TRACE2(a, b)                                            \
	do {                                                            \
		if (hdmi_printf > 7)                                        \
			pr_info("hdmi20: [trace] %s: %d %d\n", __func__, a, b); \
	} while (0)

#define LOG_TRACE3(a, b, c)                                               \
	do {                                                                  \
		if (hdmi_printf > 7)                                              \
			pr_info("hdmi20: [trace] %s: %d %d %d\n", __func__, a, b, c); \
	} while (0)

#endif	/* LOG_H_ */
