############################################################################################
# 			eyesee-mpp-framework for tina(OpenWrt) Linux
#
#	eyesee-mpp is designed for CDR/SDV product, focus on video/audio capturing and encoding, 
# it also can support video/audio decode.
#   eyesee-mpp-framework is designed for our apps. It wraps the use of mpp-components, 
#   providing simple classes such as camera, recorder and player to apps to use.
#
# Version: v1.0
# Date   : 2019-2-18
# Author : PDC-PD5
############################################################################################
all:
	@echo ==================================================
	@echo build eyesee-mpp-framework
	@echo ==================================================
	make -C utils -f tina.mk                  all
	make -C media/camera -f tina.mk           all
ifeq ($(MPPCFG_ISE),Y)
	make -C media/ise -f tina.mk              all
endif
ifeq ($(MPPCFG_EIS),Y)
	make -C media/eis -f tina.mk              all
endif
	make -C media/recorder -f tina.mk         all
	make -C media/player -f tina.mk           all
	make -C media/thumbretriever -f tina.mk   all
	make -C media/motion -f tina.mk           all
ifeq ($(MPPCFG_UVC),Y)
	make -C media/usbcamera -f tina.mk        all
endif
ifeq ($(MPPFRAMEWORKCFG_VIDEORESIZER),Y)
	make -C media/videoresizer -f tina.mk     all
endif
ifeq ($(MPPCFG_BDII),Y)
	make -C media/bdii -f tina.mk             all
endif
#	make -C demo/sample_ADAS  -f tina.mk      all
#	make -C demo/sample_AudioEncode -f tina.mk all
#	make -C demo/sample_Camera -f tina.mk     all
#	make -C demo/sample_EncodeResolutionChange -f tina.mk all
#	make -C demo/sample_Player -f tina.mk     all
#	make -C demo/sample_RecordMultiStream -f tina.mk all
#	make -C demo/sample_OSD -f tina.mk all
#	make -C demo/sample_RecorderCallbackOut -f tina.mk all
#	make -C demo/sample_RecordSwitchFileNormal -f tina.mk all
#	make -C demo/sample_RecorderSegment -f tina.mk all
#	make -C demo/sample_RecordLowDelay -f tina.mk all

ifeq ($(MPPCFG_MPP_FW_SAMPLE_CAMERA),Y)
	make -C demo -f tina.mk TARGET=sample_Camera  all
endif

ifeq ($(MPPCFG_MPP_FW_SAMPLE_CAMERA_LARGEPHOTO),Y)
	make -C demo -f tina.mk TARGET=sample_Camera_LargePhoto  all
endif

ifeq ($(MPPCFG_MPP_FW_SAMPLE_CDREMERGENCY),Y)
	make -C demo -f tina.mk TARGET=sample_CDREmergency all
endif

ifeq ($(MPPCFG_MPP_FW_SAMPLE_ENCODERESOLUTIONCHANGE),Y)
	make -C demo -f tina.mk TARGET=sample_EncodeResolutionChange  all
endif

ifeq ($(MPPCFG_MPP_FW_SAMPLE_RECORDERCALLBACKOUT),Y)
	make -C demo -f tina.mk TARGET=sample_RecorderCallbackOut  all
endif	

ifeq ($(MPPCFG_MPP_FW_SAMPLE_RECORDERSEGMENT),Y)
	make -C demo -f tina.mk TARGET=sample_RecorderSegment  all
endif	

ifeq ($(MPPCFG_MPP_FW_SAMPLE_RECORDLOWDELAY),Y)
	make -C demo -f tina.mk TARGET=sample_RecordLowDelay  all
endif	

ifeq ($(MPPCFG_MPP_FW_SAMPLE_RECORDMULTISTREAM),Y)
	make -C demo -f tina.mk TARGET=sample_RecordMultiStream  all
endif

ifeq ($(MPPCFG_MPP_FW_SAMPLE_RECORDSWITCHFILENORMAL),Y)
	make -C demo -f tina.mk TARGET=sample_RecordSwitchFileNormal  all
endif	

ifeq ($(MPPCFG_MPP_FW_SAMPLE_THUMBRETRIEVER),Y)
	make -C demo -f tina.mk TARGET=sample_ThumbRetriever  all
endif	

ifeq ($(MPPCFG_MPP_FW_SAMPLE_OSD),Y)
	make -C demo -f tina.mk TARGET=sample_OSD  all
endif

ifeq ($(MPPCFG_MPP_FW_SAMPLE_PLAYER),Y)
	make -C demo -f tina.mk TARGET=sample_Player  all
endif

ifeq ($(MPPCFG_MPP_FW_SAMPLE_USBCAMERA),Y)
	make -C demo -f tina.mk TARGET=sample_USBCamera  all
endif

	@echo build eyesee-mpp-framework done!

clean:
	@echo ==================================================
	@echo clean eyesee-mpp-framework
	@echo ==================================================
	make -C utils -f tina.mk                  clean
	make -C media/camera -f tina.mk           clean
	make -C media/ise -f tina.mk              clean
	make -C media/eis -f tina.mk              clean
	make -C media/recorder -f tina.mk         clean
	make -C media/player -f tina.mk           clean
	make -C media/thumbretriever -f tina.mk   clean
	make -C media/motion -f tina.mk           clean
	make -C media/usbcamera -f tina.mk        clean
	make -C media/videoresizer -f tina.mk     clean
	make -C media/bdii -f tina.mk             clean
#	make -C demo/sample_ADAS  -f tina.mk	  clean
#	make -C demo/sample_AudioEncode -f tina.mk clean
#	make -C demo/sample_Camera -f tina.mk     clean
#	make -C demo/sample_EncodeResolutionChange -f tina.mk clean
#	make -C demo/sample_Player -f tina.mk     clean
#	make -C demo/sample_RecordMultiStream -f tina.mk clean
#	make -C demo/sample_OSD -f tina.mk clean
#	make -C demo/sample_RecorderCallbackOut -f tina.mk clean
#	make -C demo/sample_RecordSwitchFileNormal -f tina.mk clean
#	make -C demo/sample_RecorderSegment -f tina.mk clean
#	make -C demo/sample_RecordLowDelay -f tina.mk clean

	make -C demo -f tina.mk TARGET=sample_Camera  clean
	make -C demo -f tina.mk TARGET=sample_Camera_LargePhoto  clean
	make -C demo -f tina.mk TARGET=sample_CDREmergency clean
	make -C demo -f tina.mk TARGET=sample_EncodeResolutionChange clean
	make -C demo -f tina.mk TARGET=sample_RecorderCallbackOut  clean
	make -C demo -f tina.mk TARGET=sample_RecorderSegment  clean
	make -C demo -f tina.mk TARGET=sample_RecordLowDelay  clean
	make -C demo -f tina.mk TARGET=sample_RecordMultiStream  clean
	make -C demo -f tina.mk TARGET=sample_RecordSwitchFileNormal  clean
	make -C demo -f tina.mk TARGET=sample_ThumbRetriever  clean
	make -C demo -f tina.mk TARGET=sample_OSD  clean
	make -C demo -f tina.mk TARGET=sample_Player  clean
	make -C demo -f tina.mk TARGET=sample_USBCamera  clean

	@echo clean eyesee-mpp-framework done!

