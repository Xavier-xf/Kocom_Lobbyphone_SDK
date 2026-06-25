#include common.mk

CUR_PATH := .

TARGET_BIN := video_source_mpp_test

SRCS := \
	video_source_mpp_test.c

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
    $(STAGING_DIR)/usr/lib/rt_media \
    $(STAGING_DIR)/usr/lib/eyesee-mpp

LOCAL_MPP_SHARED_LIBS :=
LOCAL_MPP_STATIC_LIBS :=
ifeq ($(CONFIG_mpp_compile_dynamic_lib), y)
######################## dynamic lib mode ########################
# Public dynamic library
LOCAL_MPP_SHARED_LIBS += \
    libz \
    libglog \
    liblog \
    libawion \
    libmedia_mpp \
    libmpp_component \
    libmedia_utils

ifeq ($(CONFIG_CONFIG_mpp_venc_support_venc_param_debug),y)
LOCAL_MPP_SHARED_LIBS += \
    libexpat
endif

ifeq ($(CONFIG_mpp_sample_configfileparser),y)
LOCAL_MPP_SHARED_LIBS += \
    libsample_confparser
endif
# These are the libraries corresponding to MPP components
ifeq ($(CONFIG_mpp_vi),y)
LOCAL_MPP_SHARED_LIBS += \
    libmpp_vi \
    libmpp_isp \
    libISP
endif
ifeq ($(CONFIG_mpp_vo),y)
LOCAL_MPP_SHARED_LIBS += \
    libmpp_vo
endif
ifneq ($(filter y, $(CONFIG_mpp_venc) $(CONFIG_mpp_vdec)),)
LOCAL_MPP_SHARED_LIBS += \
    libcdc_base
endif
ifeq ($(CONFIG_mpp_adec), y)
LOCAL_MPP_SHARED_LIBS += \
    libadecoder
endif
ifeq ($(CONFIG_mpp_uvc),y)
LOCAL_MPP_SHARED_LIBS += \
    libmpp_uvc
endif

else
######################## static lib mode ########################
# These only provide dynamic libraries
LOCAL_MPP_SHARED_LIBS += \
    libasound \
    libglog

# Public static library
LOCAL_MPP_STATIC_LIBS += \
    libz \
    liblog \
    libawion \
    libaw_mpp \
    libmedia_utils

ifeq ($(CONFIG_mpp_venc_support_venc_param_debug),y)
LOCAL_MPP_STATIC_LIBS += \
    libexpat
endif

ifneq ($(filter y, $(CONFIG_mpp_demuxer) $(CONFIG_mpp_sample_configfileparser)),)
LOCAL_MPP_STATIC_LIBS += \
    libcdx_common \
    libcdx_base
endif
ifeq ($(CONFIG_mpp_aio),y)
LOCAL_MPP_STATIC_LIBS += \
    libResample \
    libAudioVps
endif
ifeq ($(CONFIG_mpp_sample_configfileparser),y)
LOCAL_MPP_STATIC_LIBS += \
    libsample_confparser
endif
# These are the libraries corresponding to MPP components
ifeq ($(CONFIG_mpp_vi),y)
LOCAL_MPP_STATIC_LIBS += \
    libISP \
    libisp_dev \
    libisp_ini \
    libiniparser \
    libisp_ae \
    libisp_af \
    libisp_afs \
    libisp_awb \
    libisp_base \
    libisp_gtm \
    libisp_iso \
    libisp_math \
    libisp_md \
    libisp_pltm \
    libisp_rolloff
endif
ifeq ($(CONFIG_mpp_vo),y)
LOCAL_MPP_STATIC_LIBS += \
    libcedarxrender
ifeq ($(CONFIG_mpp_hw_display),y)
LOCAL_MPP_STATIC_LIBS += \
	libhwdisplay
endif
endif
ifeq ($(CONFIG_mpp_textenc),y)
LOCAL_MPP_STATIC_LIBS += \
    libcedarx_tencoder
endif
ifneq ($(filter y, $(CONFIG_mpp_venc) $(CONFIG_mpp_vdec)),)
LOCAL_MPP_STATIC_LIBS += \
    libMemAdapter \
    libVE \
    libcdc_base
endif
ifeq ($(CONFIG_mpp_venc),y)
LOCAL_MPP_STATIC_LIBS += \
    libvencoder \
    libvenc_common \
    libvenc_base
endif
ifeq ($(CONFIG_mpp_venc_h264),y)
LOCAL_MPP_STATIC_LIBS += libvenc_h264
endif
ifeq ($(CONFIG_mpp_venc_h265),y)
LOCAL_MPP_STATIC_LIBS += libvenc_h265
endif
ifeq ($(CONFIG_mpp_venc_jpeg),y)
LOCAL_MPP_STATIC_LIBS += libvenc_jpeg
endif
ifeq ($(CONFIG_mpp_vdec),y)
LOCAL_MPP_STATIC_LIBS += \
    libvdecoder \
    libvideoengine
endif
ifeq ($(CONFIG_mpp_vdec_h264),y)
LOCAL_MPP_STATIC_LIBS += libawh264
endif
ifeq ($(CONFIG_mpp_vdec_h265),y)
LOCAL_MPP_STATIC_LIBS += libawh265
endif
ifeq ($(CONFIG_mpp_vdec_jpeg),y)
LOCAL_MPP_STATIC_LIBS += libawmjpeg
endif
ifeq ($(CONFIG_mpp_aenc),y)
LOCAL_MPP_STATIC_LIBS += \
    libcedarx_aencoder
