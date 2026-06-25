##############################################
# OpenWrt Makefile for eyesee-mpp-middleware
#
#
# Most of the variables used here are defined in
# the include directives below. We just need to
# specify a basic description of the package,
# where to build our program, where to find
# the source files, and where to install the
# compiled program on the router.
#
# Be very careful of spacing in this file.
# Indents should be tabs, not spaces, and
# there should be no trailing whitespace in
# lines that are not commented.
#
##############################################
include $(TOPDIR)/rules.mk
include $(BUILD_DIR)/kernel.mk

# Name and release number of this package
PKG_NAME:=eyesee-mpp-middleware
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_MAINTAINER:=AW-PDC-PD5
# This specifies the directory where we're going to build the program.
# The root build directory, $(BUILD_DIR), is by default the build_mipsel
# directory in your OpenWrt SDK directory
PKG_BUILD_DIR := $(COMPILE_DIR)/$(PKG_NAME)
SRC_COMPILE_DIR := $(PKG_BUILD_DIR)/$(PKG_NAME)
SRC_CODE_DIR := $(TOPDIR)/external/eyesee-mpp/middleware
ifeq ($(TARGET_CHIP), sun8iw12p1)
    SRC_CODE_DIR := $(SRC_CODE_DIR)/v5
else ifeq ($(TARGET_CHIP), sun8iw16p1)
    SRC_CODE_DIR := $(SRC_CODE_DIR)/v316
else ifeq ($(TARGET_CHIP), sun8iw19p1)
    SRC_CODE_DIR := $(SRC_CODE_DIR)/sun8iw19p1
else ifeq ($(TARGET_CHIP), sun8iw21p1)
    SRC_CODE_DIR := $(SRC_CODE_DIR)/sun8iw21
else
    $(error fatal error! $(TARGET_CHIP) is not support by eyesee-mpp!)
endif

# This specifies the directory where file depends. when files in it update, increasing compile will be enable.
PKG_FILE_DEPENDS:= $(SRC_CODE_DIR)
PKG_BUILD_PARALLEL:=1

include $(BUILD_DIR)/package.mk

export TARGET_BOARD_PLATFORM
export LICHEE_PLATFORM_OS:=tina

ifeq ($(CONFIG_mpp_support_fastboot),y)
    MPPCFG_SUPPORT_FASTBOOT := Y
else
    MPPCFG_SUPPORT_FASTBOOT := N
endif
export MPPCFG_SUPPORT_FASTBOOT

ifeq ($(CONFIG_mpp_support_fastboot_tdm_offline_handle_raw),y)
    MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW := Y
else
    MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW := N
endif
export MPPCFG_SUPPORT_FASTBOOT_TDM_OFFLINE_HANDLE_RAW

ifeq ($(CONFIG_mpp_enable_aiisp),y)
    MPPCFG_ENABLE_AIISP := Y
else
    MPPCFG_ENABLE_AIISP := N
endif
export MPPCFG_ENABLE_AIISP

ifeq ($(CONFIG_mpp_sample_aisr),y)
    MPPCFG_SAMPLE_AISR := Y
else
    MPPCFG_SAMPLE_AISR := N
endif
export MPPCFG_SAMPLE_AISR

SENSOR_NAME:=
ifeq ($(CONFIG_imx386),y)
    SENSOR_NAME+=imx386
endif
ifeq ($(CONFIG_gc4663),y)
    SENSOR_NAME+=gc4663
endif
ifeq ($(CONFIG_gc1084),y)
    SENSOR_NAME+=gc1084
    ifeq ($(CONFIG_gc1084_8bit),y)
        SENSOR_NAME+=gc1084_8bit
    endif
endif
ifeq ($(CONFIG_gc2053),y)
    SENSOR_NAME+=gc2053
    ifeq ($(CONFIG_gc2053_8bit),y)
        SENSOR_NAME+=gc2053_8bit
    endif
endif
ifeq ($(CONFIG_gc0406),y)
    SENSOR_NAME+=gc0406
endif
ifeq ($(CONFIG_gc2083),y)
    SENSOR_NAME+=gc2083
endif
ifeq ($(CONFIG_f37p),y)
    SENSOR_NAME+=f37p
endif
ifeq ($(CONFIG_f355p),y)
    SENSOR_NAME+=f355p
endif
ifeq ($(CONFIG_mis2008),y)
    SENSOR_NAME+=mis2008
endif
ifeq ($(CONFIG_sc1346),y)
    SENSOR_NAME+=sc1346
endif
ifeq ($(CONFIG_sc2336),y)
    SENSOR_NAME+=sc2336
endif
ifeq ($(CONFIG_sc2355),y)
    SENSOR_NAME+=sc2355
endif
ifeq ($(CONFIG_sc3336),y)
    SENSOR_NAME+=sc3336
endif
ifeq ($(CONFIG_sc3336p),y)
    SENSOR_NAME+=sc3336p
endif
ifeq ($(CONFIG_sc4336),y)
    SENSOR_NAME+=sc4336
endif
ifeq ($(CONFIG_sc4336p),y)
    SENSOR_NAME+=sc4336p
endif
ifeq ($(CONFIG_sc5336),y)
    SENSOR_NAME+=sc5336
endif
ifeq ($(CONFIG_sc530ai),y)
    SENSOR_NAME+=sc530ai
endif
ifeq ($(CONFIG_sc200ai),y)
    SENSOR_NAME+=sc200ai
    ifeq ($(CONFIG_sc200ai_8bit),y)
        SENSOR_NAME+=sc200ai_8bit
    endif
endif
ifeq ($(CONFIG_os02g10),y)
    SENSOR_NAME+=os02g10
endif
export SENSOR_NAME

############################################################################
#below items are used for mpp configuration of previous EyeseeLinux,
#for smooth migration to tina, we reserve these config items, define them here and export.
#ref to mpp_config.mk of EyeseeLinux.
#[config item], valid in makefile.
#config chip version: AW1721
ifeq ($(TARGET_CHIP), sun8iw12p1)
    MPPCFG_CHIP := AW1721
else ifeq ($(TARGET_CHIP), sun8iw16p1)
    MPPCFG_CHIP := AW1816
else ifeq ($(TARGET_CHIP), sun8iw19p1)
    MPPCFG_CHIP := AW1817
else ifeq ($(TARGET_CHIP), sun8iw21p1)
    MPPCFG_CHIP := AW1886
else
    $(error fatal error! $(TARGET_CHIP) is not support by eyesee-mpp!)
endif
export MPPCFG_CHIP
#config log level for compile
ifeq ($(CONFIG_mpp_log_level_close),y)
    MPPCFG_LOG_LEVEL := CLOSE
else ifeq ($(CONFIG_mpp_log_level_error),y)
    MPPCFG_LOG_LEVEL := ERROR
else ifeq ($(CONFIG_mpp_log_level_warn),y)
    MPPCFG_LOG_LEVEL := WARN
else ifeq ($(CONFIG_mpp_log_level_debug),y)
    MPPCFG_LOG_LEVEL := DEBUG
else ifeq ($(CONFIG_mpp_log_level_verbose),y)
    MPPCFG_LOG_LEVEL := VERBOSE
else
    MPPCFG_LOG_LEVEL := DEBUG
endif
export MPPCFG_LOG_LEVEL
#config write file method: LINUX_VFS, DIRECT_IO
ifeq ($(CONFIG_mpp_fwrite_method_vfs),y)
    MPPCFG_FILE_SYSTEM := LINUX_VFS
else ifeq ($(CONFIG_mpp_fwrite_method_directIO),y)
    MPPCFG_FILE_SYSTEM := DIRECT_IO
else
    MPPCFG_FILE_SYSTEM := LINUX_VFS
endif
export MPPCFG_FILE_SYSTEM
#config if use hw display method
ifeq ($(CONFIG_mpp_hw_display),y)
    MPPCFG_HW_DISPLAY := Y
else
    MPPCFG_HW_DISPLAY := N
endif
export MPPCFG_HW_DISPLAY
#config if enable mpp_vi
ifeq ($(CONFIG_mpp_vi),y)
    MPPCFG_VI := Y
else
    MPPCFG_VI := N
endif
export MPPCFG_VI
#config if enable mpp_isp_awTuningApp_use_libcedarc
ifeq ($(CONFIG_mpp_isp_awTuningApp_use_libcedarc),y)
    MPPCFG_ISP_AWTUNINGAPP_USE_LIBCEDARC := Y
else
    MPPCFG_ISP_AWTUNINGAPP_USE_LIBCEDARC := N
endif
export MPPCFG_ISP_AWTUNINGAPP_USE_LIBCEDARC
#config if enable mpp_vo
ifeq ($(CONFIG_mpp_vo),y)
    MPPCFG_VO := Y
else
    MPPCFG_VO := N
endif
export MPPCFG_VO
#config if enable mpp_textEnc
ifeq ($(CONFIG_mpp_textenc),y)
    MPPCFG_TEXTENC := Y
else
    MPPCFG_TEXTENC := N
