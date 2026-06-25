LOCAL_PATH := $(shell dirname $(lastword $(MAKEFILE_LIST)))
SDK_DIR = $(LOCAL_PATH)/sdk

VIVANTE_SDK_DIR ?= $(SDK_DIR)

INCLUDE += -I$(VIVANTE_SDK_DIR)/include

CFLAGS += $(INCLUDE)

CC:=arm-openwrt-linux-gcc

################################################################################
# Supply necessary libraries.
LIBS += -L$(VIVANTE_SDK_DIR)/drivers -Wl,-rpath-link,$(VIVANTE_SDK_DIR)/drivers

#############################################################################

SOURCE = ${wildcard *.c}

TARGET_NAME = lenet

# installation directory
OUT_DIR ?= ./bin
################################################################################

target = $(OUT_DIR)/$(TARGET_NAME)
$(target):
	@mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -g -o $@ $(SOURCE) $(LIBS) -l VIPlite

.PHONY: clean
clean:
	@rm -rf *.o
	@rm -rf $(target)
	@rm -rf *~
