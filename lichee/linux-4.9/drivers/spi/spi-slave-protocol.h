/*
 * drivers/spi/spi-slave-protocol.h
 *
 * Copyright (C) 2019 - Reuuimlla Limited
 * Yihuasheng <yihuasheng@allwinnertech.com>
 *
 * SUNXI SPI Slave Protocol Definition
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 */

#define HEAD_LEN 5

#define OP_MASK		0
#define ADDR_MASK_0	1
#define ADDR_MASK_1	2
#define ADDR_MASK_2	3
#define LENGTH_MASK	4

#define SUNXI_OP_WRITE	0x01
#define SUNXI_OP_READ	0x03

struct sunxi_spi_slave_head {
	int op_code;
	int addr;
	int len;
};

struct sunxi_slave {
	struct sunxi_spi_slave_head *head;
	struct spi_transfer *t;
};




