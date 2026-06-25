EXTRA_CFLAGS += -I$(src)/../cedar-ve/codec/lib/include \
				-I$(src)/../cedar-ve/include \
				-I$(src)/../cedar-ve/codec/vencoder \
				-I$(src)/../cedar-ve/codec/base \
				-I$(src)/../cedar-ve/codec/memory \
				-I$(src)/../cedar-ve/codec/ve \
				-I$(src)/../platform/sunxi-vin \
				-I$(src)/../../../include/uapi/media/rt-media \
				-I$(src)/../../../include/uapi \
				-I$(src)/../../../include/uapi/media
EXTRA_CFLAGS += -Wno-undef -Wno-declaration-after-statement -Wno-unused-variable -Wno-unused-function -Wno-unused-label

obj-$(CONFIG_VIDEO_RT_MEDIA) += rt-media.o

rt-media-objs := rt_media.o
rt-media-objs += component/rt_message.o
rt-media-objs += component/rt_semaphore.o
rt-media-objs += component/rt_component.o
rt-media-objs += component/rt_venc_component.o
rt-media-objs += component/rt_vi_component.o
ifeq ($(CONFIG_RT_MEDIA_SUPPORT_RPMSG),y)
rt-media-objs += component/rt_rp.o
endif
rt-media-objs += ../cedar-ve/codec/lib/libvencoder
rt-media-objs += ../cedar-ve/codec/lib/libbase
rt-media-objs += ../cedar-ve/codec/lib/libve
rt-media-objs += ../cedar-ve/codec/lib/libgcc
ifeq (${CONFIG_VIDEO_RT_MEDIA},m)
rt-media-objs += ./libmath
endif

