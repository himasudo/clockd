CC      = gcc
CFLAGS  = -Wall -Wextra -O2
LDFLAGS =

LIB_SRC = lib/cycles.c
LIB_OBJ = lib/cycles.o

.PHONY: all clean

all: cycles_demo

$(LIB_OBJ): lib/cycles.c lib/cycles.h
	$(CC) $(CFLAGS) -c lib/cycles.c -o lib/cycles.o

cycles_demo: examples/cycles_demo.c $(LIB_OBJ)
	$(CC) $(CFLAGS) -Ilib examples/cycles_demo.c $(LIB_OBJ) -o cycles_demo

clean:
	rm -f $(LIB_OBJ) cycles_demo