TARGET = rpbuf_test

SRCS = $(wildcard *.c)

override CFLAGS += -I$(INSTALL_PREFIX)/usr/include/ -D_GNU_SOURCE
override INCLUDES +=
override LDFLAGS += -L$(INSTALL_PREFIX)/usr/lib -lrpbuf

all: $(TARGET) install

$(TARGET): $(SRCS)
	$(CC) $^ $(CFLAGS) $(INCLUDES) $(LDFLAGS) -o $@

install:
	mkdir -p $(INSTALL_PREFIX)/usr/bin
	cp $(TARGET) $(INSTALL_PREFIX)/usr/bin

clean:
	rm -rf $(TARGET)

.PHONY: all clean install