endif
export MPPCFG_TEXTENC
#config if enable mpp_venc
ifeq ($(CONFIG_mpp_venc),y)
    MPPCFG_VENC := Y
else
    MPPCFG_VENC := N
endif
export MPPCFG_VENC
#config if enable mpp_venc_h264
ifeq ($(CONFIG_mpp_venc_h264),y)
    MPPCFG_VENC_H264 := Y
else
    MPPCFG_VENC_H264 := N
endif
export MPPCFG_VENC_H264
#config if enable mpp_venc_h265
ifeq ($(CONFIG_mpp_venc_h265),y)
    MPPCFG_VENC_H265 := Y
else
    MPPCFG_VENC_H265 := N
endif
export MPPCFG_VENC_H265
#config if enable mpp_venc_jpeg
ifeq ($(CONFIG_mpp_venc_jpeg),y)
    MPPCFG_VENC_JPEG := Y
else
    MPPCFG_VENC_JPEG := N
endif
export MPPCFG_VENC_JPEG
#config if enable mpp_venc_support_venc_param_debug
ifeq ($(CONFIG_mpp_venc_support_venc_param_debug),y)
    MPP_VENC_SUPPORT_VENC_PARAM_DEBUG := Y
else
    MPP_VENC_SUPPORT_VENC_PARAM_DEBUG := N
endif
export MPP_VENC_SUPPORT_VENC_PARAM_DEBUG
#config if enable mpp_vdec
ifeq ($(CONFIG_mpp_vdec),y)
    MPPCFG_VDEC := Y
else
    MPPCFG_VDEC := N
endif
export MPPCFG_VDEC
#config if enable mpp_vdec_h264
ifeq ($(CONFIG_mpp_vdec_h264),y)
    MPPCFG_VDEC_H264 := Y
else
    MPPCFG_VDEC_H264 := N
endif
export MPPCFG_VDEC_H264
#config if enable mpp_vdec_h265
ifeq ($(CONFIG_mpp_vdec_h265),y)
    MPPCFG_VDEC_H265 := Y
else
    MPPCFG_VDEC_H265 := N
endif
export MPPCFG_VDEC_H265
#config if enable mpp_vdec_jpeg
ifeq ($(CONFIG_mpp_vdec_jpeg),y)
    MPPCFG_VDEC_JPEG := Y
else
    MPPCFG_VDEC_JPEG := N
endif
export MPPCFG_VDEC_JPEG
#config if enable mpp_aio
ifeq ($(CONFIG_mpp_aio),y)
    MPPCFG_AIO := Y
else
    MPPCFG_AIO := N
endif
export MPPCFG_AIO
#config if enable mpp_aenc
ifeq ($(CONFIG_mpp_aenc),y)
    MPPCFG_AENC := Y
else
    MPPCFG_AENC := N
endif
export MPPCFG_AENC
ifeq ($(CONFIG_mpp_aenc_pcm),y)
    MPPCFG_AENC_PCM := Y
else
    MPPCFG_AENC_PCM := N
endif
export MPPCFG_AENC_PCM
ifeq ($(CONFIG_mpp_aenc_g711),y)
    MPPCFG_AENC_G711 := Y
else
    MPPCFG_AENC_G711 := N
endif
export MPPCFG_AENC_G711
ifeq ($(CONFIG_mpp_aenc_g726),y)
    MPPCFG_AENC_G726 := Y
else
    MPPCFG_AENC_G726 := N
endif
export MPPCFG_AENC_G726
ifeq ($(CONFIG_mpp_aenc_aac),y)
    MPPCFG_AENC_AAC := Y
else
    MPPCFG_AENC_AAC := N
endif
export MPPCFG_AENC_AAC
ifeq ($(CONFIG_mpp_aenc_mp3),y)
    MPPCFG_AENC_MP3 := Y
else
    MPPCFG_AENC_MP3 := N
endif
export MPPCFG_AENC_MP3
ifeq ($(CONFIG_mpp_aenc_opus),y)
    MPPCFG_AENC_OPUS := Y
else
    MPPCFG_AENC_OPUS := N
endif
export MPPCFG_AENC_OPUS
#config if enable mpp_adec
ifeq ($(CONFIG_mpp_adec),y)
    MPPCFG_ADEC := Y
else
    MPPCFG_ADEC := N
endif
export MPPCFG_ADEC
ifeq ($(CONFIG_mpp_adec_wav),y)
    MPPCFG_ADEC_WAV := Y
else
    MPPCFG_ADEC_WAV := N
endif
export MPPCFG_ADEC_WAV
ifeq ($(CONFIG_mpp_adec_g726),y)
    MPPCFG_ADEC_G726 := Y
else
    MPPCFG_ADEC_G726 := N
endif
export MPPCFG_ADEC_G726
ifeq ($(CONFIG_mpp_adec_aac),y)
    MPPCFG_ADEC_AAC := Y
else
    MPPCFG_ADEC_AAC := N
endif
export MPPCFG_ADEC_AAC
ifeq ($(CONFIG_mpp_adec_mp3),y)
    MPPCFG_ADEC_MP3 := Y
else
    MPPCFG_ADEC_MP3 := N
endif
export MPPCFG_ADEC_MP3
ifeq ($(CONFIG_mpp_adec_amr),y)
    MPPCFG_ADEC_AMR := Y
else
    MPPCFG_ADEC_AMR := N
endif
export MPPCFG_ADEC_AMR
ifeq ($(CONFIG_mpp_adec_ape),y)
    MPPCFG_ADEC_APE := Y
else
    MPPCFG_ADEC_APE := N
endif
export MPPCFG_ADEC_APE
ifeq ($(CONFIG_mpp_adec_flac),y)
    MPPCFG_ADEC_FLAC := Y
else
    MPPCFG_ADEC_FLAC := N
endif
export MPPCFG_ADEC_FLAC
ifeq ($(CONFIG_mpp_adec_ogg),y)
    MPPCFG_ADEC_OGG := Y
else
    MPPCFG_ADEC_OGG := N
endif
export MPPCFG_ADEC_OGG
ifeq ($(CONFIG_mpp_adec_opus),y)
    MPPCFG_ADEC_OPUS := Y
else
    MPPCFG_ADEC_OPUS := N
endif
export MPPCFG_ADEC_OPUS
#config if enable mpp_muxer
ifeq ($(CONFIG_mpp_muxer),y)
    MPPCFG_MUXER := Y
else
    MPPCFG_MUXER := N
endif
export MPPCFG_MUXER
#config gps data pack method
ifeq ($(CONFIG_mpp_gps_pack_in_track),y)
    MPPCFG_GPS_PACK_IN_TRACK := Y
else
    MPPCFG_GPS_PACK_IN_TRACK := N
endif
export MPPCFG_GPS_PACK_IN_TRACK
ifeq ($(CONFIG_mpp_gps_pack_in_mdat),y)
    MPPCFG_GPS_PACK_IN_MDAT := Y
else
    MPPCFG_GPS_PACK_IN_MDAT := N
endif
export MPPCFG_GPS_PACK_IN_MDAT
#config if enable mpp_demuxer
ifeq ($(CONFIG_mpp_demuxer),y)
    MPPCFG_DEMUXER := Y
else
    MPPCFG_DEMUXER := N
endif
export MPPCFG_DEMUXER
ifeq ($(CONFIG_mpp_parser_aac),y)
    MPPCFG_PARSER_AAC := Y
else
    MPPCFG_PARSER_AAC := N
endif
export MPPCFG_PARSER_AAC
ifeq ($(CONFIG_mpp_parser_amr),y)
    MPPCFG_PARSER_AMR := Y
else
    MPPCFG_PARSER_AMR := N
endif
export MPPCFG_PARSER_AMR
ifeq ($(CONFIG_mpp_parser_ape),y)
    MPPCFG_PARSER_APE := Y
else
    MPPCFG_PARSER_APE := N
endif
export MPPCFG_PARSER_APE
ifeq ($(CONFIG_mpp_parser_flac),y)
    MPPCFG_PARSER_FLAC := Y
else
    MPPCFG_PARSER_FLAC := N
endif
export MPPCFG_PARSER_FLAC
ifeq ($(CONFIG_mpp_parser_id3v2),y)
    MPPCFG_PARSER_ID3V2 := Y
else
    MPPCFG_PARSER_ID3V2 := N
endif
export MPPCFG_PARSER_ID3V2
ifeq ($(CONFIG_mpp_parser_mov),y)
    MPPCFG_PARSER_MOV := Y
else
    MPPCFG_PARSER_MOV := N
endif
export MPPCFG_PARSER_MOV
ifeq ($(CONFIG_mpp_parser_mp3),y)
    MPPCFG_PARSER_MP3 := Y
else
    MPPCFG_PARSER_MP3 := N
endif
export MPPCFG_PARSER_MP3
ifeq ($(CONFIG_mpp_parser_mpg),y)
    MPPCFG_PARSER_MPG := Y
else
    MPPCFG_PARSER_MPG := N
