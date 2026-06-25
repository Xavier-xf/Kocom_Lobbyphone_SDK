include ../../common.mk

TARGET := libg2d_helper.a

SRCS := \
	g2d_helper.c

OBJS := $(SRCS:%.c=%.o)

INCLUDE_DIR := \
	-Iinclude \
	-I../sys/include \
	-I../debug/include

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
