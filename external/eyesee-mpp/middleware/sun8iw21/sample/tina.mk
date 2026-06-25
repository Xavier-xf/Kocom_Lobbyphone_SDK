# Makefile for eyesee-mpp/middleware/sample
CUR_PATH := .
PACKAGE_TOP := ..
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

# used to store all the generated sample bin files
MPP_SAMPLES_BIN_DIR = bin

include $(PACKAGE_TOP)/config/mpp_config.mk

PRODUCT = $(shell echo $(TARGET_PRODUCT) | cut -d '_' -f 1)

# based on different samples, specify the corresponding configuration
ifeq ($(TARGET), sample_vencRecreate)
SRCCS := uncommonly_samples/sample_vencRecreate/sample_vencRecreate.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_vencRecreate/sample_vencRecreate
endif
ifeq ($(TARGET), sample_takePicture)
SRCCS := \
	sample_takePicture/sample_takePicture.c \
	common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_takePicture/sample_takePicture
endif
ifeq ($(TARGET), sample_vencGdcZoom)
SRCCS := \
	sample_vencGdcZoom/sample_vencGdcZoom.c \
	common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_vencGdcZoom/sample_vencGdcZoom
endif

ifeq ($(TARGET), sample_OnlineVenc)
SRCCS := \
	sample_OnlineVenc/sample_OnlineVenc.c \
	common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_OnlineVenc/sample_OnlineVenc
endif
ifeq ($(TARGET), sample_vencQpMap)
SRCCS := uncommonly_samples/sample_vencQpMap/sample_vencQpMap.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_vencQpMap/sample_vencQpMap
endif
ifeq ($(TARGET), sample_EncppGdcOffline)
SRCCS := sample_EncppGdcOffline/sample_EncppGdcOffline.c
LOCAL_TARGET_BIN := sample_EncppGdcOffline/sample_EncppGdcOffline
endif
ifeq ($(TARGET), sample_virvi2vo_zoom)
SRCCS := uncommonly_samples/sample_virvi2vo_zoom/sample_virvi2vo_zoom.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_virvi2vo_zoom/sample_virvi2vo_zoom
endif
ifeq ($(TARGET), sample_CodecParallel)
SRCCS := \
    sample_CodecParallel/sample_CodecParallel.c \
    common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_CodecParallel/sample_CodecParallel
endif

ifeq ($(TARGET), sample_adec)
SRCCS := uncommonly_samples/sample_adec/sample_adec.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_adec/sample_adec
endif
ifeq ($(TARGET), sample_adec2ao)
SRCCS := uncommonly_samples/sample_adec2ao/sample_adec2ao.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_adec2ao/sample_adec2ao
endif
ifeq ($(TARGET), sample_aec)
SRCCS := sample_aec/sample_aec.c \
        common/sample_common_adec.c
LOCAL_TARGET_BIN := sample_aec/sample_aec
endif
ifeq ($(TARGET), sample_aenc)
SRCCS := sample_aenc/sample_aenc.c
LOCAL_TARGET_BIN := sample_aenc/sample_aenc
endif
ifeq ($(TARGET), sample_ai)
SRCCS := sample_ai/sample_ai.c
LOCAL_TARGET_BIN := sample_ai/sample_ai
endif
ifeq ($(TARGET), sample_ai2aenc)
SRCCS := sample_ai2aenc/sample_ai2aenc.c
LOCAL_TARGET_BIN := sample_ai2aenc/sample_ai2aenc
endif

ifeq ($(TARGET), sample_ai2aenc2muxer)
SRCCS := sample_ai2aenc2muxer/sample_ai2aenc2muxer.c
LOCAL_TARGET_BIN := sample_ai2aenc2muxer/sample_ai2aenc2muxer
endif
ifeq ($(TARGET), sample_ao)
SRCCS := sample_ao/sample_ao.c \
        common/sample_common_adec.c
LOCAL_TARGET_BIN := sample_ao/sample_ao
endif
ifeq ($(TARGET), sample_ao_resample_mixer)
SRCCS := uncommonly_samples/sample_ao_resample_mixer/sample_ao_resample_mixer.c \
        common/sample_common_adec.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_ao_resample_mixer/sample_ao_resample_mixer
endif
ifeq ($(TARGET), sample_ai2ao)
SRCCS := uncommonly_samples/sample_ai2ao/sample_ai2ao.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_ai2ao/sample_ai2ao
endif