endif
export MPPCFG_PARSER_MPG
ifeq ($(CONFIG_mpp_parser_ogg),y)
    MPPCFG_PARSER_OGG := Y
else
    MPPCFG_PARSER_OGG := N
endif
export MPPCFG_PARSER_OGG
ifeq ($(CONFIG_mpp_parser_ts),y)
    MPPCFG_PARSER_TS := Y
else
    MPPCFG_PARSER_TS := N
endif
export MPPCFG_PARSER_TS
ifeq ($(CONFIG_mpp_parser_wav),y)
    MPPCFG_PARSER_WAV := Y
else
    MPPCFG_PARSER_WAV := N
endif
export MPPCFG_PARSER_WAV
ifeq ($(CONFIG_mpp_stream_http),y)
    MPPCFG_STREAM_HTTP := Y
else
    MPPCFG_STREAM_HTTP := N
endif
export MPPCFG_STREAM_HTTP
ifeq ($(CONFIG_mpp_stream_tcp),y)
    MPPCFG_STREAM_TCP := Y
else
    MPPCFG_STREAM_TCP := N
endif
export MPPCFG_STREAM_TCP
#config if include libmpp_ise.so and enable ise function.
ifeq ($(CONFIG_mpp_ise),y)
    MPPCFG_ISE := Y
else
    MPPCFG_ISE := N
endif
export MPPCFG_ISE
#config if include libmpp_ise.so and enable ise mo module.
ifeq ($(CONFIG_mpp_ise_mo),y)
    MPPCFG_ISE_MO := Y
else
    MPPCFG_ISE_MO := N
endif
export MPPCFG_ISE_MO
#config if include libmpp_ise.so and enable ise gdc module.
ifeq ($(CONFIG_mpp_ise_gdc),y)
    MPPCFG_ISE_GDC := Y
else
    MPPCFG_ISE_GDC := N
endif
export MPPCFG_ISE_GDC
#config if include libmpp_ise.so and enable ise bi module.
ifeq ($(CONFIG_mpp_ise_bi),y)
    MPPCFG_ISE_BI := Y
else
    MPPCFG_ISE_BI := N
endif
export MPPCFG_ISE_BI
#config if include libmpp_ise.so and enable ise bi soft module.
ifeq ($(CONFIG_mpp_ise_bi_soft),y)
    MPPCFG_ISE_BI_SOFT := Y
else
    MPPCFG_ISE_BI_SOFT := N
endif
export MPPCFG_ISE_BI_SOFT
#config if include libmpp_ise.so and enable ise sti module.
ifeq ($(CONFIG_mpp_ise_two_ise),y)
    MPPCFG_ISE_STI := Y
else
    MPPCFG_ISE_STI := N
endif
export MPPCFG_ISE_STI
#config if include libmpp_eis.so and enable eis function.(Electronic Image Stabilization)
ifeq ($(CONFIG_mpp_eis),y)
    MPPCFG_EIS := Y
else
    MPPCFG_EIS := N
endif
export MPPCFG_EIS
#config if include libai_MOD.so and enable MOD(Motion Object Detection) detect.
ifeq ($(CONFIG_mpp_mod),y)
    MPPCFG_MOD := Y
else
    MPPCFG_MOD := N
endif
export MPPCFG_MOD
#config if include libai_VLPR.so and enable VLPR(Vehicle License Plate Recognition) detect.
ifeq ($(CONFIG_mpp_vlpr),y)
    MPPCFG_VLPR := Y
else
    MPPCFG_VLPR := N
endif
export MPPCFG_VLPR
#config if include libawmd.so and enable motion  detect.
ifeq ($(CONFIG_mpp_motion_detect_soft),y)
    MPPCFG_MOTION_DETECT_SOFT := Y
else
    MPPCFG_MOTION_DETECT_SOFT := N
endif
export MPPCFG_MOTION_DETECT_SOFT
#config if include libadas.so and enable adas  detect.
ifeq ($(CONFIG_mpp_adas_detect),y)
    MPPCFG_ADAS_DETECT := Y
else
    MPPCFG_ADAS_DETECT := N
endif
export MPPCFG_ADAS_DETECT
#config if include libadas-v2.so and enable adas-v2  detect.
ifeq ($(CONFIG_mpp_adas_detect_v2),y)
    MPPCFG_ADAS_DETECT_V2 := Y
else
    MPPCFG_ADAS_DETECT_V2 := N
endif
export MPPCFG_ADAS_DETECT_V2
#config if include libai_HCNT.so and enable HCNT(human count) detect.
ifeq ($(CONFIG_mpp_hcnt),y)
    MPPCFG_HCNT := Y
else
    MPPCFG_HCNT := N
endif
export MPPCFG_HCNT
#config if include libai_BDII.so and enable BDII() detect.
ifeq ($(CONFIG_mpp_bdii),y)
    MPPCFG_BDII := Y
else
    MPPCFG_BDII := N
endif
export MPPCFG_BDII
#config if include libeve_face.so and enable face detect.
ifeq ($(CONFIG_mpp_eveface),y)
    MPPCFG_EVEFACE := Y
else
    MPPCFG_EVEFACE := N
endif
export MPPCFG_EVEFACE
#config if enable mpp_aec
ifeq ($(CONFIG_mpp_aec),y)
    MPPCFG_AEC := Y
else
    MPPCFG_AEC := N
endif
export MPPCFG_AEC

#config aec library of webrtc if enable mpp_aec_libwebrtc
ifeq ($(CONFIG_mpp_aec_libwebrtc),y)
    MPPCFG_AEC_LIB := webrtc
endif
ifeq ($(CONFIG_mpp_aec_libuvoice),y)
    MPPCFG_AEC_LIB := uvoice
endif
export MPPCFG_AEC_LIB

#config if enable mpp_softdrc
ifeq ($(CONFIG_mpp_softdrc),y)
    MPPCFG_SOFTDRC := Y
else
    MPPCFG_SOFTDRC := N
endif
export MPPCFG_SOFTDRC
#config if enable mpp_agc
ifeq ($(CONFIG_mpp_agc),y)
    MPPCFG_AGC := Y
else
    MPPCFG_AGC := N
endif
export MPPCFG_AGC
#config agc library of webrtc if enable mpp_agc_libwebrtc
ifeq ($(CONFIG_mpp_agc_libwebrtc),y)
    MPPCFG_AGC_LIB := webrtc
else ifeq ($(CONFIG_mpp_agc_libagcfloat),y)
    MPPCFG_AGC_LIB := agcfloat
endif
export MPPCFG_AGC_LIB
#ifeq ($(CONFIG_mpp_ai_agc),y)
#    MPPCFG_AI_AGC := Y
#else
#    MPPCFG_AI_AGC := N
#endif
#export MPPCFG_AI_AGC

#config if enable mpp_ans
ifeq ($(CONFIG_mpp_ans),y)
    MPPCFG_ANS := Y
else
    MPPCFG_ANS := N
endif
export MPPCFG_ANS

#config ans library of webrtc, lstm, nosc.
ifeq ($(CONFIG_mpp_ans_libwebrtc),y)
    MPPCFG_ANS_LIB := libwebrtc
endif
ifeq ($(CONFIG_mpp_ans_liblstm),y)
    MPPCFG_ANS_LIB := liblstm
endif
ifeq ($(CONFIG_mpp_ans_libnosc),y)
    MPPCFG_ANS_LIB := libnosc
endif
export MPPCFG_ANS_LIB

#config if include libmpp_uvc.so
ifeq ($(CONFIG_mpp_uvc),y)
    MPPCFG_UVC := Y
else
    MPPCFG_UVC := N
endif
export MPPCFG_UVC

#config if enable iommu
ifeq ($(CONFIG_mpp_iommu),y)
    MPPCFG_USE_IOMMU := Y
else
    MPPCFG_USE_IOMMU := N
endif
export MPPCFG_USE_IOMMU
#prebuild all AI libs and component by static
ifeq ($(CONFIG_mpp_compile_static_lib),y)
    MPPCFG_COMPILE_STATIC_LIB := Y
else
    MPPCFG_COMPILE_STATIC_LIB := N
endif
export MPPCFG_COMPILE_STATIC_LIB
#build component to dynamic so if possible
ifeq ($(CONFIG_mpp_compile_dynamic_lib),y)
    MPPCFG_COMPILE_DYNAMIC_LIB := Y
else
    MPPCFG_COMPILE_DYNAMIC_LIB := N
endif
export MPPCFG_COMPILE_DYNAMIC_LIB
ifeq ($(CONFIG_mpp_systrace),y)
    MPPCFG_SYSTRACE := Y
else
    MPPCFG_SYSTRACE := N
endif
export MPPCFG_SYSTRACE
ifeq ($(CONFIG_TOOLCHAIN_LIBC), "musl")
    MPPCFG_TOOLCHAIN_LIBC := musl
else ifeq ($(CONFIG_TOOLCHAIN_LIBC), "glibc")
    MPPCFG_TOOLCHAIN_LIBC := glibc
