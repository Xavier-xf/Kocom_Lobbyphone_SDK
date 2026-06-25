include ../../common.mk

TARGET := libdemo_event.a

SRCS := \
	demo_events.c

OBJS := $(SRCS:%.c=%.o)

INCLUDE_DIR := \
	-Iinclude \
	-I../debug/include \
	-I$(STAGING_DIR)/usr/include

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