ifeq ($(TARGET), sample_ao2ai_aec)
SRCCS := uncommonly_samples/sample_ao2ai_aec/sample_ao2ai.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_ao2ai_aec/sample_ao2ai_aec
endif
ifeq ($(TARGET), sample_ao2ai_aec_rate_mixer)
SRCCS := uncommonly_samples/sample_ao2ai_aec_rate_mixer/sample_ao2ai_aec_rate_mixer.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_ao2ai_aec_rate_mixer/sample_ao2ai_aec_rate_mixer
endif
ifeq ($(TARGET), sample_aoSync)
SRCCS := sample_aoSync/sample_aoSync.c \
        common/sample_common_adec.c
LOCAL_TARGET_BIN := sample_aoSync/sample_aoSync
endif
ifeq ($(TARGET), sample_avmuxer)
SRCCS := sample_avmuxer/sample_avmuxer.c \
        common/file_common.c \
        common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_avmuxer/sample_avmuxer
endif
ifeq ($(TARGET), sample_avplayer)
SRCCS := sample_avplayer/sample_avplayer.c \
        common/file_common.c
LOCAL_TARGET_BIN := sample_avplayer/sample_avplayer
endif
ifeq ($(TARGET), sample_demux)
SRCCS := sample_demux/sample_demux.c
LOCAL_TARGET_BIN := sample_demux/sample_demux
endif
ifeq ($(TARGET), sample_demux2adec)
SRCCS := sample_demux2adec/sample_demux2adec.c
LOCAL_TARGET_BIN := sample_demux2adec/sample_demux2adec
endif
ifeq ($(TARGET), sample_demux2adec2ao)
SRCCS := sample_demux2adec2ao/sample_demux2adec2ao.c
LOCAL_TARGET_BIN := sample_demux2adec2ao/sample_demux2adec2ao
endif

ifeq ($(TARGET), sample_demux2vdec)
SRCCS := sample_demux2vdec/sample_demux2vdec.c
LOCAL_TARGET_BIN := sample_demux2vdec/sample_demux2vdec
endif
ifeq ($(TARGET), sample_demux2vdec_saveFrame)
SRCCS := sample_demux2vdec_saveFrame/sample_demux2vdec_saveFrame.c \
    common/file_common.c
LOCAL_TARGET_BIN := sample_demux2vdec_saveFrame/sample_demux2vdec_saveFrame
endif
ifeq ($(TARGET), sample_demux2vdec2vo)
SRCCS := sample_demux2vdec2vo/sample_demux2vdec2vo.c
LOCAL_TARGET_BIN := sample_demux2vdec2vo/sample_demux2vdec2vo
endif
ifeq ($(TARGET), sample_directIORead)
SRCCS := uncommonly_samples/sample_directIORead/sample_directIORead.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_directIORead/sample_directIORead
endif
ifeq ($(TARGET), sample_driverVipp)
SRCCS := sample_driverVipp/sample_driverVipp.c
LOCAL_TARGET_BIN := sample_driverVipp/sample_driverVipp
endif

ifeq ($(TARGET), sample_file_repair)
SRCCS := sample_file_repair/sample_file_repair.c
LOCAL_TARGET_BIN := sample_file_repair/sample_file_repair
endif
ifeq ($(TARGET), sample_fish)
SRCCS := uncommonly_samples/sample_fish/sample_fish.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_fish/sample_fish
endif
ifeq ($(TARGET), sample_g2d)
SRCCS := \
    sample_g2d/sample_g2d.c \
    sample_g2d/sample_g2d_mem.c
LOCAL_TARGET_BIN := sample_g2d/sample_g2d
endif
ifeq ($(TARGET), sample_glog)
SRCCS := sample_glog/sample_glog.cpp
LOCAL_TARGET_BIN := sample_glog/sample_glog
endif
ifeq ($(TARGET), sample_hello)
SRCCS := uncommonly_samples/sample_hello/sample_hello.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_hello/sample_hello
endif

# librgb_ctrl, and librgb_ctrl depends on liblz4
# eyesee-mpp\system\public\rgb_ctrl
# eyesee-mpp\external\lz4-1.7.5
ifeq ($(TARGET), sample_isposd)
SRCCS := uncommonly_samples/sample_isposd/sample_isposd.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_isposd/sample_isposd
endif
ifeq ($(TARGET), sample_MotionDetect)
SRCCS := sample_MotionDetect/sample_MotionDetect.c
LOCAL_TARGET_BIN := sample_MotionDetect/sample_MotionDetect
endif
ifeq ($(TARGET), sample_motor)
SRCCS := \
    uncommonly_samples/sample_motor/sample_motor.c \
	uncommonly_samples/sample_motor/fan_control.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_motor/sample_motor
