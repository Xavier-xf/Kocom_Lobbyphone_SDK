CUR_PATH := .
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=xxx/out/v316-perfnor/staging_dir/target
RTMEDIA_INCLUDE:=$(STAGING_DIR)/usr/include/rt_media
RTMEDIA_LIBDIR:=$(STAGING_DIR)/usr/lib/rt_media
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
LIBCEDARC_INCLUDE:= \
	$(STAGING_DIR)/usr/include/libcedarc/include \
	$(STAGING_DIR)/usr/include/libcedarc/base/include
LIBCEDARC_LIBDIR:=$(STAGING_DIR)/usr/lib/

-include $(EYESEE_MPP_INCLUDE)/middleware/config/mpp_config.mk

LOCAL_TARGET_BIN := demo_aov

VIDEO_SOURCE_TYPE_MPP := 0
VIDEO_SOURCE_TYPE_RT_MEDIA := 1
VIDEO_ENCODE_TYPE_MPP := 0
VIDEO_ENCODE_TYPE_RT_MEDIA := 1
USE_MPP_MUXER := 1

SRCCS := \
	persion_detect/rt_aiservice_pdet.c \
	persion_detect/persion_detect.c \
	video_encode/base/video_encode.c \
	video_source/base/video_source.c \
	utils/option.c \
	utils/sdcard_manager.c \
	video_source/rt_media/video_source_rt_media.c \
	video_encode/rt_media/video_encode_rt_media.c \
	utils/demo_aov_mux.c \
	demo_aov.c

LOCAL_MPP_SHARED_LIBS :=
LOCAL_MPP_STATIC_LIBS :=
ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
LOCAL_SHARED_LIBS += \
	libdl \
	librt \
	libpthread
endif


ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
######################## dynamic lib mode ########################
# Public dynamic library
LOCAL_MPP_SHARED_LIBS += \
    libz \
    liblog \
    libawion \
    libmedia_mpp \
    libmpp_component \
    libmedia_utils

ifneq ($(filter Y, $(MPP_VENC_SUPPORT_VENC_PARAM_DEBUG) $(RT_MEDIA_SUPPORT_VENC_PARAM_DEBUG)),)
LOCAL_MPP_SHARED_LIBS += \
    libexpat
endif

ifeq ($(MPPCFG_SAMPLE_CONFIGFILEPARSER),Y)
LOCAL_MPP_SHARED_LIBS += \
    libsample_confparser
endif
# These are the libraries corresponding to MPP components
ifeq ($(MPPCFG_VI),Y)
LOCAL_MPP_SHARED_LIBS += \
    libmpp_vi \
    libmpp_isp \
    libISP
endif
ifeq ($(MPPCFG_VO),Y)
LOCAL_MPP_SHARED_LIBS += \
    libmpp_vo
endif
ifeq ($(MPPCFG_ISE),Y)
LOCAL_MPP_SHARED_LIBS += \
    libmpp_ise
endif
ifneq ($(filter Y, $(MPPCFG_VENC) $(MPPCFG_VDEC)),)
LOCAL_MPP_SHARED_LIBS += \
    libcdc_base
endif
ifeq ($(MPPCFG_ADEC), Y)
LOCAL_MPP_SHARED_LIBS += \
    libadecoder
endif
ifeq ($(MPPCFG_EIS),Y)
LOCAL_MPP_SHARED_LIBS += \
    libmpp_eis \
    lib_eis
endif
ifeq ($(MPPCFG_UVC),Y)
LOCAL_MPP_SHARED_LIBS += \
    libmpp_uvc
endif

else
######################## static lib mode ########################
# These only provide dynamic libraries
LOCAL_MPP_SHARED_LIBS += \
    libasound \
    liblog

# Public static library
LOCAL_MPP_STATIC_LIBS += \
    libz \
    libawion \
    libaw_mpp \
    libmedia_utils

ifneq ($(filter Y, $(MPP_VENC_SUPPORT_VENC_PARAM_DEBUG) $(RT_MEDIA_SUPPORT_VENC_PARAM_DEBUG)),)
LOCAL_MPP_STATIC_LIBS += \
    libexpat
