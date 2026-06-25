
obj := test.o constants.o

test: ${obj}
	gcc -o $@ ${obj}

%.o: %.c
	gcc -std=c99 -c $^

clean:
	rm -rf *.o test *.txt
