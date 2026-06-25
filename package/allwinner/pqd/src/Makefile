TARGET := pqd
SRC := pqdaemon.c pq_core.c
SRC += utils/utils.c utils/debug.c utils/cmdqueue.c utils/helper.c
SRC += pqdata/pqdata.c
SRC += sockets/socket.c sockets/socket_client.c
SRC += hardwares/hardwares.c hardwares/de20x/de20x.c
SRC += hardwares/de20x/fcm/fcm.c hardwares/de20x/fcm/constants.c

INCLUDE := -I./utils/
INCLUDE += -I./sockets/
INCLUDE += -I./pqdata/
INCLUDE += -I./hardwares/
INCLUDE += -I./hardwares/de20x/
INCLUDE += -I./hardwares/de20x/fcm/
INCLUDE += -I./

objs = $(SRC:%.c=%.o)

CFLAGS += -std=gnu99 -Wall -Wno-unused-function
CFLAGS += -mfloat-abi=softfp -mfpu=vfpv3
CFLAGS += -I./ -Iutils -g

LDFLAGS += -lpthread -lm

$(TARGET): ${objs}
	$(CC) $(LDFLAGS) $+ -o $@

 %.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE)  -g -c -o  $@ $<

#$(TARGET):
#	$(CC) $(CFLAGS) $(INCLUDE) -g -o $@ $(SRC)

.PHONY:  clean

all: $(TARGET)

clean:
	$(RM) ${TARGET} $(objs)
