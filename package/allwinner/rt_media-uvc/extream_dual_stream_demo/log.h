#ifndef __LOG_H__
#define __LOG_H__

#include <stdio.h>

#define logv(x, arg...)// printf("[VER]%s:%d<%s>: " x "\n", __FILE__, __LINE__, __FUNCTION__, ##arg)
#define logd(x, arg...) printf("[DEG]%s:%d<%s>: " x "\n", __FILE__, __LINE__, __FUNCTION__, ##arg)
#define logw(x, arg...) printf("[WAR]%s:%d<%s>: " x "\n", __FILE__, __LINE__, __FUNCTION__, ##arg)
#define loge(x, arg...) printf("[ERR]%s:%d<%s>: " x "\n", __FILE__, __LINE__, __FUNCTION__, ##arg)

#endif
