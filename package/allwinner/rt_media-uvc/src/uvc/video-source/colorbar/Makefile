include ../../../common.mk

TARGET := libvideo_source_colorbar.a

SRCS := \
	colorbar.c

OBJS := $(SRCS:%.c=%.o)

INCLUDE_DIR := \
	-Iinclude \
	-I../../../utils/debug/include \
	-I../base/include

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
