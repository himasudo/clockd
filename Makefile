CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS =

LIB_SRC = lib/cycles.c lib/pointer_chase.c
LIB_OBJ = lib/cycles.o lib/pointer_chase.o

.PHONY: all clean

all: cycles_demo cache_sweep

lib/cycles.o: lib/cycles.c lib/cycles.h
	$(CC) $(CFLAGS) -c lib/cycles.c -o lib/cycles.o

lib/pointer_chase.o: lib/pointer_chase.c lib/pointer_chase.h
	$(CC) $(CFLAGS) -c lib/pointer_chase.c -o lib/pointer_chase.o

cycles_demo: examples/cycles_demo.c lib/cycles.o
	$(CC) $(CFLAGS) -Ilib examples/cycles_demo.c lib/cycles.o -o cycles_demo

cache_sweep: examples/cache_sweep.c $(LIB_OBJ)
	$(CC) $(CFLAGS) -Ilib examples/cache_sweep.c $(LIB_OBJ) -o cache_sweep

clean:
	rm -f $(LIB_OBJ) cycles_demo cache_sweep