endif
export MPPCFG_TOOLCHAIN_LIBC
#config if include libfilerepair
ifeq ($(CONFIG_mpp_filerepair),y)
    MPPCFG_FILEREPAIR := Y
else
    MPPCFG_FILEREPAIR := N
endif
export MPPCFG_FILEREPAIR
#config if include libsample_confparser
ifeq ($(CONFIG_mpp_sample_configfileparser),y)
    MPPCFG_SAMPLE_CONFIGFILEPARSER := Y
else
    MPPCFG_SAMPLE_CONFIGFILEPARSER := N
endif
export MPPCFG_SAMPLE_CONFIGFILEPARSER

#select mpp sample
ifeq ($(CONFIG_mpp_sample),y)
    MPPCFG_SAMPLE := Y
else
    MPPCFG_SAMPLE := N
endif
export MPPCFG_SAMPLE
ifeq ($(CONFIG_mpp_sample_takePicture),y)
    MPPCFG_SAMPLE_TAKE_PICTURE := Y
else
    MPPCFG_SAMPLE_TAKE_PICTURE := N
endif
export MPPCFG_SAMPLE_TAKE_PICTURE

ifeq ($(CONFIG_mpp_sample_vencGdcZoom),y)
    MPPCFG_SAMPLE_VENC_GDCZOOM := Y
else
    MPPCFG_SAMPLE_VENC_GDCZOOM := N
endif
export MPPCFG_SAMPLE_VENC_GDCZOOM
ifeq ($(CONFIG_mpp_sample_vencRecreate),y)
    MPPCFG_SAMPLE_VENC_RECREATE := Y
else
    MPPCFG_SAMPLE_VENC_RECREATE := N
endif
export MPPCFG_SAMPLE_VENC_RECREATE
ifeq ($(CONFIG_mpp_sample_OnlineVenc),y)
    MPPCFG_SAMPLE_ONLINEVENC := Y
else
    MPPCFG_SAMPLE_ONLINEVENC := N
endif
export MPPCFG_SAMPLE_ONLINEVENC
ifeq ($(CONFIG_mpp_sample_vencQpMap),y)
    MPPCFG_SAMPLE_VENC_QPMAP := Y
else
    MPPCFG_SAMPLE_VENC_QPMAP := N
endif
export MPPCFG_SAMPLE_VENC_QPMAP
ifeq ($(CONFIG_mpp_sample_EncppGdcOffline),y)
    MPPCFG_SAMPLE_ENCPPGDCOFFLINE := Y
else
    MPPCFG_SAMPLE_ENCPPGDCOFFLINE := N
endif
export MPPCFG_SAMPLE_ENCPPGDCOFFLINE
ifeq ($(CONFIG_mpp_sample_virvi2vo_zoom),y)
    MPPCFG_SAMPLE_VIRVI2VO_ZOOM := Y
else
    MPPCFG_SAMPLE_VIRVI2VO_ZOOM := N
endif
export MPPCFG_SAMPLE_VIRVI2VO_ZOOM
ifeq ($(CONFIG_mpp_sample_CodecParallel),y)
    MPPCFG_SAMPLE_CODEC_PARALLEL := Y
else
    MPPCFG_SAMPLE_CODEC_PARALLEL := N
endif
export MPPCFG_SAMPLE_CODEC_PARALLEL
ifeq ($(CONFIG_mpp_sample_adec),y)
    MPPCFG_SAMPLE_ADEC := Y
else
    MPPCFG_SAMPLE_ADEC := N
endif
export MPPCFG_SAMPLE_ADEC
ifeq ($(CONFIG_mpp_sample_adec2ao),y)
    MPPCFG_SAMPLE_ADEC2AO := Y
else
    MPPCFG_SAMPLE_ADEC2AO := N
endif
export MPPCFG_SAMPLE_ADEC2AO
ifeq ($(CONFIG_mpp_sample_aec),y)
    MPPCFG_SAMPLE_AEC := Y
else
    MPPCFG_SAMPLE_AEC := N
endif
export MPPCFG_SAMPLE_AEC
ifeq ($(CONFIG_mpp_sample_aenc),y)
    MPPCFG_SAMPLE_AENC := Y
else
    MPPCFG_SAMPLE_AENC := N
endif
export MPPCFG_SAMPLE_AENC
ifeq ($(CONFIG_mpp_sample_ai),y)
    MPPCFG_SAMPLE_AI := Y
else
    MPPCFG_SAMPLE_AI := N
endif
export MPPCFG_SAMPLE_AI
ifeq ($(CONFIG_mpp_sample_ai2aenc),y)
    MPPCFG_SAMPLE_AI2AENC := Y
else
    MPPCFG_SAMPLE_AI2AENC := N
endif
export MPPCFG_SAMPLE_AI2AENC
ifeq ($(CONFIG_mpp_sample_ai2aenc2muxer),y)
    MPPCFG_SAMPLE_AI2AENC2MUXER := Y
else
    MPPCFG_SAMPLE_AI2AENC2MUXER := N
endif
export MPPCFG_SAMPLE_AI2AENC2MUXER
ifeq ($(CONFIG_mpp_sample_ao),y)
    MPPCFG_SAMPLE_AO := Y
else
    MPPCFG_SAMPLE_AO := N
endif
export MPPCFG_SAMPLE_AO
ifeq ($(CONFIG_mpp_sample_aoSync),y)
    MPPCFG_SAMPLE_AOSYNC := Y
else
    MPPCFG_SAMPLE_AOSYNC := N
endif
export MPPCFG_SAMPLE_AOSYNC
ifeq ($(CONFIG_mpp_sample_ao_resample_mixer),y)
    MPPCFG_SAMPLE_AO_RESAMPLE_MIXER := Y
else
    MPPCFG_SAMPLE_AO_RESAMPLE_MIXER := N
endif
export MPPCFG_SAMPLE_AO_RESAMPLE_MIXER
ifeq ($(CONFIG_mpp_sample_ao2ai_aec),y)
    MPPCFG_SAMPLE_AO2AI_AEC := Y
else
    MPPCFG_SAMPLE_AO2AI_AEC := N
endif
export MPPCFG_SAMPLE_AO2AI_AEC
ifeq ($(CONFIG_mpp_sample_ao2ai_aec_rate_mixer),y)
    MPPCFG_SAMPLE_AO2AI_AEC_RATE_MIXER := Y
else
    MPPCFG_SAMPLE_AO2AI_AEC_RATE_MIXER := N
endif
export MPPCFG_SAMPLE_AO2AI_AEC_RATE_MIXER
ifeq ($(CONFIG_mpp_sample_avmuxer),y)
    MPPCFG_SAMPLE_AVMUXER := Y
else
    MPPCFG_SAMPLE_AVMUXER := N
endif
export MPPCFG_SAMPLE_AVMUXER
export CONFIG_mpp_sample_avplayer
ifeq ($(CONFIG_mpp_sample_demux),y)
    MPPCFG_SAMPLE_DEMUX := Y
else
    MPPCFG_SAMPLE_DEMUX := N
endif
export MPPCFG_SAMPLE_DEMUX
ifeq ($(CONFIG_mpp_sample_demux2adec),y)
    MPPCFG_SAMPLE_DEMUX2ADEC := Y
else
    MPPCFG_SAMPLE_DEMUX2ADEC := N
endif
export MPPCFG_SAMPLE_DEMUX2ADEC
ifeq ($(CONFIG_mpp_sample_demux2adec2ao),y)
    MPPCFG_SAMPLE_DEMUX2ADEC2AO := Y
else
    MPPCFG_SAMPLE_DEMUX2ADEC2AO := N
endif
export MPPCFG_SAMPLE_DEMUX2ADEC2AO
ifeq ($(CONFIG_mpp_sample_demux2vdec),y)
    MPPCFG_SAMPLE_DEMUX2VDEC := Y
else
    MPPCFG_SAMPLE_DEMUX2VDEC := N
endif
export MPPCFG_SAMPLE_DEMUX2VDEC
ifeq ($(CONFIG_mpp_sample_demux2vdec_saveFrame),y)
    MPPCFG_SAMPLE_DEMUX2VDEC_SAVEFRAME := Y
else
    MPPCFG_SAMPLE_DEMUX2VDEC_SAVEFRAME := N
endif
export MPPCFG_SAMPLE_DEMUX2VDEC_SAVEFRAME
ifeq ($(CONFIG_mpp_sample_demux2vdec2vo),y)
    MPPCFG_SAMPLE_DEMUX2VDEC2VO := Y
else
    MPPCFG_SAMPLE_DEMUX2VDEC2VO := N
endif
export MPPCFG_SAMPLE_DEMUX2VDEC2VO
ifeq ($(CONFIG_mpp_sample_directIORead),y)
    MPPCFG_SAMPLE_DIRECTIOREAD := Y
else
    MPPCFG_SAMPLE_DIRECTIOREAD := N
endif
export MPPCFG_SAMPLE_DIRECTIOREAD
ifeq ($(CONFIG_mpp_sample_driverVipp),y)
    MPPCFG_SAMPLE_DRIVERVIPP := Y
