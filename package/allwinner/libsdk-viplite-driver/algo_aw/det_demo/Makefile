$(info "CC=$(CC)")
$(info "CXX=$(CXX)")
$(info "CPP=$(CPP)")
$(info "CFLAGS=$(CFLAGS)")
$(info "CXXFLAGS=$(CXXFLAGS)")
$(info "CPPFLAGS=$(CPPFLAGS)")
$(info "LDFLAGS=$(LDFLAGS)")
$(info "CONFIG_PREFIX=$(CONFIG_PREFIX)")
$(info "ARCH=$(ARCH)")
$(info "AR=$(AR)")

CC  ?= $(TINA_TOP)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-gcc
CPP ?= $(TINA_TOP)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-g++
CXX ?= $(TINA_TOP)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-g++

LIB_SEARCH_PATHS :=
ifeq ($(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC), musl)
    LIB_SEARCH_PATHS += ./../../algo_aw/library/musl ./../../sdk_release/library/musl
else ifeq ($(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC), glibc)
    LIB_SEARCH_PATHS += ./../../algo_aw/library/glibc ./../../sdk_release/library/glibc
else
    $(error fatal error! unknown libc version:$(LIBSDK_VIPLITE_DRIVER_TOOLCHAIN_LIBC))
endif

L_LIB_SEARCH_PATHS := $(patsubst %,-L%,$(LIB_SEARCH_PATHS))

all:
	${CXX} demo.cc list.c aw_errors.c -I./ -I./../../algo_aw/include $(L_LIB_SEARCH_PATHS) -l:libawnn.a -l:libVIPlite.a \
		-l:libVIPuser.a -lpthread -o awnn_det_demo

clean:
	rm -fr *.o awnn_det_demo
