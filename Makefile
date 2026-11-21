# ============================================================
#  Makefile — Proyecto Final Programación de Sistemas 2026-2027
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -g
AR      = ar
ARFLAGS = rcs

BIN     = server client
LIB     = libmonitor.a
OBJS    = monitor.o

.PHONY: all clean

all: $(LIB) $(BIN)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

monitor.o: monitor.c monitor.h
	$(CC) $(CFLAGS) -c $< -o $@

server: server.c $(LIB)
	$(CC) $(CFLAGS) server.c -L. -lmonitor -o $@

client: client.c
	$(CC) $(CFLAGS) client.c -o $@

clean:
	rm -f *.o $(LIB) $(BIN)