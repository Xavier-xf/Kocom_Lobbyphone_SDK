TARGET = aactd

SRCS = $(wildcard *.c)

TARGET_RENAME := $(subst -,_,$(TARGET_PLATFORM))

override CFLAGS += -DTARGET_PLATFORM_$(TARGET_RENAME)
override INCLUDES +=
override LDFLAGS += -laactd -lpthread

all: $(TARGET) install

$(TARGET): $(SRCS)
	$(CC) $^ $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@

install:
	mkdir -p $(INSTALL_PREFIX)/usr/bin
	cp $(TARGET) $(INSTALL_PREFIX)/usr/bin

clean:
	rm -rf $(TARGET)

.PHONY: all clean install