endif
ifeq ($(CONFIG_mpp_aenc_aac),y)
LOCAL_MPP_STATIC_LIBS += \
    libaacenc
endif
ifeq ($(CONFIG_mpp_aenc_mp3),y)
LOCAL_MPP_STATIC_LIBS += \
    libmp3enc
endif
ifeq ($(CONFIG_mpp_adec), y)
LOCAL_MPP_STATIC_LIBS += \
    libadecoder
endif
ifeq ($(CONFIG_mpp_adec_wav),y)
LOCAL_MPP_STATIC_LIBS += \
    libwav
endif
ifeq ($(CONFIG_mpp_adec_g726),y)
LOCAL_MPP_STATIC_LIBS += \
    libaw_g726dec
endif
ifeq ($(CONFIG_mpp_adec_aac),y)
LOCAL_MPP_STATIC_LIBS += \
    libaac
endif
ifeq ($(CONFIG_mpp_adec_mp3),y)
LOCAL_MPP_STATIC_LIBS += \
    libmp3
endif
ifeq ($(CONFIG_mpp_adec_amr),y)
LOCAL_MPP_STATIC_LIBS += \
    libamr
endif
ifeq ($(CONFIG_mpp_adec_ape),y)
LOCAL_MPP_STATIC_LIBS += \
    libape
endif
ifeq ($(CONFIG_mpp_adec_flac),y)
LOCAL_MPP_STATIC_LIBS += \
    libflac
endif
ifeq ($(CONFIG_mpp_adec_ogg),y)
LOCAL_MPP_STATIC_LIBS += \
    libogg
endif
ifeq ($(CONFIG_mpp_adec_opus),y)
LOCAL_MPP_STATIC_LIBS += \
    libopus
endif
ifeq ($(CONFIG_mpp_muxer),y)
LOCAL_MPP_STATIC_LIBS += \
    libcedarxstream \
    libmuxers \
    libmp4_muxer \
    libraw_muxer \
    libmpeg2ts_muxer \
    libaac_muxer \
    libmp3_muxer \
    libwav_muxer \
    libffavutil \
    libFsWriter
endif
ifeq ($(CONFIG_mpp_demuxer),y)
LOCAL_MPP_STATIC_LIBS += \
    libcedarxdemuxer \
    libcdx_aac_parser \
    libcdx_id3v2_parser \
    libcdx_mp3_parser \
    libcdx_mov_parser \
    libcdx_mpg_parser \
    libcdx_ts_parser \
    libcdx_wav_parser \
    libcdx_amr_parser \
    libcdx_ape_parser \
    libcdx_flac_parser \
    libcdx_ogg_parser \
    libcdx_parser \
    libcdx_file_stream \
    libcdx_stream
endif
ifeq ($(CONFIG_mpp_stream_http),y)
LOCAL_MPP_STATIC_LIBS += libcdx_http_stream
endif
ifeq ($(CONFIG_mpp_stream_tcp),y)
LOCAL_MPP_STATIC_LIBS += libcdx_tcp_stream
endif
ifeq ($(CONFIG_mpp_aec),y)
  ifeq ($(CONFIG_mpp_aec_libwebrtc),y)
    LOCAL_MPP_STATIC_LIBS += libAec
  else ifeq ($(CONFIG_mpp_aec_libuvoice),y)
    LOCAL_MPP_SHARED_LIBS += libuvoice_ecnr_sdk
  endif
endif
ifeq ($(CONFIG_mpp_softdrc),y)
LOCAL_MPP_STATIC_LIBS += \
    libDrc
endif
ifeq ($(CONFIG_mpp_ans),y)
  ifeq ($(CONFIG_mpp_ans_libwebrtc),y)
    LOCAL_MPP_STATIC_LIBS += libAns
  else ifeq ($(CONFIG_mpp_ans_liblstm),y)
    LOCAL_MPP_STATIC_LIBS += libAns
  else ifeq ($(CONFIG_mpp_ans_libnosc),y)
    LOCAL_MPP_STATIC_LIBS += libAns
  endif
endif
ifeq ($(CONFIG_mpp_agc), y)
  ifeq ($(CONFIG_mpp_agc_LIB),webrtc)
    LOCAL_MPP_STATIC_LIBS += libAgc
  else ifeq ($(CONFIG_mpp_agc_LIB),agcfloat)
    LOCAL_MPP_STATIC_LIBS += libAgcFloat
  endif
endif

endif

LOCAL_STATIC_LIBS := \
    libuvc

ifeq ($(CONFIG_PACKAGE_rt_media), y)
LOCAL_SHARED_LIBS += \
	libexpat
LOCAL_STATIC_LIBS += \
	librt_media
else
LOCAL_SHARED_LIBS += \
	$(LOCAL_MPP_SHARED_LIBS)
LOCAL_STATIC_LIBS += \
	$(LOCAL_MPP_STATIC_LIBS)
endif

inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))

ifeq ($(CONFIG_PACKAGE_rt_media), y)
VIDEO_SOURCE_RT_MEDIA := 1
else
VIDEO_SOURCE_RT_MEDIA := 0
endif

LOCAL_CFLAGS := \
    $(CFLAGS) \
    -DVIDEO_SOURCE_RT_MEDIA=$(VIDEO_SOURCE_RT_MEDIA) \
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
