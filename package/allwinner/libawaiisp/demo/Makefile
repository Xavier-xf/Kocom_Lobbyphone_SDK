$(info "CC=$(CC)")
$(info "CPP=$(CPP)")
$(info "CXX=$(CXX)")
CC  ?= $(TINA_TOP)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-gcc
CPP ?= $(TINA_TOP)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-g++
CXX ?= $(TINA_TOP)/prebuilt/gcc/linux-x86/arm/toolchain-sunxi-musl/toolchain/bin/arm-openwrt-linux-muslgnueabi-g++

LIB_SEARCH_PATHS :=
ifeq ($(LIBAWAIISP_TOOLCHAIN_LIBC), musl)
    LIB_SEARCH_PATHS += ../sdk/library/musl
else ifeq ($(LIBAWAIISP_TOOLCHAIN_LIBC), glibc)
    LIB_SEARCH_PATHS += ../sdk/library/glibc
else
    $(error fatal error! unknown libc version:$(LIBAWAIISP_TOOLCHAIN_LIBC))
endif



L_LIB_SEARCH_PATHS := $(patsubst %,-L%,$(LIB_SEARCH_PATHS))

all:
	${CC} $(CFLAGS) awaiisp_demo.c confparser.c iniparser.c dictionary.c -I./ -I../sdk/include $(L_LIB_SEARCH_PATHS) \
	-Wl,-Bstatic -Wl,--start-group -lawaiisp -lawion -lawnn_full -Wl,--end-group -Wl,-Bdynamic -lpthread -lm -o awaiisp_demo

clean:
	rm -fr *.o awaiisp_demo
