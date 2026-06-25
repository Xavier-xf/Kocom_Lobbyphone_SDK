# Makefile for eyesee-mpp/system/public/liblog
CUR_PATH := .

CUR_INSTALLDEV_DIR := InstallDev
CUR_INSTALL_DIR := install

#set source files here.
SRCCS :=
ifeq ($(CONFIG_glogCWrapper_use_glog),y)
SRCCS += \
    glog_helper.cpp \
    log_print.cpp
endif
ifeq ($(CONFIG_glogCWrapper_use_printf),y)
SRCCS += \
    log_print.c
endif

#include directories
INCLUDE_DIRS := \
    $(CUR_PATH) \
    $(CUR_PATH)/include

LOCAL_SHARED_LIBS :=
ifeq ($(CONFIG_glogCWrapper_use_glog),y)
LOCAL_SHARED_LIBS += \
    libglog
endif

LOCAL_STATIC_LIBS :=

LIB_SEARCH_PATHS := $(STAGING_DIR)/usr/lib

#set dst file name: shared library, static library, execute bin.
LOCAL_TARGET_DYNAMIC := liblog
LOCAL_TARGET_STATIC := liblog
LOCAL_TARGET_BIN :=

#generate include directory flags for gcc.
inc_paths := $(foreach inc,$(filter-out -I%,$(INCLUDE_DIRS)),$(addprefix -I, $(inc))) $(filter -I%, $(INCLUDE_DIRS))
#Extra flags to give to the C compiler
LOCAL_CFLAGS := $(CFLAGS) $(inc_paths) -fPIC -Wall
#Extra flags to give to the C++ compiler
LOCAL_CXXFLAGS := $(CXXFLAGS) $(inc_paths) -fPIC -Wall
#Extra flags to give to the C preprocessor and programs that use it (the C and Fortran compilers).
LOCAL_CPPFLAGS := $(CPPFLAGS)
#target device arch: x86, arm
LOCAL_TARGET_ARCH := $(ARCH)
#Extra flags to give to compilers when they are supposed to invoke the linker,‘ld’.
LOCAL_LDFLAGS := $(LDFLAGS)

LOCAL_DYNAMIC_LDFLAGS := $(LOCAL_LDFLAGS) -shared \
    $(patsubst %,-L%,$(LIB_SEARCH_PATHS)) \
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
all: $(target_dynamic) $(target_static) InstallDev install
	@echo ===================================
	@echo build libglogCWrapper done
	@echo ===================================
$(target_dynamic): $(OBJS)
	$(CC) $^ $(LOCAL_DYNAMIC_LDFLAGS) -o $@
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

$(target_static): $(OBJS)
	$(AR) -rcs -o $@ $^
	@echo ----------------------------
	@echo "finish target: $@"
#	@echo "object files:  $+"
#	@echo "source files:  $(SRCCS)"
	@echo ----------------------------

InstallDev:
	@echo "install dev of libglogCWrapper"
	mkdir -p $(CUR_INSTALLDEV_DIR)/usr/lib
	mkdir -p $(CUR_INSTALLDEV_DIR)/usr/include/glogCWrapper
	find $(CUR_PATH) \( -name "liblog.a" -o -name "liblog.so" \) -exec cp {} $(CUR_INSTALLDEV_DIR)/usr/lib \;
	cp $(CUR_PATH)/include/*.h $(CUR_INSTALLDEV_DIR)/usr/include/glogCWrapper

install:
	@echo "install of libglogCWrapper"
ifeq ($(CONFIG_PACKAGE_rt_media),y)
	mkdir -p $(CUR_INSTALL_DIR)/lib
	if [ -f $(CUR_PATH)/liblog.so ]; then \
		cp -p $(CUR_PATH)/liblog.so $(CUR_INSTALL_DIR)/lib; \
	fi
else
	mkdir -p $(CUR_INSTALL_DIR)/usr/lib
	if [ -f $(CUR_PATH)/liblog.so ]; then \
		cp -p $(CUR_PATH)/liblog.so $(CUR_INSTALL_DIR)/usr/lib; \
	fi
endif

#patten rules to generate local object files
%.cpp.o: %.cpp
	$(CXX) $(LOCAL_CXXFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<

%.c.o: %.c
	$(CC) $(LOCAL_CFLAGS) $(LOCAL_CPPFLAGS) -c -o $@ $<

# clean all
.PHONY: clean
clean:
	-rm -f $(OBJS) $(target_dynamic) $(target_static)
	-rm -rf $(CUR_INSTALLDEV_DIR) $(CUR_INSTALL_DIR)
