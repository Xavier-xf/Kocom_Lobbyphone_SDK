$(info "CC=$(CC)")
$(info "CPP=$(CPP)")
$(info "CXX=$(CXX)")
CC  ?= $(TINA_TOP)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-gcc
CPP ?= $(TINA_TOP)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-g++
CXX ?= $(TINA_TOP)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-g++

LIB_SEARCH_PATHS :=
ifeq ($(LIBAWNN_FULL_TOOLCHAIN_LIBC), musl)
    LIB_SEARCH_PATHS += ../sdk/library/glibc
else ifeq ($(LIBAWNN_FULL_TOOLCHAIN_LIBC), glibc)
    LIB_SEARCH_PATHS += ../sdk/library/musl
endif

L_LIB_SEARCH_PATHS := $(patsubst %,-L%,$(LIB_SEARCH_PATHS))

all:
	${CXX} demo.cc list.c aw_errors.c -I./ -I../sdk/include $(L_LIB_SEARCH_PATHS) -l:libawnn_full.a -lpthread -o awnn_det_demo

clean:
	rm -fr *.o awnn_det_demo
