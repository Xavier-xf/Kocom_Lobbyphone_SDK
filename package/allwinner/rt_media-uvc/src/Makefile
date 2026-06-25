CUR_PATH := .

TARGET_BIN := rt_media-uvc

VIDEO_SOURCE_RT_MEDIA := 1
VIDEO_SOURCE_COLORBAR := 1

SRCS := \
	uvc/libuvc/video_buffers.c \
	uvc/libuvc/uvc.c \
	uvc/libuvc/stream.c \
	uvc/libuvc/v4l2.c \
	uvc/video-source/base/video_source.c \
	utils/g2d/g2d_helper.c \
	utils/configfs_parser/configfs_parser.c \
	utils/option/option.c \
	utils/libevent/demo_events.c \
	uac/uac.c \
	uac/alsa_interface.c \
	rt_media-uvc.c

ifeq ($(VIDEO_SOURCE_RT_MEDIA), 1)
  SRCS += \
	uvc/video-source/rt_media/rt_awaiisp_common.c \
	uvc/video-source/rt_media/video_source_rt_media.c
endif
#ifeq ($(VIDEO_SOURCE_MPP), 1)
#  SRCS += uvc/video-source/mpp/video_source_mpp.c \
#	uvc/video-source/mpp/sample_common_venc.c
#endif
ifeq ($(VIDEO_SOURCE_COLORBAR), 1)
  SRCS += uvc/video-source/colorbar/video_source_colorbar.c
endif

ifeq ($(CONFIG_mpp_aec_libwebrtc), y)
  SRCS += uac/uac_aec.c
endif
ifeq ($(CONFIG_mpp_ans), y)
  SRCS += uac/uac_ans.c
endif
ifeq ($(CONFIG_mpp_agc), y)
  SRCS += uac/uac_agc.c
endif

OBJS := $(SRCS:%.c=%.o)

INCLUDE_DIR := \
	-I$(KERNEL_DIR)/user_headers/include \
	-I$(STAGING_DIR)/usr/include/rt_media \
	-I$(STAGING_DIR)/usr/include/libawaiisp \
	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware

#ifeq ($(VIDEO_SOURCE_MPP), 1)
#INCLIDE_DIR += \
#	-I$(STAGING_DIR)/usr/include/libcedarc/base/include \
#	-I$(STAGING_DIR)/usr/include/libcedarc/include \
#	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/include/component \
#	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/include/utils \
#	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/include/media \
#	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp \
#	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/include \
#	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/isp_tuning \
#	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/include/device \
#	-I$(STAGING_DIR)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/include/V4l2Camera \
#endif

LIBS_DIR := \
	-L$(STAGING_DIR)/usr/lib/rt_media \
	-L$(STAGING_DIR)/usr/lib \
	-L$(STAGING_DIR)/usr/lib/eyesee-mpp


ifeq ($(CONFIG_mpp_compile_static_lib),y)

SHARED_LIBS := \
	-lpthread \
	-lexpat \
	-lasound \
	-llog

STATIC_LIBS := \
	-lrt_media \
	-lawion \
	-lawaiisp \
	-lawnn_full

LOCAL_CFLAGS := \
	$(CFLAGS)

ifeq ($(CONFIG_mpp_aec_libwebrtc), y)
  STATIC_LIBS += -lAec
  LOCAL_CFLAGS += -DENABLE_AEC=1
endif
ifeq ($(CONFIG_mpp_agc), y)
  STATIC_LIBS += -lAgc
  LOCAL_CFLAGS += -DENABLE_AGC=1
endif
ifeq ($(CONFIG_mpp_ans), y)
  STATIC_LIBS += -lAns
  LOCAL_CFLAGS += -DENABLE_ANS=1
endif

else

SHARED_LIBS := \
	-lpthread \
	-lexpat \
	-lasound \
	-llog \
	-lrt_media \
	-lawion \
	-lawaiisp \
	-lawnn_full \
	-lmedia_mpp

LOCAL_CFLAGS := \
	$(CFLAGS)

endif

LOCAL_CFLAGS += -DVIDEO_SOURCE_RT_MEDIA=$(VIDEO_SOURCE_RT_MEDIA)
LOCAL_CFLAGS += -DVIDEO_SOURCE_COLORBAR=$(VIDEO_SOURCE_COLORBAR)

ifeq ($(LICHEE_CHIP), sun8iw12p1)
  LOCAL_CFLAGS += -DAWCHIP=0x1721
else ifeq ($(LICHEE_CHIP), sun8iw16p1)
  LOCAL_CFLAGS += -DAWCHIP=0x1816
else ifeq ($(LICHEE_CHIP), sun8iw19p1)
  LOCAL_CFLAGS += -DAWCHIP=0x1817
else ifeq ($(LICHEE_CHIP), sun8iw21p1)
  LOCAL_CFLAGS += -DAWCHIP=0x1886
else ifeq ($(LICHEE_CHIP), sun300iw1p1)
  LOCAL_CFLAGS += -DAWCHIP=0x1882
else
  LOCAL_CFLAGS += -DAWCHIP=0x1721
endif

LOCAL_CXXFLAGS := \
    $(LDFLAGS) \
    $(CXXFLAGS) \
    $(CPPFLAGS) \
    $(LIBS_DIR) \
    -Wl,-Bstatic -Wl,--whole-archive -Wl,--start-group $(STATIC_LIBS) -Wl,--end-group -Wl,--no-whole-archive \
    -Wl,-Bdynamic $(SHARED_LIBS)

.PHONY: all clean

all:$(TARGET_BIN)

%.o:%.c
	$(CC) $(LOCAL_CFLAGS) $(INCLUDE_DIR) $^ -MD -MP -MF $(@:%=%.d) -c -o $@

$(TARGET_BIN):$(OBJS)
	$(CXX) $^ $(LOCAL_CXXFLAGS) -o $@

clean:
	$(RM) $(TARGET_BIN) $(OBJS)
