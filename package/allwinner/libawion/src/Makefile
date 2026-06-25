TARGET := libawion.so
STATIC_TARGET := libawion.a
LIBS :=

commonSources:=

ifneq ($(filter $(KERNEL_VERSION), CONF_KERNEL_VERSION_3_4 CONF_KERNEL_VERSION_3_10 CONF_KERNEL_VERSION_4_4 CONF_KERNEL_VERSION_4_9),)
commonSources += ion_alloc.c
else ifneq ($(filter $(KERNEL_VERSION), CONF_KERNEL_VERSION_5_4 CONF_KERNEL_VERSION_5_4_ANSC),)
commonSources += ion_alloc_5_4.c
else
commonSources += ion_alloc_5_15.c
endif

local_CFLAGS := -I./include -I$(LINUX_USER_HEADERS)/include

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

install: $(TARGET) $(STATIC_TARGET)
	mkdir -p $(INSTALL_PREFIX)/usr/lib
	cp $(TARGET) $(INSTALL_PREFIX)/usr/lib
	cp $(STATIC_TARGET) $(INSTALL_PREFIX)/usr/lib
	mkdir -p $(INSTALL_PREFIX)/usr/include
	cp -r include/*  $(INSTALL_PREFIX)/usr/include