else
    MPPCFG_SAMPLE_DRIVERVIPP := N
endif
export MPPCFG_SAMPLE_DRIVERVIPP
ifeq ($(CONFIG_mpp_sample_file_repair),y)
    MPPCFG_SAMPLE_FILE_REPAIR := Y
else
    MPPCFG_SAMPLE_FILE_REPAIR := N
endif
export MPPCFG_SAMPLE_FILE_REPAIR
ifeq ($(CONFIG_mpp_sample_fish),y)
    MPPCFG_SAMPLE_FISH := Y
else
    MPPCFG_SAMPLE_FISH := N
endif
export MPPCFG_SAMPLE_FISH
ifeq ($(CONFIG_mpp_sample_g2d),y)
    MPPCFG_SAMPLE_G2D := Y
else
    MPPCFG_SAMPLE_G2D := N
endif
export MPPCFG_SAMPLE_G2D
ifeq ($(CONFIG_mpp_sample_glog),y)
    MPPCFG_SAMPLE_GLOG := Y
else
    MPPCFG_SAMPLE_GLOG := N
endif
export MPPCFG_SAMPLE_GLOG
ifeq ($(CONFIG_mpp_sample_hello),y)
    MPPCFG_SAMPLE_HELLO := Y
else
    MPPCFG_SAMPLE_HELLO := N
endif
export MPPCFG_SAMPLE_HELLO
ifeq ($(CONFIG_mpp_sample_isposd),y)
    MPPCFG_SAMPLE_ISPOSD := Y
else
    MPPCFG_SAMPLE_ISPOSD := N
endif
export MPPCFG_SAMPLE_ISPOSD
ifeq ($(CONFIG_mpp_sample_MotionDetect),y)
    MPPCFG_SAMPLE_MOTIONDETECT := Y
else
    MPPCFG_SAMPLE_MOTIONDETECT := N
endif
export MPPCFG_SAMPLE_MOTIONDETECT
ifeq ($(CONFIG_mpp_sample_motor),y)
    MPPCFG_SAMPLE_MOTOR := Y
else
    MPPCFG_SAMPLE_MOTOR := N
endif
export MPPCFG_SAMPLE_MOTOR
ifeq ($(CONFIG_mpp_sample_multi_vi2venc2muxer),y)
    MPPCFG_SAMPLE_MULTI_VI2VENC2MUXER := Y
else
    MPPCFG_SAMPLE_MULTI_VI2VENC2MUXER := N
endif
export MPPCFG_SAMPLE_MULTI_VI2VENC2MUXER
ifeq ($(CONFIG_mpp_sample_muxer_multi_stream),y)
    MPPCFG_SAMPLE_MUXER_MULTI_STREAM := Y
else
    MPPCFG_SAMPLE_MUXER_MULTI_STREAM := N
endif
export MPPCFG_SAMPLE_MUXER_MULTI_STREAM
ifeq ($(CONFIG_mpp_sample_PersonDetect),y)
    MPPCFG_SAMPLE_PERSONDETECT := Y
else
    MPPCFG_SAMPLE_PERSONDETECT := N
endif
export MPPCFG_SAMPLE_PERSONDETECT
ifeq ($(CONFIG_mpp_sample_pthread_cancel),y)
    MPPCFG_SAMPLE_PTHREAD_CANCEL := Y
else
    MPPCFG_SAMPLE_PTHREAD_CANCEL := N
endif
export MPPCFG_SAMPLE_PTHREAD_CANCEL
ifeq ($(CONFIG_mpp_sample_region),y)
    MPPCFG_SAMPLE_REGION := Y
else
    MPPCFG_SAMPLE_REGION := N
endif
export MPPCFG_SAMPLE_REGION

ifeq ($(CONFIG_mpp_sample_RegionDetect),y)
    MPPCFG_SAMPLE_REGION_DETECT := Y
else
    MPPCFG_SAMPLE_REGION_DETECT := N
endif
export MPPCFG_SAMPLE_REGION_DETECT

ifeq ($(CONFIG_mpp_sample_FaceTrack),y)
    MPPCFG_SAMPLE_FACE_TRACK := Y
else
    MPPCFG_SAMPLE_FACE_TRACK := N
endif
export MPPCFG_SAMPLE_FACE_TRACK

ifeq ($(CONFIG_mpp_sample_IntrusionMonitor),y)
    MPPCFG_SAMPLE_INTRUSION_MONITOR := Y
else
    MPPCFG_SAMPLE_INTRUSION_MONITOR := N
endif
export MPPCFG_SAMPLE_INTRUSION_MONITOR

ifeq ($(CONFIG_mpp_sample_rtsp),y)
    MPPCFG_SAMPLE_RTSP := Y
else
    MPPCFG_SAMPLE_RTSP := N
endif
export MPPCFG_SAMPLE_RTSP
ifeq ($(CONFIG_mpp_sample_smartIPC_demo),y)
    MPPCFG_SAMPLE_SMARTIPC_DEMO := Y
else
    MPPCFG_SAMPLE_SMARTIPC_DEMO := N
endif
export MPPCFG_SAMPLE_SMARTIPC_DEMO
ifeq ($(CONFIG_mpp_sample_smartPreview_demo),y)
    MPPCFG_SAMPLE_SMARTPREVIEW_DEMO := Y
else
    MPPCFG_SAMPLE_SMARTPREVIEW_DEMO := N
endif
export MPPCFG_SAMPLE_SMARTPREVIEW_DEMO
ifeq ($(CONFIG_mpp_sample_select),y)
    MPPCFG_SAMPLE_SELECT := Y
else
    MPPCFG_SAMPLE_SELECT := N
endif
export MPPCFG_SAMPLE_SELECT
ifeq ($(CONFIG_mpp_sample_sound_controler),y)
    MPPCFG_SAMPLE_SOUND_CONTROLER := Y
else
    MPPCFG_SAMPLE_SOUND_CONTROLER := N
endif
export MPPCFG_SAMPLE_SOUND_CONTROLER
ifeq ($(CONFIG_mpp_sample_timelapse),y)
    MPPCFG_SAMPLE_TIMELAPSE := Y
else
    MPPCFG_SAMPLE_TIMELAPSE := N
endif
export MPPCFG_SAMPLE_TIMELAPSE
ifeq ($(CONFIG_mpp_sample_twinchn_virvi2venc2ce),y)
    MPPCFG_SAMPLE_TWINCHN_VIRVI2VENC2CE := Y
else
    MPPCFG_SAMPLE_TWINCHN_VIRVI2VENC2CE := N
endif
export MPPCFG_SAMPLE_TWINCHN_VIRVI2VENC2CE
ifeq ($(CONFIG_mpp_sample_UILayer),y)
    MPPCFG_SAMPLE_UILAYER := Y
else
    MPPCFG_SAMPLE_UILAYER := N
endif
export MPPCFG_SAMPLE_UILAYER
ifeq ($(CONFIG_mpp_sample_uvc_vo),y)
    MPPCFG_SAMPLE_UVC_VO := Y
else
    MPPCFG_SAMPLE_UVC_VO := N
endif
export MPPCFG_SAMPLE_UVC_VO
ifeq ($(CONFIG_mpp_sample_uvc2vdec_vo),y)
    MPPCFG_SAMPLE_UVC2VDEC_VO := Y
else
    MPPCFG_SAMPLE_UVC2VDEC_VO := N
endif
export MPPCFG_SAMPLE_UVC2VDEC_VO
ifeq ($(CONFIG_mpp_sample_uvc2vdenc2vo),y)
    MPPCFG_SAMPLE_UVC2VDENC2VO := Y
else
    MPPCFG_SAMPLE_UVC2VDENC2VO := N
endif
export MPPCFG_SAMPLE_UVC2VDENC2VO
ifeq ($(CONFIG_mpp_sample_uvc2vo),y)
    MPPCFG_SAMPLE_UVC2VO := Y
else
    MPPCFG_SAMPLE_UVC2VO := N
endif
export MPPCFG_SAMPLE_UVC2VO
ifeq ($(CONFIG_mpp_sample_uvc_vcodec_vo_uac),y)
    MPPCFG_SAMPLE_UVC_VCODEC_VO_UAC := Y
else
    MPPCFG_SAMPLE_UVC_VCODEC_VO_UAC := N
endif
export MPPCFG_SAMPLE_UVC_VCODEC_VO_UAC
ifeq ($(CONFIG_mpp_sample_uvc_vi_codec),y)
    MPPCFG_SAMPLE_UVC_VI_CODEC := Y
else
    MPPCFG_SAMPLE_UVC_VI_CODEC := N
endif
export MPPCFG_SAMPLE_UVC_VI_CODEC
ifeq ($(CONFIG_mpp_sample_uac),y)
    MPPCFG_SAMPLE_UAC := Y
else
    MPPCFG_SAMPLE_UAC := N
endif
export MPPCFG_SAMPLE_UAC
ifeq ($(CONFIG_mpp_sample_uvcout),y)
    MPPCFG_SAMPLE_UVCOUT := Y
