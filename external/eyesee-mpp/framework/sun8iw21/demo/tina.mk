# Makefile for eyesee-mpp/middleware/sample
CUR_PATH := .
PACKAGE_TOP := ..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

# used to store all the generated sample bin files
MPP_SAMPLES_BIN_DIR = bin

-include $(EYESEE_MPP_INCLUDE)/middleware/config/mpp_config.mk

# based on different samples, specify the corresponding configuration
ifeq ($(TARGET), sample_Camera)
SRCCS := sample_Camera/sample_Camera.cpp
LOCAL_TARGET_BIN := sample_Camera/sample_Camera
endif

ifeq ($(TARGET), sample_Camera_LargePhoto)
SRCCS := sample_Camera_LargePhoto/sample_Camera_LargePhoto.cpp
LOCAL_TARGET_BIN := sample_Camera_LargePhoto/sample_Camera_LargePhoto
endif

ifeq ($(TARGET), sample_CDREmergency)
SRCCS := sample_CDREmergency/sample_CDREmergency.cpp
LOCAL_TARGET_BIN := sample_CDREmergency/sample_CDREmergency
endif

ifeq ($(TARGET), sample_EncodeResolutionChange)
SRCCS := sample_EncodeResolutionChange/sample_EncodeResolutionChange.cpp
LOCAL_TARGET_BIN := sample_EncodeResolutionChange/sample_EncodeResolutionChange
endif

ifeq ($(TARGET), sample_RecorderCallbackOut)
SRCCS := sample_RecorderCallbackOut/sample_RecorderCallbackOut.cpp
LOCAL_TARGET_BIN := sample_RecorderCallbackOut/sample_RecorderCallbackOut
endif


ifeq ($(TARGET), sample_RecorderSegment)
SRCCS := sample_RecorderSegment/sample_RecorderSegment.cpp
LOCAL_TARGET_BIN := sample_RecorderSegment/sample_RecorderSegment
endif


ifeq ($(TARGET), sample_RecordLowDelay)
SRCCS := sample_RecordLowDelay/sample_RecordLowDelay.cpp
LOCAL_TARGET_BIN := sample_RecordLowDelay/sample_RecordLowDelay
endif


ifeq ($(TARGET), sample_RecordMultiStream)
SRCCS := sample_RecordMultiStream/sample_RecordMultiStream.cpp
LOCAL_TARGET_BIN := sample_RecordMultiStream/sample_RecordMultiStream
endif


ifeq ($(TARGET), sample_RecordSwitchFileNormal)
SRCCS := sample_RecordSwitchFileNormal/sample_RecordSwitchFileNormal.cpp
LOCAL_TARGET_BIN := sample_RecordSwitchFileNormal/sample_RecordSwitchFileNormal
endif


ifeq ($(TARGET), sample_ThumbRetriever)
SRCCS := sample_ThumbRetriever/sample_ThumbRetriever.cpp
LOCAL_TARGET_BIN := sample_ThumbRetriever/sample_ThumbRetriever
endif

ifeq ($(TARGET), sample_OSD)
SRCCS := sample_OSD/sample_OSD.cpp
LOCAL_TARGET_BIN := sample_OSD/sample_OSD
endif

ifeq ($(TARGET), sample_Player)
SRCCS := sample_Player/sample_Player.cpp
LOCAL_TARGET_BIN := sample_Player/sample_Player
endif

ifeq ($(TARGET), sample_USBCamera)
SRCCS := ./sample_USBCamera/sample_USBCamera.cpp
LOCAL_TARGET_BIN := ./sample_USBCamera/sample_USBCamera
endif

LOCAL_SHARED_LIBS :=
LOCAL_STATIC_LIBS :=

ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
LOCAL_SHARED_LIBS += \
	libdl \
	librt \
	libpthread
endif


ifeq ($(MPPCFG_COMPILE_DYNAMIC_LIB), Y)
######################## dynamic lib mode ########################
# Public dynamic library
LOCAL_SHARED_LIBS += \
    libexpat \
    libglog \
    liblog \
    libawion \
    libPluginMpp \
    libsample_confparser \
    libmedia_mpp \
    libmpp_component \
    libmedia_utils\
    libcustomaw_media_utils

# These are the libraries corresponding to MPP components
ifeq ($(MPPCFG_VI),Y)
LOCAL_SHARED_LIBS += \
    libmpp_vi \
    libmpp_isp \
    libISP
endif
ifeq ($(MPPCFG_VO),Y)
LOCAL_SHARED_LIBS += \
    libmpp_vo
endif
ifeq ($(MPPCFG_ISE),Y)
LOCAL_SHARED_LIBS += \
    libmpp_ise
