include ../../common.mk

TARGET := libuvc.a

SRCS := \
	stream.c \
	uvc.c \
	v4l2.c \
	video_buffers.c

OBJS := $(SRCS:%.c=%.o)

INCLUDE_DIR := \
	-Iinclude \
	-I../../utils/debug/include \
	-I../../utils/libevent/include \
	-I../../utils/configfs_parser/include \
	-I../../utils/list/include \
	-I../../utils/sys/include \
	-I../video-source/base/include \
	-I../../utils/configfs_parser/include \
	-I$(KERNEL_DIR)/user_headers/include

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