else
    MPPCFG_SAMPLE_UVCOUT := N
endif
export MPPCFG_SAMPLE_UVCOUT
ifeq ($(CONFIG_mpp_sample_usbcamera),y)
    MPPCFG_SAMPLE_USBCAMERA := Y
else
    MPPCFG_SAMPLE_USBCAMERA := N
endif
export MPPCFG_SAMPLE_USBCAMERA
ifeq ($(CONFIG_mpp_sample_vdec),y)
    MPPCFG_SAMPLE_VDEC := Y
else
    MPPCFG_SAMPLE_VDEC := N
endif
export MPPCFG_SAMPLE_VDEC
ifeq ($(CONFIG_mpp_sample_venc),y)
    MPPCFG_SAMPLE_VENC := Y
else
    MPPCFG_SAMPLE_VENC := N
endif
export MPPCFG_SAMPLE_VENC
ifeq ($(CONFIG_mpp_sample_venc2muxer),y)
    MPPCFG_SAMPLE_VENC2MUXER := Y
else
    MPPCFG_SAMPLE_VENC2MUXER := N
endif
export MPPCFG_SAMPLE_VENC2MUXER
ifeq ($(CONFIG_mpp_sample_vi_g2d),y)
    MPPCFG_SAMPLE_VI_G2D := Y
else
    MPPCFG_SAMPLE_VI_G2D := N
endif
export MPPCFG_SAMPLE_VI_G2D
ifeq ($(CONFIG_mpp_sample_vi_reset),y)
    MPPCFG_SAMPLE_VI_RESET := Y
else
    MPPCFG_SAMPLE_VI_RESET := N
endif
export MPPCFG_SAMPLE_VI_RESET
ifeq ($(CONFIG_mpp_sample_vin_isp_test),y)
    MPPCFG_SAMPLE_VIN_ISP_TEST := Y
else
    MPPCFG_SAMPLE_VIN_ISP_TEST := N
endif
export MPPCFG_SAMPLE_VIN_ISP_TEST
ifeq ($(CONFIG_mpp_sample_virvi),y)
    MPPCFG_SAMPLE_VIRVI := Y
else
    MPPCFG_SAMPLE_VIRVI := N
endif
export MPPCFG_SAMPLE_VIRVI
ifeq ($(CONFIG_mpp_sample_virvi2eis2venc),y)
    MPPCFG_SAMPLE_VIRVI2EIS2VENC := Y
else
    MPPCFG_SAMPLE_VIRVI2EIS2VENC := N
endif
export MPPCFG_SAMPLE_VIRVI2EIS2VENC
ifeq ($(CONFIG_mpp_sample_virvi2fish2venc),y)
    MPPCFG_SAMPLE_VIRVI2FISH2VENC := Y
else
    MPPCFG_SAMPLE_VIRVI2FISH2VENC := N
endif
export MPPCFG_SAMPLE_VIRVI2FISH2VENC
ifeq ($(CONFIG_mpp_sample_virvi2fish2vo),y)
    MPPCFG_SAMPLE_VIRVI2FISH2VO := Y
else
    MPPCFG_SAMPLE_VIRVI2FISH2VO := N
endif
export MPPCFG_SAMPLE_VIRVI2FISH2VO
ifeq ($(CONFIG_mpp_sample_virvi2venc),y)
    MPPCFG_SAMPLE_VIRVI2VENC := Y
else
    MPPCFG_SAMPLE_VIRVI2VENC := N
endif
export MPPCFG_SAMPLE_VIRVI2VENC
ifeq ($(CONFIG_mpp_sample_virvi2vencSync),y)
    MPPCFG_SAMPLE_VIRVI2VENCSYNC := Y
else
    MPPCFG_SAMPLE_VIRVI2VENCSYNC := N
endif
export MPPCFG_SAMPLE_VIRVI2VENCSYNC
ifeq ($(CONFIG_mpp_sample_virvi2venc2ce),y)
    MPPCFG_SAMPLE_VIRVI2VENC2CE := Y
else
    MPPCFG_SAMPLE_VIRVI2VENC2CE := N
endif
export MPPCFG_SAMPLE_VIRVI2VENC2CE
ifeq ($(CONFIG_mpp_sample_virvi2venc2muxer),y)
    MPPCFG_SAMPLE_VIRVI2VENC2MUXER := Y
else
    MPPCFG_SAMPLE_VIRVI2VENC2MUXER := N
endif
export MPPCFG_SAMPLE_VIRVI2VENC2MUXER
ifeq ($(CONFIG_mpp_sample_virvi2vo),y)
    MPPCFG_SAMPLE_VIRVI2VO := Y
else
    MPPCFG_SAMPLE_VIRVI2VO := N
endif
export MPPCFG_SAMPLE_VIRVI2VO
ifeq ($(CONFIG_mpp_sample_vo),y)
    MPPCFG_SAMPLE_VO := Y
else
    MPPCFG_SAMPLE_VO := N
endif
export MPPCFG_SAMPLE_VO

ifeq ($(CONFIG_mpp_sample_odet_demo),y)
    MPPCFG_SAMPLE_ODET_DEMO := Y
else
    MPPCFG_SAMPLE_ODET_DEMO := N
endif
export MPPCFG_SAMPLE_ODET_DEMO

ifeq ($(CONFIG_mpp_sample_facekit_demo),y)
    MPPCFG_SAMPLE_FACEKIT_DEMO := Y
else
    MPPCFG_SAMPLE_FACEKIT_DEMO := N
endif
export MPPCFG_SAMPLE_FACEKIT_DEMO

ifeq ($(CONFIG_mpp_sample_recorder),y)
    MPPCFG_SAMPLE_RECORDER := Y
else
    MPPCFG_SAMPLE_RECORDER := N
endif
export MPPCFG_SAMPLE_RECORDER

ifeq ($(CONFIG_mpp_sample_CDR_demo),y)
    MPPCFG_SAMPLE_CDR_DEMO := Y
else
    MPPCFG_SAMPLE_CDR_DEMO := N
endif
export MPPCFG_SAMPLE_CDR_DEMO

ifeq ($(CONFIG_mpp_sample_ai2ao),y)
    MPPCFG_SAMPLE_AI2AO := Y
else
    MPPCFG_SAMPLE_AI2AO := N
endif
export MPPCFG_SAMPLE_AI2AO

ifeq ($(CONFIG_mpp_sample_WebRtcAec),y)
    MPPCFG_SAMPLE_WEBRTCAEC := Y
else
    MPPCFG_SAMPLE_WEBRTCAEC := N
endif
export MPPCFG_SAMPLE_WEBRTCAEC

ifeq ($(CONFIG_mpp_sample_UvoiceAec),y)
    MPPCFG_SAMPLE_UVOICEAEC := Y
else
    MPPCFG_SAMPLE_UVOICEAEC := N
endif
export MPPCFG_SAMPLE_UVOICEAEC

ifeq ($(CONFIG_mpp_sample_VideoResizer),y)
    MPPCFG_SAMPLE_VIDEORESIZER := Y
else
    MPPCFG_SAMPLE_VIDEORESIZER := N
endif
export MPPCFG_SAMPLE_VIDEORESIZER

# Specify package information for this program.
# The variables defined here should be self explanatory.
# If you are running Kamikaze, delete the DESCRIPTION
# variable below and uncomment the Kamikaze define
# directive for the description below
define Package/$(PKG_NAME)
  SECTION:=eyesee-mpp-middleware
  CATEGORY:=Allwinner
  SUBMENU:=eyesee-mpp
  TITLE:=eyesee-mpp-middleware for Tina Linux
  DEPENDS:= +eyesee-mpp-system +mpp_demuxer:zlib +alsa-lib +mpp_systrace:awsystrace +mpp_venc_support_venc_param_debug:libexpat \
      +libawion
endef

define Package/$(PKG_NAME)-demo
  CATEGORY:=Allwinner
  SUBMENU:=eyesee-mpp
  TITLE:=samples for eyesee-mpp-middleware
  DEPENDS:= $(PKG_NAME) +(mpp_sample_smartIPC_demo||mpp_sample_rtsp):eyesee-mpp-private-rtsp +(mpp_sample_aisr):libawaisr
endef

define Package/$(PKG_NAME)/config
source "$(SOURCE)/Config.in"
endef

define Package/$(PKG_NAME)-demo/config
source "$(SOURCE)/Config-demo.in"
endef

# Uncomment portion below for Kamikaze and delete DESCRIPTION variable above
define Package/$(PKG_NAME)/description
  eyesee-mpp-middleware contains eyesee-mpp basic libraries.
endef

define Package/$(PKG_NAME)-demo/description
  eyesee-mpp-middleware-demo contains all samples designed to test mpp-middleware and to show how to use mpp-middleware.
endef