endif
ifeq ($(MPPCFG_ADEC), Y)
LOCAL_SHARED_LIBS += \
    libadecoder
endif
ifeq ($(MPPCFG_EIS),Y)
LOCAL_SHARED_LIBS += \
    libmpp_eis \
    lib_eis
endif
ifeq ($(MPPCFG_UVC),Y)
LOCAL_SHARED_LIBS += \
    libmpp_uvc
endif

# These libraries are only used by the corresponding samples
LOCAL_STATIC_LIBS += \
    libcamera \
    librecorder \
    libplayer \
    libthumbretriever

ifeq ($(MPPCFG_UVC),Y)
LOCAL_STATIC_LIBS += libusbcamera
endif

else
######################## static lib mode ########################
# These only provide dynamic libraries
LOCAL_SHARED_LIBS += \
    libasound \
    libglog

# Public static library
LOCAL_STATIC_LIBS += \
    libexpat \
    libz \
    liblog \
    libawion \
    libaw_mpp \
    libmedia_utils \
    libcedarxstream \
    libsample_confparser\
    libPluginMpp \
    libIniParserMpp \
    libcdx_base \
    libResample\
    libAudioVps \
    libcustomaw_media_utils

# These are the libraries corresponding to MPP components
ifeq ($(MPPCFG_VI),Y)
LOCAL_STATIC_LIBS += \
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
LOCAL_STATIC_LIBS += \
    libcedarxrender
ifeq ($(MPPCFG_HW_DISPLAY),Y)
LOCAL_STATIC_LIBS += \
	libhwdisplay
endif
endif
ifeq ($(MPPCFG_TEXTENC),Y)
LOCAL_STATIC_LIBS += \
    libcedarx_tencoder
endif
ifneq ($(filter Y, $(MPPCFG_VENC) $(MPPCFG_VDEC)),)
LOCAL_STATIC_LIBS += \
    libMemAdapter \
    libVE \
    libcdc_base
endif
ifeq ($(MPPCFG_VENC),Y)
LOCAL_STATIC_LIBS += \
    libvencoder \
    libvenc_common \
    libvenc_base
endif
ifeq ($(MPPCFG_VENC_H264),Y)
LOCAL_STATIC_LIBS += libvenc_h264
endif
ifeq ($(MPPCFG_VENC_H265),Y)
LOCAL_STATIC_LIBS += libvenc_h265
endif
ifeq ($(MPPCFG_VENC_JPEG),Y)
LOCAL_STATIC_LIBS += libvenc_jpeg
endif
ifeq ($(MPPCFG_VDEC),Y)
LOCAL_STATIC_LIBS += \
    libvdecoder \
    libvideoengine
endif
ifeq ($(MPPCFG_VDEC_H264),Y)
LOCAL_STATIC_LIBS += libawh264
endif
ifeq ($(MPPCFG_VDEC_H265),Y)
LOCAL_STATIC_LIBS += libawh265
endif
ifeq ($(MPPCFG_VDEC_JPEG),Y)
LOCAL_STATIC_LIBS += libawmjpeg
endif
ifeq ($(MPPCFG_AENC),Y)
LOCAL_STATIC_LIBS += \
    libcedarx_aencoder
endif
ifeq ($(MPPCFG_AENC_AAC),Y)
LOCAL_STATIC_LIBS += \
    libaacenc
endif
ifeq ($(MPPCFG_AENC_MP3),Y)
LOCAL_STATIC_LIBS += \
    libmp3enc
endif
ifeq ($(MPPCFG_AENC_OPUS),Y)
LOCAL_STATIC_LIBS += \
    libopusenc
endif
ifeq ($(MPPCFG_ADEC), Y)
LOCAL_STATIC_LIBS += \
    libadecoder
endif
ifeq ($(MPPCFG_ADEC_WAV),Y)
LOCAL_STATIC_LIBS += \
    libwav
endif
ifeq ($(MPPCFG_ADEC_G726),Y)
LOCAL_STATIC_LIBS += \
    libaw_g726dec
endif
ifeq ($(MPPCFG_ADEC_AAC),Y)
LOCAL_STATIC_LIBS += \
    libaac
endif
ifeq ($(MPPCFG_ADEC_MP3),Y)
LOCAL_STATIC_LIBS += \
    libmp3
endif
ifeq ($(MPPCFG_ADEC_AMR),Y)
LOCAL_STATIC_LIBS += \
    libamr
endif
ifeq ($(MPPCFG_ADEC_APE),Y)
LOCAL_STATIC_LIBS += \
    libape
