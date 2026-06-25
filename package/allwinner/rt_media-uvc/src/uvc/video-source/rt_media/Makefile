include ../../../common.mk

TARGET := libvideo_source_rt_media.a

SRCS := \
	rt_awaiisp_common.c \
	video_source_rt_media.c

OBJS := $(SRCS:%.c=%.o)

INCLUDE_DIR := \
	-Iinclude \
	-I../base/include \
	-I../../../utils/g2d/include \
	-I../../../utils/debug/include \
	-I../../../utils/list/include \
	-I$(KERNEL_DIR)/user_headers/include \
	-I$(STAGING_DIR)/usr/include/rt_media \
	-I$(STAGING_DIR)/usr/include/libawaiisp

LOCAL_CFLAGS := \
	$(CFLAGS)

.PHONY:all clean

all:$(TARGET)

$(TARGET):$(OBJS)
	$(AR) -rcs -o $@ $+

%.o:%.c
	$(CC) $(LOCAL_CFLAGS) $(INCLUDE_DIR) -c -o $@ $<

clean:
	-rm $(OBJS) $(TARGET)