# Specify what needs to be done to prepare for building the package.
# In our case, we need to copy the source files to the build directory.
# This is NOT the default.  The default uses the PKG_SOURCE_URL and the
# PKG_SOURCE which is not defined here to download the source from the web.
# In order to just build a simple program that we have just written, it is
# much easier to do it this way.

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	if [ -d $(SRC_COMPILE_DIR) ]; then \
		rm -rf $(SRC_COMPILE_DIR); \
	fi
	# copy custom mpp_config.mk to source code directory.
	#if [ -f $(TOPDIR)/target/allwinner/$(BOARD)/custom-config/mpp_config.mk ]; then \
	#	echo "copy happen!"; \
	#	$(CP) $(TOPDIR)/target/allwinner/$(BOARD)/custom-config/mpp_config.mk $(SRC_CODE_DIR)/config/mpp_config.mk; \
	#fi
	ln -s $(SRC_CODE_DIR) $(SRC_COMPILE_DIR)
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) $(PKG_JOBS) -C $(SRC_COMPILE_DIR) -f tina.mk \
		ARCH="$(TARGET_ARCH)" \
		AR="$(TARGET_AR)" \
		CC="$(TARGET_CC)" \
		CXX="$(TARGET_CXX)" \
		CFLAGS="$(TARGET_CFLAGS)" \
		CXXFLAGS="$(TARGET_CXXFLAGS)" \
		CPPFLAGS="$(TARGET_CPPFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		LINUX_USER_HEADERS="$(LINUX_DIR)/user_headers" \
		all
endef

