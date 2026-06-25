# Makefile for eyesee-mpp/middleware/sample
CUR_PATH := .
PACKAGE_TOP := $(PACKAGE_TOP)
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

# used to store all the generated sample bin files

include $(PACKAGE_TOP)/config/mpp_config.mk
SCLIB_TOP=${CUR_PATH}/../..
include $(SCLIB_TOP)/config.mk
# based on different samples, specify the corresponding configuration

SRCCS := \
    patternDemo.c \
	encPattern.c \
	decPattern.c

LOCAL_TARGET_BIN := patternDemo

LOCAL_SHARED_LIBS :=
LOCAL_STATIC_LIBS :=

ifeq ($(MPPCFG_TOOLCHAIN_LIBC), glibc)
LOCAL_SHARED_LIBS += \
	libdl \
	librt \
	libpthread
endif

######################## static lib mode ########################

LOCAL_SHARED_LIBS += \
    libasound \
    libglog

ifeq ($(VENC_SUPPORT_EXT_PARAM),Y)
LOCAL_SHARED_LIBS += \
    libexpat
endif

# Public static library
LOCAL_STATIC_LIBS += \
    libz \
    liblog \
    libion \
    libMemAdapter \
    libVE \
    libcdc_base

ifeq ($(VENC_SUPPORT_EXT_PARAM),Y)
LOCAL_STATIC_LIBS += \
    libexpat
endif

# These are the libraries corresponding to MPP components

LOCAL_STATIC_LIBS += \
    libvencoder \
    libvenc_common \
    libvenc_base

ifeq ($(USE_VENC_H264), true)
    LOCAL_STATIC_LIBS += libvenc_h264
endif

ifeq ($(USE_VENC_H265), true)
    LOCAL_STATIC_LIBS += libvenc_h265
endif

ifeq ($(USE_VENC_JPEG), true)
    LOCAL_STATIC_LIBS += libvenc_jpeg
endif
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
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/include

INCLUDE_DIRS += \
    $(SCLIB_TOP)/include \
    $(SCLIB_TOP)/base/include \
    $(SCLIB_TOP)/vencoder/base/include

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
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/ve \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/memory \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/base \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec/common \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec/h264 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec/h265 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vencoder/libcodec/jpeg \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine/h264 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine/h265 \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/vdecoder/videoengine/mjpeg \
    $(PACKAGE_TOP)/media/LIBRARY/libcedarc/library/out

empty:=
space:= $(empty) $(empty)

LOCAL_BIN_LDFLAGS := $(LOCAL_LDFLAGS) \
    $(patsubst %,-L%,$(LIB_SEARCH_PATHS)) \
    -Wl,-rpath-link=$(subst $(space),:,$(strip $(LIB_SEARCH_PATHS))) \
    -Wl,-Bstatic \
    -Wl,--whole-archive \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,--no-whole-archive \
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
	-rm -rf $(OBJS) $(OBJS:%=%.d) $(LOCAL_TARGET_BIN)

#add *.h prerequisites