endif
ifeq ($(TARGET), sample_multi_vi2venc2muxer)
SRCCS := \
    sample_multi_vi2venc2muxer/sample_multi_vi2venc2muxer.c \
    common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_multi_vi2venc2muxer/sample_multi_vi2venc2muxer
endif
ifeq ($(TARGET), sample_muxer_multi_stream)
SRCCS := \
    sample_muxer_multi_stream/sample_muxer_multi_stream.c \
    common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_muxer_multi_stream/sample_muxer_multi_stream
endif
ifeq ($(TARGET), sample_PersonDetect)
SRCCS := sample_PersonDetect/sample_PersonDetect.c
LOCAL_TARGET_BIN := sample_PersonDetect/sample_PersonDetect
endif
ifeq ($(TARGET), sample_pthread_cancel)
SRCCS := uncommonly_samples/sample_pthread_cancel/sample_pthread_cancel.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_pthread_cancel/sample_pthread_cancel
endif
ifeq ($(TARGET), sample_region)
SRCCS := sample_region/sample_region.c
LOCAL_TARGET_BIN := sample_region/sample_region
endif
ifeq ($(TARGET), sample_RegionDetect)
SRCCS := \
    sample_RegionDetect/service.c \
    sample_RegionDetect/MotionDetect.c \
    sample_RegionDetect/MppHelper.c
LOCAL_TARGET_BIN := sample_RegionDetect/sample_RegionDetect
endif

ifeq ($(TARGET), sample_FaceTrack)
SRCCS := \
    sample_FaceTrack/sample_FaceTrack.c \
    sample_FaceTrack/MppHelper.c \
    sample_FaceTrack/uvc.c \
    sample_FaceTrack/parser_uvc_configfs.c

LOCAL_TARGET_BIN := sample_FaceTrack/sample_FaceTrack
endif

ifeq ($(TARGET), sample_IntrusionMonitor)
SRCCS := \
    sample_IntrusionMonitor/sample_IntrusionMonitor.c \
    sample_IntrusionMonitor/MppHelper.c \
    sample_IntrusionMonitor/uvc.c \
    sample_IntrusionMonitor/parser_uvc_configfs.c

LOCAL_TARGET_BIN := sample_IntrusionMonitor/sample_IntrusionMonitor
endif

# libTinyServer
ifeq ($(TARGET), sample_rtsp)
SRCCS := \
	sample_rtsp/sample_rtsp.c \
	common/rtsp_server.cpp \
	common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_rtsp/sample_rtsp
endif
ifeq ($(TARGET), sample_smartIPC_demo)
SRCCS := \
	sample_smartIPC_demo/sample_smartIPC_demo.c \
	common/rtsp_server.cpp \
	common/sdcard_manager.c \
	common/record.c \
	common/aiservice.c \
	common/aiservice_detect.c \
	common/aiservice_hw_scale.c \
	common/aiservice_mpp_helper.c \
	common/awaiisp_common.c \
	common/sample_common_venc.c \
	common/gtm_ldci_common.c \
	common/tdm_raw_process.c \
	common/sample_common_isp.c
LOCAL_TARGET_BIN := sample_smartIPC_demo/sample_smartIPC_demo
endif
ifeq ($(TARGET), sample_aisr)
SRCCS := \
	sample_aisr/sample_aisr.c \
	common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_aisr/sample_aisr
endif
ifeq ($(TARGET), sample_smartPreview_demo)
SRCCS := \
	uncommonly_samples/sample_smartPreview_demo/sample_smartPreview_demo.c \
	common/aiservice.c \
	common/aiservice_detect.c \
	common/aiservice_hw_scale.c \
	common/aiservice_mpp_helper.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_smartPreview_demo/sample_smartPreview_demo
endif
ifeq ($(TARGET), sample_select)
SRCCS := sample_select/sample_select.c
LOCAL_TARGET_BIN := sample_select/sample_select
endif
# libactivate.a or libtxzEngineUSB.so
ifeq ($(TARGET), sample_sound_controler)
SRCCS := uncommonly_samples/sample_sound_controler/sample_sound_controler.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_sound_controler/sample_sound_controler
endif
ifeq ($(TARGET), sample_timelapse)
SRCCS := sample_timelapse/sample_timelapse.c
LOCAL_TARGET_BIN := sample_timelapse/sample_timelapse
endif
# libssl, libcrypto
ifeq ($(TARGET), sample_twinchn_virvi2venc2ce)
SRCCS := sample_twinchn_virvi2venc2ce/sample_twinchn_virvi2venc2ce.c
LOCAL_TARGET_BIN := sample_twinchn_virvi2venc2ce/sample_twinchn_virvi2venc2ce
endif