define Build/Clean
	if [ -d $(SRC_COMPILE_DIR) ]; then \
		MPPCFG_COMPILE_DYNAMIC_LIB=Y \
		MPPCFG_COMPILE_STATIC_LIB=Y \
		$(MAKE) -C $(SRC_COMPILE_DIR) -f tina.mk \
			ARCH="$(TARGET_ARCH)" \
			AR="$(TARGET_AR)" \
			CC="$(TARGET_CC)" \
			CXX="$(TARGET_CXX)" \
			CFLAGS="$(TARGET_CFLAGS)" \
			CXXFLAGS="$(TARGET_CXXFLAGS)" \
			CPPFLAGS="$(TARGET_CPPFLAGS)" \
			LDFLAGS="$(TARGET_LDFLAGS)" \
			clean; \
	else \
		echo "$(SRC_COMPILE_DIR) is not exist!"; \
	fi
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib/eyesee-mpp/
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware
	$(CP) -r $(SRC_COMPILE_DIR)/include $(1)/usr/include/eyesee-mpp/middleware
	$(CP) -r $(SRC_COMPILE_DIR)/config $(1)/usr/include/eyesee-mpp/middleware
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media
	$(CP) -r $(SRC_COMPILE_DIR)/media/include $(1)/usr/include/eyesee-mpp/middleware/media
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/include_ai_common ]; then \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/include_ai_common $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY; \
	fi
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/include_demux $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/include_eve_common ]; then \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/include_eve_common $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY; \
	fi
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/include_FsWriter $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/include_muxer $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/include_stream $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY
	if [ -f $(SRC_COMPILE_DIR)/media/utils/libmedia_utils.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/utils/libmedia_utils.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/utils/libmedia_utils.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/utils/libmedia_utils.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libstream/libcedarxstream.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libstream/libcedarxstream.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libstream/libcedarxstream.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libstream/libcedarxstream.a $(1)/usr/lib/eyesee-mpp; \
	fi
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libFsWriter/libFsWriter.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libFsWriter/libFsWriter.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/common/libavutil/libffavutil.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/common/libavutil/libffavutil.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/mp3_muxer/libmp3_muxer.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/mp3_muxer/libmp3_muxer.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/aac_muxer/libaac_muxer.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/aac_muxer/libaac_muxer.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/wav_muxer/libwav_muxer.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/wav_muxer/libwav_muxer.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/mp4_muxer/libmp4_muxer.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/mp4_muxer/libmp4_muxer.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/mpeg2ts_muxer/libmpeg2ts_muxer.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/mpeg2ts_muxer/libmpeg2ts_muxer.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/raw_muxer/libraw_muxer.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/raw_muxer/libraw_muxer.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/muxers/libmuxers.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libmuxer/muxers/libmuxers.a $(1)/usr/lib/eyesee-mpp; \
	fi
endif
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libfilerepair
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libfilerepair/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libfilerepair
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libfilerepair \( -name "libfilerepair.a" -o -name "libfilerepair.so" \) | xargs -I{} $(CP) {} $(1)/usr/lib/eyesee-mpp
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/*.h $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/isp_tuning
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/isp_tuning/*.h $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/isp_tuning
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/isp_dev
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/isp_dev/*.h $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libisp/isp_dev
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/out/out/ ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/out/out/ -name "*.a" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
endif
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/iniparser/libiniparser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/iniparser/libiniparser.a $(1)/usr/lib/eyesee-mpp; \
	fi
endif
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/isp_cfg/libisp_ini.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/isp_cfg/libisp_ini.so $(1)/usr/lib/eyesee-mpp; \
	fi
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/isp_cfg/libisp_ini.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/isp_cfg/libisp_ini.a $(1)/usr/lib/eyesee-mpp; \
	fi
endif
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/isp_dev/libisp_dev.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/isp_dev/libisp_dev.a $(1)/usr/lib/eyesee-mpp; \
	fi
endif
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/libISP.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/libISP.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/libISP.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/libISP.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libMODSoft ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libMODSoft; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libMODSoft/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libMODSoft; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libMODSoft/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libMODSoft/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libADAS; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libADAS; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libISE
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libISE/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libISE
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libISE/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libISE/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/aec_lib ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/aec_lib; \
		if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/aec_lib/include ]; then \
			$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/aec_lib/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/aec_lib; \
		fi \
	fi
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/aec_lib/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/aec_lib/out -name "*.a" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
endif
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/drc_lib ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/drc_lib; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/drc_lib/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/drc_lib; \
	fi
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/drc_lib/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/drc_lib/out -name "*.a" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
endif
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/agc_lib ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/agc_lib; \
		if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/agc_lib/include ]; then \
			$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/agc_lib/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/agc_lib; \
		fi \
	fi
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/agc_lib/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/agc_lib/out -name "*.a" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
endif
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/ans_lib ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/ans_lib; \
		if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/ans_lib/include ]; then \
			$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/ans_lib/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/ans_lib; \
		fi \
	fi
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/ans_lib/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/ans_lib/out -name "*.a" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
endif
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libResample ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libResample; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libResample/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libResample; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libResample ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libResample \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libAudioVps ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libAudioVps; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libAudioVps/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libAudioVps; \
	fi
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libAudioVps/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libAudioVps/out -name "*.a" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
endif
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libcedarc
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libcedarc
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/base/include/UserKernelAdapter.h ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libcedarc/base/include; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/base/include/UserKernelAdapter.h $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libcedarc/base/include; \
	fi
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/base \( -name "libcdc_base.a" -o -name "libcdc_base.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/ve ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/ve \( -name "libVE.a" -o -name "libVE.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/library/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/library/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/memory \( -name "libMemAdapter.a" -o -name "libMemAdapter.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/base \( -name "libvenc_base.a" -o -name "libvenc_base.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec \( -name "libvenc_codec.a" -o -name "libvenc_codec.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/common ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/common \( -name "libvenc_common.a" -o -name "libvenc_common.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/h264 ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/h264 \( -name "libvenc_h264.a" -o -name "libvenc_h264.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/h265 ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/h265 \( -name "libvenc_h265.a" -o -name "libvenc_h265.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/jpeg ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/jpeg \( -name "libvenc_jpeg.a" -o -name "libvenc_jpeg.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder -maxdepth 1 \( -name "libvencoder.a" -o -name "libvencoder.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine \( -name "libvideoengine.a" -o -name "libvideoengine.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder -maxdepth 1 \( -name "libvdecoder.a" -o -name "libvdecoder.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/h264 ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/h264 \( -name "libawh264.a" -o -name "libawh264.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/h265 ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/h265 \( -name "libawh265.a" -o -name "libawh265.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpegplus ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpegplus \( -name "libawmjpegplus.a" -o -name "libawmjpegplus.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpeg ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpeg \( -name "libawmjpeg.a" -o -name "libawmjpeg.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/base \( -name "libcdx_base.a" -o -name "libcdx_base.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libIniParser
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libIniParser/*.h $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libIniParser
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libIniParser/libIniParserMpp.a $(1)/usr/lib/eyesee-mpp
endif
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libPlugin/plugin \( -name "libPluginMpp.a" -o -name "libPluginMpp.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/stream/file/libcdx_file_stream.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/stream/file/libcdx_file_stream.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/stream/http/libcdx_http_stream.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/stream/http/libcdx_http_stream.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/stream/tcp/libcdx_tcp_stream.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/stream/tcp/libcdx_tcp_stream.a $(1)/usr/lib/eyesee-mpp; \
	fi
endif
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/stream/base \( -name "libcdx_stream.a" -o -name "libcdx_stream.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/aac/libcdx_aac_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/aac/libcdx_aac_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/id3v2/libcdx_id3v2_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/id3v2/libcdx_id3v2_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/mp3/libcdx_mp3_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/mp3/libcdx_mp3_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/mov/libcdx_mov_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/mov/libcdx_mov_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/mpg/libcdx_mpg_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/mpg/libcdx_mpg_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/ts/libcdx_ts_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/ts/libcdx_ts_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/wav/libcdx_wav_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/wav/libcdx_wav_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/amr/libcdx_amr_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/amr/libcdx_amr_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/ape/libcdx_ape_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/ape/libcdx_ape_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/flac/libcdx_flac_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/flac/libcdx_flac_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/ogg/libcdx_ogg_parser.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/ogg/libcdx_ogg_parser.a $(1)/usr/lib/eyesee-mpp; \
	fi
endif
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/base \( -name "libcdx_parser.a" -o -name "libcdx_parser.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemux/libcedarxdemuxer.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemux/libcedarxdemuxer.a $(1)/usr/lib/eyesee-mpp; \
	fi
endif
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/AudioLib
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/osal $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/AudioLib
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/AudioLib/midware/encoding
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/midware/encoding/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/AudioLib/midware/encoding
	$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/midware/encoding/*.h $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/AudioLib/midware/encoding
	find $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/midware/encoding -maxdepth 1 \( -name "libcedarx_aencoder.a" -o -name "libcedarx_aencoder.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/AudioLib/midware/decoding
	$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/midware/decoding/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/AudioLib/midware/decoding
	find $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/midware/decoding -maxdepth 1 \( -name "libadecoder.a" -o -name "libadecoder.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/lib/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/lib/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
ifeq ($(CONFIG_mpp_compile_static_lib),y)
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/lib/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/lib/out -name "*.a" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
endif
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libai_common/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libai_common/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/textEncLib
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/textEncLib/include ]; then \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/textEncLib/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/textEncLib; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/textEncLib ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/textEncLib -maxdepth 1 \( -name "libcedarx_tencoder.a" -o -name "libcedarx_tencoder.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libaiMOD ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libaiMOD; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libaiMOD/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libaiMOD; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libaiMOD/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libaiMOD/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libADAS; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libADAS; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi

	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libaiHCNT ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libaiHCNT; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libaiHCNT/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libaiHCNT; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libaiHCNT/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libaiHCNT/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libaiBDII ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libaiBDII; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libaiBDII/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libaiBDII; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libaiBDII/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libaiBDII/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libVLPR ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libVLPR; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libVLPR/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libVLPR; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libVLPR/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libVLPR/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libeveface ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libeveface; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libeveface/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libeveface; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libeveface/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libeveface/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libevekernel/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libevekernel/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libBodyDetect ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libBodyDetect; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/libBodyDetect/include $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/libBodyDetect; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libBodyDetect/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libBodyDetect/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libVideoStabilization/libEIS.a ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libVideoStabilization/libEIS.a $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libVideoStabilization/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libVideoStabilization/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_algo/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_algo/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_core ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/lib_aw_ai_core/include; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_core/include/* $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/lib_aw_ai_core/include; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_core/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_core/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_mt ]; then \
		$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/lib_aw_ai_mt/include; \
		$(CP) -r $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_mt/include/* $(1)/usr/include/eyesee-mpp/middleware/media/LIBRARY/lib_aw_ai_mt/include; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_mt/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_mt/out \( -name "*.a" -o -name "*.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/librender -maxdepth 1 \( -name "libcedarxrender.a" -o -name "libcedarxrender.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_vi.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_isp.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_ise.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_eis.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_vo.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_uvc.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media/component -maxdepth 1 -name "libmpp_component.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmedia_mpp.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libaw_mpp.a" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	$(INSTALL_DIR) $(1)/usr/include/eyesee-mpp/middleware/sample/configfileparser
	$(CP) -r $(SRC_COMPILE_DIR)/sample/configfileparser/*.h $(1)/usr/include/eyesee-mpp/middleware/sample/configfileparser
	find $(SRC_COMPILE_DIR)/sample/configfileparser -maxdepth 1 \( -name "libsample_confparser.a" -o -name "libsample_confparser.so" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
endef

# We do not need to define Build/Configure or Build/Compile directives
# The defaults are appropriate for compiling a simple program such as this one


# Specify where and how to install the program. Since we only have one file,
# the helloworld executable, install it by copying it to the /bin directory on
# the router. The $(1) variable represents the root directory on the router running
# OpenWrt. The $(INSTALL_DIR) variable contains a command to prepare the install
# directory if it does not already exist.  Likewise $(INSTALL_BIN) contains the
# command to copy the binary file from its current location (in our case the build
# directory) to the install directory.
define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/lib/eyesee-mpp
	$(INSTALL_DIR) $(1)/usr/bin
	if [ -f $(SRC_COMPILE_DIR)/media/utils/libmedia_utils.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/utils/libmedia_utils.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libstream/libcedarxstream.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libstream/libcedarxstream.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libfilerepair/libfilerepair.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libfilerepair/libfilerepair.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/out/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/out/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/isp_cfg/libisp_ini.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/isp_cfg/libisp_ini.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/libISP.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libisp/libISP.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libMODSoft/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libMODSoft/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libISE/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libISE/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/base -name "libcdc_base.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/ve ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/ve -name "libVE.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/library/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/library/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/memory -name "libMemAdapter.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/base -name "libvenc_base.so"  -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec -name "libvenc_codec.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/common ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/common -name "libvenc_common.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/h264 ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/h264 -name "libvenc_h264.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/h265 ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/h265 -name "libvenc_h265.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/jpeg ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder/libcodec/jpeg -name "libvenc_jpeg.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vencoder -maxdepth 1 -name "libvencoder.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine -name "libvideoengine.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder -maxdepth 1 -name "libvdecoder.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/h264 ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/h264 -name "libawh264.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/h265 ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/h265 -name "libawh265.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpegplus ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpegplus -name "libawmjpegplus.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpeg ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpeg -name "libawmjpeg.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	$(INSTALL_DIR) $(1)/etc
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/config ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libcedarc/config -name "cedarc.conf" -exec $(CP) {} $(1)/etc \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libPlugin/config/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libPlugin/config/out -name "cedarx.conf" -exec $(CP) {} $(1)/etc \;; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libPlugin/plugin/libPluginMpp.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libPlugin/plugin/libPluginMpp.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/base/libcdx_base.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/base/libcdx_base.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/stream/base/libcdx_stream.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/stream/base/libcdx_stream.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -f $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/base/libcdx_parser.so ]; then \
		$(CP) $(SRC_COMPILE_DIR)/media/LIBRARY/libdemuxer/libcore/parser/base/libcdx_parser.so $(1)/usr/lib/eyesee-mpp; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/lib/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/lib/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/midware/encoding -maxdepth 1 -name "libcedarx_aencoder.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media/LIBRARY/AudioLib/midware/decoding -maxdepth 1 -name "libadecoder.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/aec_lib/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/aec_lib/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/ans_lib/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/ans_lib/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libai_common/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libai_common/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi

	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/textEncLib ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/textEncLib -maxdepth 1 -name "libcedarx_tencoder.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi

	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libaiMOD/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libaiMOD/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libADAS/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libaiHCNT/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libaiHCNT/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libaiBDII/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libaiBDII/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libVLPR/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libVLPR/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libeveface/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libeveface/out \( -name "*.so" -o -name "*.ld" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libevekernel/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libevekernel/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libBodyDetect/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libBodyDetect/out \( -name "*.so" -o -name "pdet.*" \) -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libVideoStabilization/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libVideoStabilization/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_algo/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_algo/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_core/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_core/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_mt/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/lib_aw_ai_mt/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libResample ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libResample -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	if [ -d $(SRC_COMPILE_DIR)/media/LIBRARY/libAudioVps/out ]; then \
		find $(SRC_COMPILE_DIR)/media/LIBRARY/libAudioVps/out -name "*.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;; \
	fi
	find $(SRC_COMPILE_DIR)/media/librender -maxdepth 1 -name "libcedarxrender.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_vi.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_isp.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_ise.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_eis.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_vo.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmpp_uvc.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media/component -maxdepth 1 -name "libmpp_component.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/media -maxdepth 1 -name "libmedia_mpp.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;
	find $(SRC_COMPILE_DIR)/sample/configfileparser -maxdepth 1 -name "libsample_confparser.so" -exec $(CP) {} $(1)/usr/lib/eyesee-mpp \;


	#	$(INSTALL_BIN) $(SRC_COMPILE_DIR)/logcat/alogcat.sh $(1)/bin/alogcat
endef

define Package/$(PKG_NAME)-demo/install
	@echo "package $(PKG_NAME)-demo has no libs or exeFiles to install.";
endef

#define Package/$(PKG_NAME)/InstallDev
#endef

# This line executes the necessary commands to compile our program.
# The above define directives specify all the information needed, but this
# line calls BuildPackage which in turn actually uses this information to
# build a package.

$(eval $(call BuildPackage,$(PKG_NAME)))
$(eval $(call BuildPackage,$(PKG_NAME)-demo))
