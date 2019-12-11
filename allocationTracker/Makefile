CFLAGS=-Wall -g -O3
LDFLAGS=-lnuma

.PHONY: all clean
all: ldlib.so

ldlib.so: ldlib.c
	g++ -fPIC ${CFLAGS} -c ldlib.c
	g++ -shared -Wl,-soname,libpmalloc.so -o ldlib.so ldlib.o -ldl -lpthread

clean:
	rm -f *.o *.so