ifeq ($(TARGET), sample_UILayer)
SRCCS := sample_UILayer/sample_UILayer.c
LOCAL_TARGET_BIN := sample_UILayer/sample_UILayer
endif
ifeq ($(TARGET), sample_uvc_vo)
SRCCS := sample_uvcin/sample_uvc_vo/sample_uvc_vo.c
LOCAL_TARGET_BIN := sample_uvcin/sample_uvc_vo/sample_uvc_vo
endif
ifeq ($(TARGET), sample_uvc2vdec_vo)
SRCCS := sample_uvcin/sample_uvc2vdec_vo/sample_uvc2vdec_vo.c
LOCAL_TARGET_BIN := sample_uvcin/sample_uvc2vdec_vo/sample_uvc2vdec_vo
endif
ifeq ($(TARGET), sample_uvc2vdenc2vo)
SRCCS := sample_uvcin/sample_uvc2vdenc2vo/sample_uvc2vdenc2vo.c
LOCAL_TARGET_BIN := sample_uvcin/sample_uvc2vdenc2vo/sample_uvc2vdenc2vo
endif
ifeq ($(TARGET), sample_uvc2vo)
SRCCS := sample_uvcin/sample_uvc2vo/sample_uvc2vo.c
LOCAL_TARGET_BIN := sample_uvcin/sample_uvc2vo/sample_uvc2vo
endif
ifeq ($(TARGET), sample_uvc_vcodec_vo_uac)
SRCCS := sample_uvcin/sample_uvc_vcodec_vo_uac/sample_uvc_vcodec_vo_uac.c
LOCAL_TARGET_BIN := sample_uvcin/sample_uvc_vcodec_vo_uac/sample_uvc_vcodec_vo_uac
endif
ifeq ($(TARGET), sample_uvc_vi_codec)
SRCCS := sample_uvcin/sample_uvc_vi_codec/sample_uvc_vi_codec.c \
    common/rtsp_server.cpp
LOCAL_TARGET_BIN := sample_uvcin/sample_uvc_vi_codec/sample_uvc_vi_codec
endif

ifeq ($(TARGET), sample_uac)
SRCCS := sample_uac/sample_uac.c
LOCAL_TARGET_BIN := sample_uac/sample_uac
endif
ifeq ($(TARGET), sample_uvcout)
SRCCS := sample_uvcout/sample_uvcout.c \
    sample_uvcout/utils/parser_uvc_configfs.c \
    sample_uvcout/uac/uac.c \
    common/awaiisp_common.c \
    common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_uvcout/sample_uvcout
endif
ifeq ($(TARGET), sample_usbcamera)
SRCCS := uncommonly_samples/sample_usbcamera/sample_usbcamera.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_usbcamera/sample_usbcamera
endif
ifeq ($(TARGET), sample_vdec)
SRCCS := sample_vdec/sample_vdec.c
LOCAL_TARGET_BIN := sample_vdec/sample_vdec
endif
ifeq ($(TARGET), sample_venc)
SRCCS := \
    sample_venc/sample_venc_mem.c \
    sample_venc/sample_venc.c
LOCAL_TARGET_BIN := sample_venc/sample_venc
endif
ifeq ($(TARGET), sample_venc2muxer)
SRCCS := uncommonly_samples/sample_venc2muxer/sample_venc2muxer.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_venc2muxer/sample_venc2muxer
endif
ifeq ($(TARGET), sample_vi_g2d)
SRCCS := uncommonly_samples/sample_vi_g2d/sample_vi_g2d.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_vi_g2d/sample_vi_g2d
endif

ifeq ($(TARGET), sample_vi_reset)
SRCCS := uncommonly_samples/sample_vi_reset/sample_vi_reset.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_vi_reset/sample_vi_reset
endif
ifeq ($(TARGET), sample_vin_isp_test)
SRCCS := uncommonly_samples/sample_vin_isp_test/sample_vin_isp_test.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_vin_isp_test/sample_vin_isp_test
endif
ifeq ($(TARGET), sample_virvi)
SRCCS := sample_virvi/sample_virvi.c \
	common/sample_common_isp.c
LOCAL_TARGET_BIN := sample_virvi/sample_virvi
endif
ifeq ($(TARGET), sample_virvi2eis2venc)
SRCCS := uncommonly_samples/sample_virvi2eis2venc/sample_virvi2eis2venc.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_virvi2eis2venc/sample_virvi2eis2venc
endif
ifeq ($(TARGET), sample_virvi2fish2venc)
SRCCS := uncommonly_samples/sample_virvi2fish2venc/sample_virvi2fish2venc.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_virvi2fish2venc/sample_virvi2fish2venc
endif

