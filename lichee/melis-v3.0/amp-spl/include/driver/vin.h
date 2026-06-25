#ifndef __VIN_H__
#define __VIN_H__

#include <include.h>

#define CSI0_MASTER_CLK_REG_OFFSET 0x0C08
#define CSI1_MASTER_CLK_REG_OFFSET 0x0C0C
#define CSI2_MASTER_CLK_REG_OFFSET 0x0C10
#ifdef CONFIG_SENSOR_GC2053_MIPI
#define CSI0_MASTER_CLK_PIN SUNXI_GPE(12)
#elif defined(CONFIG_SENSOR_GC4663_MIPI)
#define CSI0_MASTER_CLK_PIN SUNXI_GPA(10)
#else
#define CSI0_MASTER_CLK_PIN SUNXI_GPA(12)
#endif
#define CSI1_MASTER_CLK_PIN SUNXI_GPA(13)
#define CSI2_MASTER_CLK_PIN SUNXI_GPE(1)
#ifdef CONFIG_SENSOR_GC2053_MIPI
#define CSI0_MASTER_CLK_PIN_FUNC 5
#else
#define CSI0_MASTER_CLK_PIN_FUNC 4
#endif
#define CSI1_MASTER_CLK_PIN_FUNC 4
#define CSI2_MASTER_CLK_PIN_FUNC 2
#define CSI2_MASTER_CLK_PIN_DISABLED 0xf

#define REG_DLY  0xffff
#define CSI_GPIO_HIGH     1
#define CSI_GPIO_LOW     0
#define ARRAY_SIZE(x)       (sizeof(x) / sizeof((x)[0]))

typedef enum
{
    GPIO_PULL_DOWN_DISABLED  = 0,
    GPIO_PULL_UP = 1,
    GPIO_PULL_DOWN = 2,
} gpio_pull_status_t;

typedef enum
{
    GPIO_DIRECTION_INPUT  = 0,
    GPIO_DIRECTION_OUTPUT = 1
} gpio_direction_t;

typedef enum
{
    GPIO_DATA_LOW  = 0,
    GPIO_DATA_HIGH = 1
} gpio_data_t;

enum power_seq_cmd {
	PWR_OFF = 0,
	PWR_ON = 1,
};

struct regval_list {
	u32 addr;
	u8 data;
};

int vin_gpio_set_status(u32 pin, unsigned int status);
int vin_gpio_write(u32 pin, unsigned int out_value);
int sensor_write_array(unsigned long twi_base, u8 chip, int alen, struct regval_list *regs, int array_size);
int vin_set_mclk(int id, unsigned int on_off);
int sensor_init(void);
#endif