endif

ifneq ($(filter Y, $(MPPCFG_DEMUXER) $(MPPCFG_SAMPLE_CONFIGFILEPARSER)),)
LOCAL_MPP_STATIC_LIBS += \
    libPluginMpp \
    libIniParserMpp \
    libcdx_base
endif
ifeq ($(MPPCFG_AIO),Y)
LOCAL_MPP_STATIC_LIBS += \
    libResample \
    libAudioVps
endif
ifeq ($(MPPCFG_SAMPLE_CONFIGFILEPARSER),Y)
LOCAL_MPP_STATIC_LIBS += \
    libsample_confparser
endif
# These are the libraries corresponding to MPP components
ifeq ($(MPPCFG_VI),Y)
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
ifeq ($(MPPCFG_VO),Y)
LOCAL_MPP_STATIC_LIBS += \
    libcedarxrender
ifeq ($(MPPCFG_HW_DISPLAY),Y)
LOCAL_MPP_STATIC_LIBS += \
	libhwdisplay
endif
endif
ifeq ($(MPPCFG_TEXTENC),Y)
LOCAL_MPP_STATIC_LIBS += \
    libcedarx_tencoder
endif
ifneq ($(filter Y, $(MPPCFG_VENC) $(MPPCFG_VDEC)),)
LOCAL_MPP_STATIC_LIBS += \
    libMemAdapter \
    libVE \
    libcdc_base
endif
ifeq ($(MPPCFG_VENC),Y)
LOCAL_MPP_STATIC_LIBS += \
    libvencoder \
    libvenc_common \
    libvenc_base
endif
ifeq ($(MPPCFG_VENC_H264),Y)
LOCAL_MPP_STATIC_LIBS += libvenc_h264
endif
ifeq ($(MPPCFG_VENC_H265),Y)
LOCAL_MPP_STATIC_LIBS += libvenc_h265
endif
ifeq ($(MPPCFG_VENC_JPEG),Y)
LOCAL_MPP_STATIC_LIBS += libvenc_jpeg
endif
ifeq ($(MPPCFG_VDEC),Y)
LOCAL_MPP_STATIC_LIBS += \
    libvdecoder \
    libvideoengine
endif
ifeq ($(MPPCFG_VDEC_H264),Y)
LOCAL_MPP_STATIC_LIBS += libawh264
endif
ifeq ($(MPPCFG_VDEC_H265),Y)
LOCAL_MPP_STATIC_LIBS += libawh265
endif
ifeq ($(MPPCFG_VDEC_JPEG),Y)
LOCAL_MPP_STATIC_LIBS += libawmjpeg
endif
ifeq ($(MPPCFG_AENC),Y)
LOCAL_MPP_STATIC_LIBS += \
    libcedarx_aencoder
endif
ifeq ($(MPPCFG_AENC_AAC),Y)
LOCAL_MPP_STATIC_LIBS += \
    libaacenc
endif
ifeq ($(MPPCFG_AENC_MP3),Y)
LOCAL_MPP_STATIC_LIBS += \
    libmp3enc
endif
ifeq ($(MPPCFG_ADEC), Y)
LOCAL_MPP_STATIC_LIBS += \
    libadecoder
endif
ifeq ($(MPPCFG_ADEC_WAV),Y)
LOCAL_MPP_STATIC_LIBS += \
    libwav
endif
ifeq ($(MPPCFG_ADEC_G726),Y)
LOCAL_MPP_STATIC_LIBS += \
    libaw_g726dec
endif
ifeq ($(MPPCFG_ADEC_AAC),Y)
LOCAL_MPP_STATIC_LIBS += \
    libaac
endif
ifeq ($(MPPCFG_ADEC_MP3),Y)
LOCAL_MPP_STATIC_LIBS += \
    libmp3
endif
ifeq ($(MPPCFG_ADEC_AMR),Y)
LOCAL_MPP_STATIC_LIBS += \
    libamr
endif
ifeq ($(MPPCFG_ADEC_APE),Y)
LOCAL_MPP_STATIC_LIBS += \
    libape