ifeq ($(TARGET), sample_virvi2fish2vo)
SRCCS := uncommonly_samples/sample_virvi2fish2vo/sample_virvi2fish2vo.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_virvi2fish2vo/sample_virvi2fish2vo
endif
ifeq ($(TARGET), sample_virvi2venc)
SRCCS := \
	sample_virvi2venc/sample_virvi2venc.c \
	common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_virvi2venc/sample_virvi2venc
endif
ifeq ($(TARGET), sample_virvi2vencSync)
SRCCS := \
	uncommonly_samples/sample_virvi2vencSync/sample_virvi2vencSync.c \
	common/sample_common_venc.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_virvi2vencSync/sample_virvi2vencSync
endif
# libssl, libcrypto
ifeq ($(TARGET), sample_virvi2venc2ce)
SRCCS := sample_virvi2venc2ce/sample_virvi2venc2ce.c
LOCAL_TARGET_BIN := sample_virvi2venc2ce/sample_virvi2venc2ce
endif
ifeq ($(TARGET), sample_virvi2venc2muxer)
SRCCS := \
	sample_virvi2venc2muxer/sample_vi2venc2muxer.c \
	common/file_common.c \
	common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_virvi2venc2muxer/sample_vi2venc2muxer
endif
ifeq ($(TARGET), sample_virvi2vo)
SRCCS := sample_virvi2vo/sample_virvi2vo.c \
	common/sample_common_isp.c
LOCAL_TARGET_BIN := sample_virvi2vo/sample_virvi2vo
endif
ifeq ($(TARGET), sample_odet_demo)
SRCCS := uncommonly_samples/sample_odet_demo/sample_odet_demo.c
SRCCS += uncommonly_samples/sample_odet_demo/yolov3-tiny_nbg_viplite/main.c
SRCCS += uncommonly_samples/sample_odet_demo/yolov3-tiny_nbg_viplite/vnn_pre_process.c
SRCCS += uncommonly_samples/sample_odet_demo/yolov3-tiny_nbg_viplite/vnn_post_process.c
SRCCS += uncommonly_samples/sample_odet_demo/post/box.cpp
SRCCS += uncommonly_samples/sample_odet_demo/post/post_process.cpp
SRCCS += uncommonly_samples/sample_odet_demo/post/yolo_layer.cpp
LOCAL_TARGET_BIN := uncommonly_samples/sample_odet_demo/sample_odet_demo
endif

ifeq ($(TARGET), sample_facekit_demo)
SRCCS := sample_facekit_demo/sample_facekit_demo.c
SRCCS += sample_facekit_demo/ai_facekit.c
LOCAL_TARGET_BIN := sample_facekit_demo/sample_facekit_demo
endif

ifeq ($(TARGET), sample_vo)
SRCCS := sample_vo/sample_vo.c
LOCAL_TARGET_BIN := sample_vo/sample_vo
endif
ifeq ($(TARGET), sample_recorder)
SRCCS := \
	sample_recorder/sample_recorder.c \
	common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_recorder/sample_recorder
endif
ifeq ($(TARGET), sample_CDR_demo)
SRCCS := \
    sample_CDR_demo/sample_CDR_demo.c \
    common/rtsp_server.cpp \
    common/sample_common_venc.c
LOCAL_TARGET_BIN := sample_CDR_demo/sample_CDR_demo
endif
ifeq ($(TARGET), sample_WebRtcAec)
SRCCS := uncommonly_samples/sample_WebRtcAec/sample_WebRtcAec.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_WebRtcAec/sample_WebRtcAec
endif
ifeq ($(TARGET), sample_UvoiceAec)
SRCCS := uncommonly_samples/sample_UvoiceAec/sample_UvoiceAec.c
LOCAL_TARGET_BIN := uncommonly_samples/sample_UvoiceAec/sample_UvoiceAec
endif
ifeq ($(TARGET), sample_VideoResizer)
SRCCS := sample_VideoResizer/sample_VideoResizer.c
LOCAL_TARGET_BIN := sample_VideoResizer/sample_VideoResizer
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
    libz \
    liblog \
    libawion \
    libmedia_mpp \
    libmpp_component \
    libmedia_utils

ifeq ($(MPP_VENC_SUPPORT_VENC_PARAM_DEBUG),Y)
LOCAL_SHARED_LIBS += \
    libexpat
endif

ifeq ($(MPPCFG_SAMPLE_CONFIGFILEPARSER),Y)
LOCAL_SHARED_LIBS += \
    libsample_confparser
