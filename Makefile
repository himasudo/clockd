CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS =

LIB_SRC = lib/cycles.c lib/pointer_chase.c lib/paired.c
LIB_OBJ = lib/cycles.o lib/pointer_chase.o lib/paired.o

.PHONY: all clean

all: cycles_demo cache_sweep pingpong_demo

lib/cycles.o: lib/cycles.c lib/cycles.h
	$(CC) $(CFLAGS) -c lib/cycles.c -o lib/cycles.o

lib/pointer_chase.o: lib/pointer_chase.c lib/pointer_chase.h
	$(CC) $(CFLAGS) -c lib/pointer_chase.c -o lib/pointer_chase.o

lib/paired.o: lib/paired.c lib/paired.h lib/cycles.h
	$(CC) $(CFLAGS) -pthread -c lib/paired.c -o lib/paired.o

cycles_demo: examples/cycles_demo.c lib/cycles.o
	$(CC) $(CFLAGS) -Ilib examples/cycles_demo.c lib/cycles.o -o cycles_demo

cache_sweep: examples/cache_sweep.c $(LIB_OBJ)
	$(CC) $(CFLAGS) -Ilib examples/cache_sweep.c $(LIB_OBJ) -o cache_sweep

pingpong_demo: examples/pingpong_demo.c lib/cycles.o lib/paired.o
	$(CC) $(CFLAGS) -pthread -Ilib examples/pingpong_demo.c lib/cycles.o lib/paired.o -o pingpong_demo
	
clean:
	rm -f $(LIB_OBJ) cycles_demo cache_sweep pingpong_demo