endif
ifeq ($(MPPCFG_ADEC_FLAC),Y)
LOCAL_STATIC_LIBS += \
    libflac
endif
ifeq ($(MPPCFG_ADEC_OGG),Y)
LOCAL_STATIC_LIBS += \
    libogg
endif
ifeq ($(MPPCFG_ADEC_OPUS),Y)
LOCAL_STATIC_LIBS += \
    libopus
endif
ifeq ($(MPPCFG_MUXER),Y)
LOCAL_STATIC_LIBS += \
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
LOCAL_STATIC_LIBS += \
    libcedarxdemuxer \
    libcdx_parser \
    libcdx_file_stream \
    libcdx_stream
endif
ifeq ($(MPPCFG_PARSER_AAC),Y)
LOCAL_STATIC_LIBS += libcdx_aac_parser
endif
ifeq ($(MPPCFG_PARSER_AMR),Y)
LOCAL_STATIC_LIBS += libcdx_amr_parser
endif
ifeq ($(MPPCFG_PARSER_APE),Y)
LOCAL_STATIC_LIBS += libcdx_ape_parser
endif
ifeq ($(MPPCFG_PARSER_FLAC),Y)
LOCAL_STATIC_LIBS += libcdx_flac_parser
endif
ifeq ($(MPPCFG_PARSER_ID3V2),Y)
LOCAL_STATIC_LIBS += libcdx_id3v2_parser
endif
ifeq ($(MPPCFG_PARSER_MOV),Y)
LOCAL_STATIC_LIBS += libcdx_mov_parser
endif
ifeq ($(MPPCFG_PARSER_MP3),Y)
LOCAL_STATIC_LIBS += libcdx_mp3_parser
endif
ifeq ($(MPPCFG_PARSER_MPG),Y)
LOCAL_STATIC_LIBS += libcdx_mpg_parser
endif
ifeq ($(MPPCFG_PARSER_OGG),Y)
LOCAL_STATIC_LIBS += libcdx_ogg_parser
endif
ifeq ($(MPPCFG_PARSER_TS),Y)
LOCAL_STATIC_LIBS += libcdx_ts_parser
endif
ifeq ($(MPPCFG_PARSER_WAV),Y)
LOCAL_STATIC_LIBS += libcdx_wav_parser
endif
ifeq ($(MPPCFG_STREAM_HTTP),Y)
LOCAL_STATIC_LIBS += libcdx_http_stream
endif
ifeq ($(MPPCFG_STREAM_TCP),Y)
LOCAL_STATIC_LIBS += libcdx_tcp_stream
endif
ifeq ($(MPPCFG_AEC),Y)
  ifeq ($(MPPCFG_AEC_LIB),webrtc)
    LOCAL_STATIC_LIBS += libAec
  else ifeq ($(MPPCFG_AEC_LIB),uvoice)
    LOCAL_SHARED_LIBS += libuvoice_ecnr_sdk
  endif
endif

ifeq ($(MPPCFG_SOFTDRC),Y)
LOCAL_STATIC_LIBS += \
    libDrc
endif
ifeq ($(MPPCFG_ANS),Y)
LOCAL_STATIC_LIBS += \
    libAns
endif
#ifneq ($(filter Y, $(MPPCFG_AGC)),)
#LOCAL_STATIC_LIBS += \
#    libAgcFloat
#endif
ifeq ($(MPPCFG_AGC), Y)
  ifeq ($(MPPCFG_AGC_LIB),webrtc)
    LOCAL_STATIC_LIBS += libAgc
  else ifeq ($(MPPCFG_AGC_LIB),agcfloat)
    LOCAL_STATIC_LIBS += libAgcFloat
  endif
endif
ifeq ($(MPPCFG_EIS),Y)
LOCAL_STATIC_LIBS += \
    libEIS \
    lib_eis
endif
ifeq ($(MPPCFG_ISE_MO),Y)
LOCAL_STATIC_LIBS += \
	lib_ise_mo
endif
ifeq ($(MPPCFG_ISE_GDC),Y)
LOCAL_STATIC_LIBS += \
	lib_ise_gdc
endif

# These libraries are only used by the corresponding samples
#ifneq (,$(filter $(TARGET),sample_Camera sample_EncodeResolutionChange sample_RecorderCallbackOut sample_RecorderSegment \
#    sample_RecordLowDelay sample_RecordMultiStream sample_RecordSwitchFileNormal sample_ThumbRetriever sample_OSD))
LOCAL_STATIC_LIBS += \
	libcamera \
	librecorder	
#endif

#ifneq (,$(filter $(TARGET),sample_Player))
LOCAL_STATIC_LIBS += \
    libplayer
#endif