endif
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
ifneq ($(filter Y, $(MPPCFG_VENC) $(MPPCFG_VDEC)),)
LOCAL_SHARED_LIBS += \
    libMemAdapter \
    libVE \
    libcdc_base
endif
ifeq ($(MPPCFG_VENC),Y)
LOCAL_SHARED_LIBS += \
    libvencoder
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
ifneq (,$(filter $(TARGET),sample_rtsp sample_smartIPC_demo sample_uvc_vi_codec sample_CDR_demo))
LOCAL_SHARED_LIBS += \
    libTinyServer
endif
ifeq ($(TARGET), sample_isposd)
LOCAL_SHARED_LIBS += \
	librgb_ctrl \
	liblz4
endif
ifeq ($(TARGET), sample_sound_controler)
LOCAL_SHARED_LIBS += \
	libtxzEngineUSB
endif
ifneq (,$(filter $(TARGET),sample_twinchn_virvi2venc2ce sample_virvi2venc2ce))
LOCAL_SHARED_LIBS += \
	libssl
endif
ifneq (,$(filter $(TARGET),sample_twinchn_virvi2venc2ce sample_virvi2venc2ce sample_aisr))
LOCAL_SHARED_LIBS += \
	libcrypto
endif
ifeq ($(TARGET), sample_file_repair)
LOCAL_SHARED_LIBS += \
    libfilerepair
endif

ifneq (,$(filter $(PRODUCT), v837s))
	ifneq (,$(filter $(TARGET),sample_RegionDetect sample_PersonDetect sample_smartIPC_demo sample_smartPreview_demo sample_uvcout sample_aisr))
	LOCAL_SHARED_LIBS += \
		libawnn_full
	endif
	ifneq (,$(filter $(TARGET),sample_smartIPC_demo sample_uvcout))
	LOCAL_SHARED_LIBS += \
		libawaiisp \
		libawion
	endif
	ifneq (,$(filter $(TARGET),sample_aisr))
	LOCAL_SHARED_LIBS += \
		libawaisr
	endif
else
	ifneq (,$(filter $(TARGET),sample_odet_demo sample_facekit_demo sample_FaceTrack sample_RegionDetect sample_PersonDetect sample_smartIPC_demo sample_smartPreview_demo sample_uvcout sample_aisr))
	LOCAL_SHARED_LIBS += \
		libVIPlite \
		libVIPuser
	endif
	ifneq (,$(filter $(TARGET),sample_RegionDetect sample_FaceTrack sample_PersonDetect sample_smartIPC_demo sample_smartPreview_demo))
	LOCAL_SHARED_LIBS += \
		libawnn
	endif
	ifneq (,$(filter $(TARGET),sample_smartIPC_demo sample_uvcout))
	LOCAL_SHARED_LIBS += \
		libawaiisp \
		libawion
	endif
	ifneq (,$(filter $(TARGET),sample_facekit_demo ))
	LOCAL_SHARED_LIBS += \
		libpix_facekit
	endif
	ifneq (,$(filter $(TARGET),sample_aisr))
	LOCAL_SHARED_LIBS += \
		libawaisr
	endif
	ifneq (,$(filter $(TARGET),sample_FaceTrack))
	LOCAL_SHARED_LIBS += \
		libaw_objectTracker \
		libawf_det \
		libaw_utilities_standalone \
		librgb_ctrl \
		liblz4
	endif
	ifneq (,$(filter $(TARGET),sample_IntrusionMonitor))
	LOCAL_STATIC_LIBS += \
		libpdet \
		libVIPlite \
		libVIPuser \
		librgb_ctrl \
		liblz4
	endif
endif

LOCAL_SHARED_LIBS += \
    libasound

else
######################## static lib mode ########################
# These only provide dynamic libraries
LOCAL_SHARED_LIBS += \
    libasound \
    liblog

# Public static library
LOCAL_STATIC_LIBS += \
    libz \
    libawion \
    libaw_mpp \
    libmedia_utils

ifeq ($(MPP_VENC_SUPPORT_VENC_PARAM_DEBUG),Y)
LOCAL_STATIC_LIBS += \
    libexpat
endif

ifneq ($(filter Y, $(MPPCFG_DEMUXER) $(MPPCFG_SAMPLE_CONFIGFILEPARSER)),)
LOCAL_STATIC_LIBS += \
    libPluginMpp \
    libIniParserMpp \
    libcdx_base
endif
ifeq ($(MPPCFG_AIO),Y)
LOCAL_STATIC_LIBS += \
    libResample \
    libAudioVps
