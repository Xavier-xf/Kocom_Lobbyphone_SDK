TARGET := facehumandemo
SRCS := facehumandemo.c
OBJS := $(SRCS:%.c=%.o)

INCLUDE_DIR := $(STAGING_DIR)/usr/include

SHARE_LIBS := \
	-lVIPuser \
	-lVIPlite

STATIC_LIBS := \
	-lawnn_det

LIBS_SEARCH_DIR := $(STAGING_DIR)/usr/lib

.PHONY: all clean

all:$(TARGET)

$(TARGET):$(OBJS)
	$(CXX) -o $@ $< -Wl,-Bstatic $(STATIC_LIBS) -Wl,-Bdynamic $(SHARE_LIBS) -L$(LIBS_SEARCH_DIR)

$(OBJS):%.o:%.c
	$(CC) -c $(CFLAGS) -I$(INCLUDE_DIR) $<

clean:
	$(RM) $(TARGET) $(OBJS)
