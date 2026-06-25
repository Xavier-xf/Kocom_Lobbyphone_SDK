TARGET := falloc_test
SRC := main.c

$(TARGET):
	$(CC) $(CFLAGS) -DMEMOPTMZTN -g -o $@ $(SRC)

.PHONY: all clean

all: $(TARGET)

clean: rm -f *.o ${TARGET}
