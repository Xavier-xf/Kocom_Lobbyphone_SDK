# Makefile for eyesee-mpp/middleware/media/LIBRARY/libisp/isp_cfg
CUR_PATH := .
PACKAGE_TOP := $(PACKAGE_TOP)
EYESEE_MPP_INCLUDE:=$(STAGING_DIR)/usr/include/eyesee-mpp
EYESEE_MPP_LIBDIR:=$(STAGING_DIR)/usr/lib/eyesee-mpp
# STAGING_DIR is exported in rules.mk, so it can be used directly here.
# STAGING_DIR:=.../tina-v316/out/v316-perfnor/staging_dir/target

include $(PACKAGE_TOP)/config/mpp_config.mk

#set source files here.
SRCCS := \
    $(wildcard ./*.c)

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(EYESEE_MPP_INCLUDE)/system/public/include \
    $(EYESEE_MPP_INCLUDE)/system/public/include/utils \
    $(CUR_PATH)/SENSOR_H \
    $(CUR_PATH)/../iniparser/src/ \
    $(CUR_PATH)/../include/ \
    $(CUR_PATH)/../isp_dev/ \
    $(CUR_PATH)/../isp_math/zlib/include

LOCAL_SHARED_LIBS :=

LOCAL_STATIC_LIBS :=

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC :=
LOCAL_TARGET_STATIC := libisp_ini
LOCAL_TARGET_BIN :=

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) \
                $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(CEDARX_EXT_CFLAGS) $(inc_paths) -fPIC -Wall
ifeq ($(filter imx386,$(SENSOR_NAME)), imx386)
    LOCAL_CFLAGS += -DSENSOR_IMX386=1
    LOCAL_CXXFLAGS += -DSENSOR_IMX386=1
endif
ifeq ($(filter gc4663,$(SENSOR_NAME)), gc4663)
    LOCAL_CFLAGS += -DSENSOR_GC4663=1
    LOCAL_CXXFLAGS += -DSENSOR_GC4663=1
endif
ifeq ($(filter gc1084,$(SENSOR_NAME)), gc1084)
    LOCAL_CFLAGS += -DSENSOR_GC1084=1
    LOCAL_CXXFLAGS += -DSENSOR_GC1084=1
ifeq ($(filter gc1084_8bit,$(SENSOR_NAME)), gc1084_8bit)
    LOCAL_CFLAGS += -DSENSOR_GC1084_8BIT=1
    LOCAL_CXXFLAGS += -DSENSOR_GC1084_8BIT=1
endif
endif
ifeq ($(filter gc2053,$(SENSOR_NAME)), gc2053)
    LOCAL_CFLAGS += -DSENSOR_GC2053=1
    LOCAL_CXXFLAGS += -DSENSOR_GC2053=1
ifeq ($(filter gc2053_8bit,$(SENSOR_NAME)), gc2053_8bit)
    LOCAL_CFLAGS += -DSENSOR_GC2053_8BIT=1
    LOCAL_CXXFLAGS += -DSENSOR_GC2053_8BIT=1
endif
endif

ifeq ($(filter gc0406,$(SENSOR_NAME)), gc0406)
    LOCAL_CFLAGS += -DSENSOR_GC0406=1
    LOCAL_CXXFLAGS += -DSENSOR_GC0406=1
endif
ifeq ($(filter gc2083,$(SENSOR_NAME)), gc2083)
    LOCAL_CFLAGS += -DSENSOR_GC2083=1
    LOCAL_CXXFLAGS += -DSENSOR_GC2083=1
endif
ifeq ($(filter f37p,$(SENSOR_NAME)), f37p)
    LOCAL_CFLAGS += -DSENSOR_F37P=1
    LOCAL_CXXFLAGS += -DSENSOR_F37P=1
endif
ifeq ($(filter f355p,$(SENSOR_NAME)), f355p)
    LOCAL_CFLAGS += -DSENSOR_F355P=1
    LOCAL_CXXFLAGS += -DSENSOR_F355P=1
endif
ifeq ($(filter mis2008,$(SENSOR_NAME)), mis2008)
    LOCAL_CFLAGS += -DSENSOR_MIS2008=1
    LOCAL_CXXFLAGS += -DSENSOR_MIS2008=1
endif
ifeq ($(filter sc1346,$(SENSOR_NAME)), sc1346)
    LOCAL_CFLAGS += -DSENSOR_SC1346=1
    LOCAL_CXXFLAGS += -DSENSOR_SC1346=1
endif
ifeq ($(filter sc2336,$(SENSOR_NAME)), sc2336)
    LOCAL_CFLAGS += -DSENSOR_sc2336=1
    LOCAL_CXXFLAGS += -DSENSOR_sc2336=1
endif
ifeq ($(filter sc2355,$(SENSOR_NAME)), sc2355)
    LOCAL_CFLAGS += -DSENSOR_sc2355=1
    LOCAL_CXXFLAGS += -DSENSOR_sc2355=1
endif
ifeq ($(filter sc3336,$(SENSOR_NAME)), sc3336)
    LOCAL_CFLAGS += -DSENSOR_SC3336=1
    LOCAL_CXXFLAGS += -DSENSOR_SC3336=1
endif
ifeq ($(filter sc3336p,$(SENSOR_NAME)), sc3336p)
    LOCAL_CFLAGS += -DSENSOR_SC3336P=1
    LOCAL_CXXFLAGS += -DSENSOR_SC3336P=1
endif
ifeq ($(filter sc4336,$(SENSOR_NAME)), sc4336)
    LOCAL_CFLAGS += -DSENSOR_SC4336=1
    LOCAL_CXXFLAGS += -DSENSOR_SC4336=1
endif
ifeq ($(filter sc4336p,$(SENSOR_NAME)), sc4336p)
    LOCAL_CFLAGS += -DSENSOR_SC4336P=1
    LOCAL_CXXFLAGS += -DSENSOR_SC433P=1
endif
ifeq ($(filter sc5336,$(SENSOR_NAME)), sc5336)
    LOCAL_CFLAGS += -DSENSOR_SC5336=1
    LOCAL_CXXFLAGS += -DSENSOR_SC5336=1
endif
ifeq ($(filter sc530ai,$(SENSOR_NAME)), sc530ai)
    LOCAL_CFLAGS += -DSENSOR_SC530AI=1
    LOCAL_CXXFLAGS += -DSENSOR_SC530AI=1
endif
ifeq ($(filter os02g10,$(SENSOR_NAME)), os02g10)
    LOCAL_CFLAGS += -DSENSOR_OS02G10=1
    LOCAL_CXXFLAGS += -DSENSOR_OS02G10=1
endif
ifeq ($(filter bf2257cs,$(SENSOR_NAME)), bf2257cs)
    LOCAL_CFLAGS += -DSENSOR_BF2257CS=1
    LOCAL_CXXFLAGS += -DSENSOR_BF2257CS=1
endif
ifeq ($(filter sc202cs,$(SENSOR_NAME)), sc202cs)
    LOCAL_CFLAGS += -DSENSOR_SC202CS=1
    LOCAL_CXXFLAGS += -DSENSOR_SC202CS=1
endif
ifeq ($(filter sc2336p,$(SENSOR_NAME)), sc2336p)
    LOCAL_CFLAGS += -DSENSOR_SC2336P=1
    LOCAL_CXXFLAGS += -DSENSOR_SC2336P=1
endif
ifeq ($(filter sc200ai,$(SENSOR_NAME)), sc200ai)
    LOCAL_CFLAGS += -DSENSOR_SC200AI=1
    LOCAL_CXXFLAGS += -DSENSOR_SC200AI=1
ifeq ($(filter sc200ai_8bit,$(SENSOR_NAME)), sc200ai_8bit)
    LOCAL_CFLAGS += -DSENSOR_SC200AI_8BIT=1
    LOCAL_CXXFLAGS += -DSENSOR_SC200AI_8BIT=1
endif
endif
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LOCAL_DYNAMIC_LDFLAGS := $(LOCAL_LDFLAGS) -shared \
    -L $(EYESEE_MPP_LIBDIR) \
    -Wl,-Bstatic \
    -Wl,--start-group $(foreach n, $(LOCAL_STATIC_LIBS), -l$(patsubst lib%,%,$(patsubst %.a,%,$(notdir $(n))))) -Wl,--end-group \
    -Wl,-Bdynamic \
    $(foreach y, $(LOCAL_SHARED_LIBS), -l$(patsubst lib%,%,$(patsubst %.so,%,$(notdir $(y)))))

#generate object files
OBJS := $(SRCCS:%=%.o) #OBJS=$(patsubst %,%.o,$(SRCCS))

#add dynamic lib name suffix and static lib name suffix.
target_dynamic := $(if $(LOCAL_TARGET_DYNAMIC),$(LOCAL_TARGET_DYNAMIC).so,)
target_static := $(if $(LOCAL_TARGET_STATIC),$(LOCAL_TARGET_STATIC).a,)

#generate exe file.
.PHONY: all
all: $(target_dynamic) $(target_static)
	@echo ===================================
	@echo build eyesee-mpp-middleware-media-LIBRARY-libisp-isp_cfg done
	@echo ===================================

$(target_dynamic): $(OBJS)
	$(CC) $+ $(LOCAL_DYNAMIC_LDFLAGS) -o $@
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

$(target_static): $(OBJS)
	$(AR) -rcs -o $@ $+
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

#patten rules to generate local object files
%.cpp.o: %.cpp
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(patsubst %,%.d,$@) -c -o $@ $<
%.cc.o: %.cc
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(patsubst %,%.d,$@) -c -o $@ $<

%.c.o: %.c
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -MD -MP -MF $(patsubst %,%.d,$@) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -f $(OBJS) $(OBJS:%=%.d) $(target_dynamic) $(target_static)

#add *.h prerequisites
-include $(OBJS:%=%.d)

