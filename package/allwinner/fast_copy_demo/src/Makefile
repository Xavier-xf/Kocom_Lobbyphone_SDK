target			= fast_copy_demo

INCLUDES        += -I. -I./include
#STATIC_LIB		:= 
DYNAMIC_LIB		:= -lm -lc
SRCDIRS = $(shell find . -maxdepth 5 -type d)

SRCS := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))
SRCS += $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
OBJS = $(SRCS:.c=.o)
LOCAL_LDFLAGS := -lpthread
ifdef STATIC_LIB
	LOCAL_LDFLAGS += -L ./lib/static -Wl,-Bstatic -Xlinker "-(" $(STATIC_LIB) -Xlinker "-)"
endif

ifdef DYNAMIC_LIB
	LOCAL_LDFLAGS += -L ./lib/dynamic -Wl,-Bdynamic $(DYNAMIC_LIB)
else
	LOCAL_LDFLAGS += -static 
endif

%.o: %.c
	@echo $(CFLAGS)
	$(CC) $(INCLUDES) $(CFLAGS) $(LOCAL_CFLAGS) -c -o $@ $<

$(target): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) $(LOCAL_LDFLAGS)

all:install
install:$(target)

clean:
	rm -rf $(target) $(OBJS)