endif
ifeq ($(MPPCFG_SAMPLE_CONFIGFILEPARSER),Y)
LOCAL_STATIC_LIBS += \
    libsample_confparser
endif
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
  ifeq ($(MPPCFG_ANS_LIB),libwebrtc)
    LOCAL_STATIC_LIBS += libAns
  else ifeq ($(MPPCFG_ANS_LIB),liblstm)
    LOCAL_STATIC_LIBS += libAns
  else ifeq ($(MPPCFG_ANS_LIB),libnosc)
    LOCAL_STATIC_LIBS += libAns
  endif
endif
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

ifneq (,$(filter $(PRODUCT), v837s))
	ifneq (,$(filter $(TARGET),sample_RegionDetect sample_PersonDetect sample_smartIPC_demo sample_smartPreview_demo sample_uvcout sample_aisr))
	LOCAL_STATIC_LIBS += \
		libawnn_full
	endif
	ifneq (,$(filter $(TARGET),sample_smartIPC_demo sample_uvcout))
	LOCAL_STATIC_LIBS += \
		libawaiisp \
		libawion
	endif
	ifneq (,$(filter $(TARGET),sample_aisr))
	LOCAL_STATIC_LIBS += \
        libawaisr
    endif
else
	ifneq (,$(filter $(TARGET),sample_odet_demo sample_facekit_demo sample_RegionDetect sample_FaceTrack sample_PersonDetect sample_smartIPC_demo sample_smartPreview_demo sample_uvcout sample_aisr))
	LOCAL_SHARED_LIBS += \
		libVIPlite \
		libVIPuser
	endif
	ifneq (,$(filter $(TARGET),sample_RegionDetect sample_FaceTrack sample_PersonDetect sample_smartIPC_demo sample_smartPreview_demo))
	LOCAL_STATIC_LIBS += \
		libawnn
	endif
	ifneq (,$(filter $(TARGET),sample_smartIPC_demo sample_uvcout))
	LOCAL_STATIC_LIBS += \
		libawaiisp \
		libawion
	endif
	ifneq (,$(filter $(TARGET),sample_facekit_demo))
	LOCAL_STATIC_LIBS += \
		libpix_facekit
	endif
	ifneq (,$(filter $(TARGET),sample_aisr))
	LOCAL_STATIC_LIBS += \
        libawaisr
    	endif
	ifneq (,$(filter $(TARGET),sample_FaceTrack))
	LOCAL_STATIC_LIBS += \
		libaw_objectTracker \
		libawf_det \
		libaw_utilities_standalone \
		librgb_ctrl \
		liblz4
	endif
	ifneq (,$(filter $(TARGET),sample_IntrusionMonitor))
	LOCAL_STATIC_LIBS += \
		libpdet \
		libVIPlite \
		libVIPuser \
		librgb_ctrl \
		liblz4
	endif
endif

ifneq (,$(filter $(TARGET),sample_rtsp sample_smartIPC_demo sample_uvc_vi_codec sample_CDR_demo))
LOCAL_STATIC_LIBS += \
    libTinyServer
endif

ifeq ($(TARGET), sample_isposd)
LOCAL_STATIC_LIBS += \
	librgb_ctrl \
	liblz4
endif
ifeq ($(TARGET), sample_sound_controler)
LOCAL_STATIC_LIBS += \
	libactivate
endif
ifneq (,$(filter $(TARGET),sample_twinchn_virvi2venc2ce sample_virvi2venc2ce))
LOCAL_STATIC_LIBS += \
	libssl
endif
ifneq (,$(filter $(TARGET),sample_twinchn_virvi2venc2ce sample_virvi2venc2ce sample_aisr))
LOCAL_STATIC_LIBS += \
	libcrypto
endif
ifeq ($(TARGET), sample_file_repair)
LOCAL_STATIC_LIBS += \
    libfilerepair
endif

endif

#LOCAL_SHARED_LIBS += libcurl
#LOCAL_SHARED_LIBS += libunwind

