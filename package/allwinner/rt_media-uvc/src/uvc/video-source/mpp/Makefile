include ../../../common.mk

TARGET := libvideo_source_mpp.a

SRCS := \
	venc_common.c \
	video_source_mpp.c

OBJS := $(SRCS:%.c=%.o)

INCLUDE_DIR := \
	-Iinclude \
	-I../base/include \
	-I../../../utils/g2d/include \
	-I../../../utils/debug/include \
	-I../../../utils/list/include \
	-I$(KERNEL_DIR)/user_headers/include \
	-I$(STAGING_DIR)/usr/include/rt_media \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/include \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/include/media \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/include \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/include \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/include/utils \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/include/V4l2Camera \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/include/device \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libcedarc/include \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/include/component \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/isp_tuning

LOCAL_CFLAGS := \
	$(CFLAGS) \
	-DAWCHIP=0x1886

.PHONY:all clean

all:$(TARGET)

$(TARGET):$(OBJS)
	$(AR) -rcs -o $@ $+

%.o:%.c
	$(CC) $(LOCAL_CFLAGS) $(INCLUDE_DIR) -c -o $@ $<

clean:
	-rm $(OBJS) $(TARGET)
