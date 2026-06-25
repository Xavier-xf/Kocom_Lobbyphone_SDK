TARGET := libuapi.so
STATIC_TARGET := libuapi.a
LIBS :=

commonSources:= \
	yuv_rotate.c \
	videoOutPort.c \
	gpu_yuv2rgb.c

local_CFLAGS := -I./include

LDFLAGS += -lawion

OBJS = $(patsubst %.c, %.c.o, $(commonSources))

$(TARGET):$(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(LIBS) $(local_CFLAGS) -o $@ $^ -fPIC -shared

$(STATIC_TARGET):$(OBJS)
	$(AR) rcs $@ $^

$(OBJS):%.c.o:%.c
	$(CC) $(CFLAGS) $(LDFLAGS) $(local_CFLAGS) -c $< -o $@ -fPIC -shared

all: $(TARGET) $(STATIC_TARGET) install

clean:
	-rm $(OBJS) $(TARGET) $(STATIC_TARGET)
install:
	mkdir -p $(INSTALL_PREFIX)/usr/lib
	cp $(TARGET) $(INSTALL_PREFIX)/usr/lib
	cp $(STATIC_TARGET) $(INSTALL_PREFIX)/usr/lib
	mkdir -p $(INSTALL_PREFIX)/usr/include
	cp -r include/*  $(INSTALL_PREFIX)/usr/include
