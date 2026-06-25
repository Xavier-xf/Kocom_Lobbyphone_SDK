#ifndef __DE_HW_SCALE_H__
#define __DE_HW_SCALE_H__

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//#include "aw_util.h"

#include <linux/fb.h>
#include <linux/kernel.h>

typedef signed char s8;
typedef unsigned char u8;

typedef signed short s16;
typedef unsigned short u16;

typedef signed int s32;
typedef unsigned int u32;

typedef signed long long s64;
typedef unsigned long long u64;

#include <video/sunxi_display2.h>

typedef struct DEWBInfo
{
    int mIonFd;
    int mWidth;
    int mHeight;

    enum disp_csc_type ePixFmtTpye;
    enum disp_pixel_format ePixFmt;

    unsigned int nPhyAddr;
    void* pVirtAddr;
} DEWBInfo;

typedef struct SampleDewbInfo
{
    int mDeId;
    int mDeFd;
    DEWBInfo mInputInfo;
    DEWBInfo mOutputInfo;
} SampleDewbInfo;

int De_Init(SampleDewbInfo *pDewbInfo);
int De_Wb_Scale(SampleDewbInfo *pDewbInfo);
int De_reset(SampleDewbInfo *pDewbInfo);
int De_Deinit(SampleDewbInfo *pDewbInfo);

#endif // __DE_HW_SCALE_H__
