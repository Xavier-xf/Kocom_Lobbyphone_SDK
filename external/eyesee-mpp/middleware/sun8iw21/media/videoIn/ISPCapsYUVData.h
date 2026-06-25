/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef __ISPCAPSYUVDATA_H__
#define __ISPCAPSYUVDATA_H__
//#include <sys/io.h>
//#include <utils/plat_log.h>
//#include <sys/stat.h>

//#include <endian.h>
//#include <errno.h>
//#include <memory.h>
//#include <fcntl.h>
//#include <getopt.h>
//#include <sched.h>
//#include <pthread.h>
//#include <stdbool.h>
//#include <stdio.h>
//#include <stdbool.h>
//#include <stdlib.h>
//#include <string.h>
//#include <sys/prctl.h>
//#include <linux/unistd.h>
//#include <linux/kernel.h>
//#include <linux/types.h>
//#include <sys/syscall.h>

//#include <ion_memmanager.h>
//#include <sys_linux_ioctl.h>
//#include "isp_dev.h"
//#include "isp.h"
//#include "media/mpi_sys.h"
//#include "media/mm_comm_vi.h"
//#include "media/mpi_vi.h"
//#include "media/mpi_isp.h"
//#include "mm_component.h"

//#include <VideoVirViCompPortIndex.h>

#include "videoInputHw.h"
//#include "sunxi_camera_v2.h"
//#include <mpi_videoformat_conversion.h>
//#include <ChannelRegionInfo.h>
//#include <BITMAP_S.h>
//#include "VIPPDrawOSD_V5.h"
//#include "ConfigOption.h"

//#include <media_debug.h>

#include <mm_comm_video.h>

#define MAX_LEN 301

int SendYuvToApp(VIDEO_FRAME_INFO_S *pstFrameInfo, viChnManager *pManager, int vipp_id);

#endif
