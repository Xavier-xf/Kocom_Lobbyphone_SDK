#include common.mk

CUR_PATH := .

TARGET_BIN := video_source_rt_media_test

SRCS := \
	video_source_rt_media_test.c

OBJS := $(SRCS:%.c=%.o)

INCLUDE_DIRS := \
    ../../../../utils/sys/include \
    ../../../../utils/debug/include \
    ../../../../utils/option/include \
    ../../../../utils/libevent/include \
    ../../../../utils/configfs_parser/include \
    ../../../../uvc/libuvc/include \
    ../../../../uvc/video-source/base/include

LIB_SEARCH_PATHS := \
    ../../../.. \
    $(STAGING_DIR)/usr/lib \
    $(STAGING_DIR)/usr/lib/rt_media

LOCAL_STATIC_LIBS := \
    libexpat \
    libVIPlite \
    libVIPuser \
    libawaiisp \
    libawion \
    librt_media \
    libuvc

inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))

LOCAL_CFLAGS := \
    $(CFLAGS) \
    $(inc_paths)

LOCAL_CXXFLAGS := \
    $(LDFLAGS) \
    $(CXXFLAGS) \
    $(CPPFLAGS) \
    $(patsubst %,-L%,$(LIB_SEARCH_PATHS)) \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

.PHONY: all clean

all:$(TARGET_BIN)

%.o:%.c
	$(CC) $(LOCAL_CFLAGS) $^ -c -o $@

$(TARGET_BIN):$(OBJS)
	$(CXX) $^ $(LOCAL_CXXFLAGS) -o $@

clean:
	-rm $(TARGET_BIN) $(OBJS)
