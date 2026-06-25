TARGET := fsck_msdos
SRC := main.c boot.c check.c dir.c fat.c

$(TARGET):
	$(CC) $(CFLAGS) -DMEMOPTMZTN -g -o $@ $(SRC)

.PHONY: all clean

all: $(TARGET)

clean: rm -f *.o ${TARGET}
