/*
 *    Filename: cedarv_ve.h
 *     Version: 0.01alpha
 * Description: Video engine driver API, Don't modify it in user space.
 *     License: GPLv2
 *
 *		Author  : xyliu <xyliu@allwinnertech.com>
 *		Date    : 2016/04/13
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
/* Notice: It's video engine driver API, Don't modify it in user space. */
#ifndef _CEDAR_VE_H_
#define _CEDAR_VE_H_

#include <linux/cedar_ve_uapi.h>

/* the struct must be same with user-header-file: libcedarc/veAw.h*/
typedef struct ve_channel_proc_info {
	unsigned char *base_info_data;
	unsigned int   base_info_size;
	unsigned char *advance_info_data;
	unsigned int   advance_info_size;
	unsigned int   channel_id;
} ve_channel_proc_info;

enum VE_INTERRUPT_RESULT_TYPE {
	VE_INT_RESULT_TYPE_TIMEOUT   = 0,
	VE_INT_RESULT_TYPE_NORMAL    = 1,
	VE_INT_RESULT_TYPE_CSI_RESET = 2,
};

typedef struct CsiOnlineRelatedInfo {
	unsigned int csi_frame_start_cnt;
	unsigned int csi_frame_done_cnt;
	unsigned int csi_cur_frame_addr;
	unsigned int csi_pre_frame_addr;
	unsigned int csi_line_start_cnt;
	unsigned int csi_line_done_cnt;
} CsiOnlineRelatedInfo;

struct page_buf_info {
	/* total_size = header + data + ext */
	unsigned int header_size;
	unsigned int data_size;
	unsigned int ext_size;
	unsigned int phy_addr_0;
	unsigned int phy_addr_1;
	unsigned int buf_id;
	unsigned int vir_addr_0;
	unsigned int vir_addr_1;
};

#endif