endif
ifeq ($(MPPCFG_ADEC_FLAC),Y)
LOCAL_MPP_STATIC_LIBS += \
    libflac
endif
ifeq ($(MPPCFG_ADEC_OGG),Y)
LOCAL_MPP_STATIC_LIBS += \
    libogg
endif
ifeq ($(MPPCFG_ADEC_OPUS),Y)
LOCAL_MPP_STATIC_LIBS += \
    libopus
endif
ifeq ($(MPPCFG_MUXER),Y)
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
ifeq ($(MPPCFG_DEMUXER),Y)
LOCAL_MPP_STATIC_LIBS += \
    libcedarxdemuxer \
    libcdx_parser \
    libcdx_file_stream \
    libcdx_stream
endif
ifeq ($(MPPCFG_PARSER_AAC),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_aac_parser
endif
ifeq ($(MPPCFG_PARSER_AMR),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_amr_parser
endif
ifeq ($(MPPCFG_PARSER_APE),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_ape_parser
endif
ifeq ($(MPPCFG_PARSER_FLAC),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_flac_parser
endif
ifeq ($(MPPCFG_PARSER_ID3V2),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_id3v2_parser
endif
ifeq ($(MPPCFG_PARSER_MOV),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_mov_parser
endif
ifeq ($(MPPCFG_PARSER_MP3),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_mp3_parser
endif
ifeq ($(MPPCFG_PARSER_MPG),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_mpg_parser
endif
ifeq ($(MPPCFG_PARSER_OGG),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_ogg_parser
endif
ifeq ($(MPPCFG_PARSER_TS),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_ts_parser
endif
ifeq ($(MPPCFG_PARSER_WAV),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_wav_parser
endif
ifeq ($(MPPCFG_STREAM_HTTP),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_http_stream
endif
ifeq ($(MPPCFG_STREAM_TCP),Y)
LOCAL_MPP_STATIC_LIBS += libcdx_tcp_stream
endif
ifeq ($(MPPCFG_AEC),Y)
  ifeq ($(MPPCFG_AEC_LIB),webrtc)
    LOCAL_MPP_STATIC_LIBS += libAec
  else ifeq ($(MPPCFG_AEC_LIB),uvoice)
    LOCAL_MPP_STATIC_LIBS += libuvoice_ecnr_sdk
  endif
endif
ifeq ($(MPPCFG_SOFTDRC),Y)
LOCAL_MPP_STATIC_LIBS += \
    libDrc
endif
ifeq ($(MPPCFG_ANS),Y)
  ifeq ($(MPPCFG_ANS_LIB),libwebrtc)
    LOCAL_MPP_STATIC_LIBS += libAns
  else ifeq ($(MPPCFG_ANS_LIB),liblstm)
    LOCAL_MPP_STATIC_LIBS += libAns
  else ifeq ($(MPPCFG_ANS_LIB),libnosc)
    LOCAL_MPP_STATIC_LIBS += libAns
  endif
endif
ifeq ($(MPPCFG_AGC), Y)
  ifeq ($(MPPCFG_AGC_LIB),webrtc)
    LOCAL_MPP_STATIC_LIBS += libAgc
  else ifeq ($(MPPCFG_AGC_LIB),agcfloat)
    LOCAL_MPP_STATIC_LIBS += libAgcFloat
  endif
endif
ifeq ($(MPPCFG_EIS),Y)
LOCAL_MPP_STATIC_LIBS += \
    libEIS \
    lib_eis
endif
ifeq ($(MPPCFG_ISE_MO),Y)
LOCAL_MPP_STATIC_LIBS += \
	lib_ise_mo
endif
ifeq ($(MPPCFG_ISE_GDC),Y)
LOCAL_MPP_STATIC_LIBS += \
	lib_ise_gdc
endif
endif

LOCAL_SHARED_LIBS := \
	libpthread

LOCAL_STATIC_LIBS := \
	libawion \
	liblz4 \
	librgb_ctrl \
	librt_media \
	libVIPlite \
	libVIPuser \
	libawnn

ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
LOCAL_SHARED_LIBS += \
    libdl \
    librt
endif
ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
LOCAL_SHARED_LIBS += \
	$(LOCAL_MPP_SHARED_LIBS)
else
LOCAL_SHARED_LIBS += \
	$(LOCAL_MPP_SHARED_LIBS)
LOCAL_STATIC_LIBS += \
	$(LOCAL_MPP_STATIC_LIBS)
endif

INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(RTMEDIA_INCLUDE) \
    $(STAGING_DIR)/usr/include/viplite-driver \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(EYESEE_MPP_INCLUDE)/system/public/include/vo \
    $(EYESEE_MPP_INCLUDE)/system/public/include/openssl \
    $(EYESEE_MPP_INCLUDE)/system/public/include/crypto \
    $(EYESEE_MPP_INCLUDE)/system/public/rgb_ctrl \
    $(EYESEE_MPP_INCLUDE)/system/public/libion/include \
    $(EYESEE_MPP_INCLUDE)/system/private/rtsp/IPCProgram/interface \
    $(EYESEE_MPP_INCLUDE)/external/sound_controler \
    $(EYESEE_MPP_INCLUDE)/external/sound_controler/include \
    $(EYESEE_MPP_INCLUDE)/middleware/include/utils \
    $(EYESEE_MPP_INCLUDE)/middleware/include/media \
    $(EYESEE_MPP_INCLUDE)/middleware/include/media/utils \
    $(EYESEE_MPP_INCLUDE)/middleware/include \
    $(EYESEE_MPP_INCLUDE)/middleware/config \
    $(EYESEE_MPP_INCLUDE)/middleware/media/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/include/utils \
    $(EYESEE_MPP_INCLUDE)/middleware/media/include/component \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libISE \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libISE/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp/include/device \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp/include/V4l2Camera \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libisp/isp_tuning \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_stream \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_FsWriter \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libcedarc/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/AudioLib/osal \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/AudioLib/midware/encoding \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/AudioLib/midware/decoding/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/agc_lib/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/agc_float_lib/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_muxer \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libfilerepair/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libIniParser \
    $(EYESEE_MPP_INCLUDE)/middleware/sample/configfileparser \
    $(LINUX_USER_HEADERS)/include

LIB_SEARCH_PATHS := \
    $(RTMEDIA_LIBDIR) \
    $(LIBCEDARC_LIBDIR) \
    $(STAGING_DIR)/usr/lib/viplite-driver

LIB_SEARCH_PATHS += \
	$(EYESEE_MPP_LIBDIR)

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) \
	-fPIC -Wall -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-label \
	-Wno-unused-function
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-label -Wno-unused-function
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

empty:=
space:= $(empty) $(empty)

LOCAL_BIN_LDFLAGS := $(LOCAL_LDFLAGS) \
    $(patsubst %,-L%,$(LIB_SEARCH_PATHS)) \
    -Wl,-rpath-link=$(subst $(space),:,$(strip $(LIB_SEARCH_PATHS))) \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))
#-Wl,--whole-archive \
#-Wl,--no-whole-archive \

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))
DEPEND_LIBS := $(wildcard $(foreach p, $(patsubst %/,%,$(LIB_SEARCH_PATHS)), \
                            $(addprefix $(p)/, \
                              $(foreach y,$(LOCAL_SHARED_LIBS),$(patsubst %,%.so,$(patsubst %.so,%,$(notdir $(y))))) \
                              $(foreach n,$(LOCAL_STATIC_LIBS),$(patsubst %,%.a,$(patsubst %.a,%,$(notdir $(n))))) \
                            ) \
                          ) \
               )

.PHONY: all
all:$(LOCAL_TARGET_BIN)
	@echo "build $(LOCAL_TARGET_BIN) done"

$(LOCAL_TARGET_BIN):$(OBJS) $(DEPEND_LIBS)
	$(CXX) $(OBJS) $(LOCAL_BIN_LDFLAGS) -o $@

$(filter %.c.o, $(OBJS)): %.o: %
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(@:%=%.d) -c -o $@ $<

.PHONY: clean
clean:
	-rm -rf $(OBJS) $(OBJS:%=%.d) $(LOCAL_TARGET_BIN)

#add *.h prerequisites
-include $(OBJS:%=%.d)