ifneq (,$(filter $(TARGET),sample_ThumbRetriever))
LOCAL_STATIC_LIBS += \
    libthumbretriever
endif

ifneq (,$(filter $(TARGET),sample_USBCamera))
LOCAL_STATIC_LIBS += \
    libusbcamera
endif

endif

#LOCAL_SHARED_LIBS += libunwind

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/include/media \
    $(PACKAGE_TOP)/include/media/bdii \
    $(PACKAGE_TOP)/include/media/camera \
    $(PACKAGE_TOP)/include/media/eis \
	$(PACKAGE_TOP)/include/media/ise\
	$(PACKAGE_TOP)/include/media/motion \
	$(PACKAGE_TOP)/include/media/player \
	$(PACKAGE_TOP)/include/media/recorder \
	$(PACKAGE_TOP)/include/media/thumbretriever \
	$(PACKAGE_TOP)/include/media/usbcamera \
	$(PACKAGE_TOP)/include/media/videoresizer \
    $(PACKAGE_TOP)/include/utils \
    $(PACKAGE_TOP)/media/camera \
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
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libAIE_Vda/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_ai_common \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_eve_common \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libeveface/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_stream \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_FsWriter \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libcedarc/include \
	$(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/AudioLib/osal \
	$(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/AudioLib/midware/encoding \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/AudioLib/midware/encoding/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/AudioLib/midware/decoding/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/agc_lib/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/textEncLib/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/include_muxer \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libfilerepair/include \
    $(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libIniParser \
    $(EYESEE_MPP_INCLUDE)/middleware/sample/configfileparser \
	$(EYESEE_MPP_INCLUDE)/middleware/media/LIBRARY/libADAS/include \
    $(LINUX_USER_HEADERS)/include


#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-label -Wno-unused-function
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall -Wno-unused-but-set-variable -Wno-unused-variable -Wno-unused-label -Wno-unused-function
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LIB_SEARCH_PATHS := \
    $(EYESEE_MPP_LIBDIR) \
    $(PACKAGE_TOP)/media/bdii \
    $(PACKAGE_TOP)/media/camera \
    $(PACKAGE_TOP)/media/eis \
    $(PACKAGE_TOP)/media/ise \
    $(PACKAGE_TOP)/media/motion \
    $(PACKAGE_TOP)/media/player \
    $(PACKAGE_TOP)/media/recorder \
    $(PACKAGE_TOP)/media/thumbretriever \
    $(PACKAGE_TOP)/media/usbcamera \
    $(PACKAGE_TOP)/media/videoresizer \
    $(PACKAGE_TOP)/utils

empty:=
space:= $(empty) $(empty)

LOCAL_BIN_LDFLAGS := $(LOCAL_LDFLAGS) \
    $(patsubst %,-L%,$(LIB_SEARCH_PATHS)) \
    -Wl,-rpath-link=$(subst $(space),:,$(strip $(LIB_SEARCH_PATHS))) \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))
DEPEND_LIBS := $(wildcard $(foreach p, $(patsubst %/,%,$(LIB_SEARCH_PATHS)), \
                            $(addprefix $(p)/, \
                              $(foreach y,$(LOCAL_SHARED_LIBS),$(patsubst %,%.so,$(patsubst %.so,%,$(notdir $(y))))) \
                              $(foreach n,$(LOCAL_STATIC_LIBS),$(patsubst %,%.a,$(patsubst %.a,%,$(notdir $(n))))) \
                            ) \
                          ) \
               )

#generate exe file.
.PHONY: all
all: $(LOCAL_TARGET_BIN)
	@echo ===================================
	@echo build eyesee-mpp-middleware-sample-$(LOCAL_TARGET_BIN) done
	@echo ===================================

$(LOCAL_TARGET_BIN): $(OBJS) $(DEPEND_LIBS)
	$(CXX) $(OBJS) $(LOCAL_BIN_LDFLAGS) -o $@
	-mkdir -p $(MPP_SAMPLES_BIN_DIR)
	-cp -f $@ $(MPP_SAMPLES_BIN_DIR)
	-cp -f $(wildcard *.conf $(TARGET)/*.conf) $(MPP_SAMPLES_BIN_DIR)
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

#patten rules to generate local object files
$(filter %.cpp.o %.cc.o, $(OBJS)): %.o: %
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(@:%=%.d) -c -o $@ $<
$(filter %.c.o, $(OBJS)): %.o: %
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(@:%=%.d) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -rf $(OBJS) $(OBJS:%=%.d) $(LOCAL_TARGET_BIN) $(MPP_SAMPLES_BIN_DIR)

#add *.h prerequisites
-include $(OBJS:%=%.d)