#LOCAL_STATIC_LIBS += libleaktracer

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
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
    $(STAGING_DIR)/usr/include/viplite-driver \
    $(STAGING_DIR)/usr/include/object_tracker \
    $(STAGING_DIR)/usr/include/face_detect \
    $(STAGING_DIR)/usr/include/person_detect \
    $(STAGING_DIR)/usr/include/libawaiisp \
	$(STAGING_DIR)/usr/include/libawaisr \
    $(PACKAGE_TOP)/include/utils \
    $(PACKAGE_TOP)/include/media \
    $(PACKAGE_TOP)/include/media/utils \
    $(PACKAGE_TOP)/include \
    $(PACKAGE_TOP)/config \
    $(PACKAGE_TOP)/media/include \
    $(PACKAGE_TOP)/media/include/utils \
    $(PACKAGE_TOP)/media/include/audio \
    $(PACKAGE_TOP)/media/include/component \
    $(PACKAGE_TOP)/media/LIBRARY/libISE \
    $(PACKAGE_TOP)/media/LIBRARY/libISE/include \
    $(PACKAGE_TOP)/media/LIBRARY/libisp \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include/device \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/include/V4l2Camera \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_tuning \
    $(PACKAGE_TOP)/media/LIBRARY/libAIE_Vda/include \
    $(PACKAGE_TOP)/media/LIBRARY/include_ai_common \
    $(PACKAGE_TOP)/media/LIBRARY/include_eve_common \
    $(PACKAGE_TOP)/media/LIBRARY/libeveface/include \
    $(PACKAGE_TOP)/media/LIBRARY/include_stream \
    $(PACKAGE_TOP)/media/LIBRARY/include_FsWriter \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/include \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/osal \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/encoding \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/encoding/include \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/decoding/include \
    $(PACKAGE_TOP)/media/LIBRARY/aec_lib/include \
    $(PACKAGE_TOP)/media/LIBRARY/agc_lib/include \
    $(PACKAGE_TOP)/media/LIBRARY/include_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libfilerepair/include \
    $(PACKAGE_TOP)/media/LIBRARY/libIniParser \
    $(PACKAGE_TOP)/sample/configfileparser \
    $(PACKAGE_TOP)/sample/common \
    $(LINUX_USER_HEADERS)/include

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
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
    $(PACKAGE_TOP)/sample/configfileparser \
    $(PACKAGE_TOP)/media/utils \
    $(PACKAGE_TOP)/media \
    $(PACKAGE_TOP)/media/component \
    $(PACKAGE_TOP)/media/LIBRARY/libstream \
    $(PACKAGE_TOP)/media/LIBRARY/libIniParser \
    $(PACKAGE_TOP)/media/LIBRARY/libPlugin/plugin \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/base \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/common \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/aac \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/id3v2 \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/mov \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/mp3 \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/mpg \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/ts \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/wav \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/amr \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/ape \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/flac \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/ogg \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/parser/base \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/stream/base \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/stream/file \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/stream/http \
    $(PACKAGE_TOP)/media/LIBRARY/libdemuxer/libcore/stream/tcp \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/memory \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/library/out \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/ve \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec/common \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec/h264 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec/h265 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec/jpeg \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine/h264 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine/h265 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpeg \
    $(PACKAGE_TOP)/media/LIBRARY/libdemux \
    $(PACKAGE_TOP)/media/LIBRARY/libfilerepair \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/muxers \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/mp4_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/raw_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/mpeg2ts_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/aac_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/mp3_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/wav_muxer \
    $(PACKAGE_TOP)/media/LIBRARY/libmuxer/common/libavutil \
    $(PACKAGE_TOP)/media/LIBRARY/libFsWriter \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/decoding \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/midware/encoding \
    $(PACKAGE_TOP)/media/LIBRARY/AudioLib/lib/out \
    $(PACKAGE_TOP)/media/LIBRARY/audioEffectLib/lib \
    $(PACKAGE_TOP)/media/LIBRARY/textEncLib \
    $(PACKAGE_TOP)/media/LIBRARY/aec_lib/out \
    $(PACKAGE_TOP)/media/LIBRARY/drc_lib/out \
    $(PACKAGE_TOP)/media/LIBRARY/agc_lib/out \
    $(PACKAGE_TOP)/media/LIBRARY/ans_lib/out \
    $(PACKAGE_TOP)/media/LIBRARY/libResample \
    $(PACKAGE_TOP)/media/LIBRARY/libAudioVps/out \
    $(PACKAGE_TOP)/media/LIBRARY/libisp \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/out/out \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_cfg \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/isp_dev \
    $(PACKAGE_TOP)/media/LIBRARY/libisp/iniparser \
    $(PACKAGE_TOP)/media/LIBRARY/libISE/out \
    $(PACKAGE_TOP)/media/LIBRARY/libVideoStabilization \
    $(PACKAGE_TOP)/media/LIBRARY/libVideoStabilization/out \
    $(PACKAGE_TOP)/media/librender

ifneq (,$(filter $(TARGET),sample_odet_demo sample_PersonDetect sample_RegionDetect sample_FaceTrack sample_smartIPC_demo sample_smartPreview_demo sample_facekit_demo sample_aisr))
LIB_SEARCH_PATHS += $(STAGING_DIR)/usr/lib
endif


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
