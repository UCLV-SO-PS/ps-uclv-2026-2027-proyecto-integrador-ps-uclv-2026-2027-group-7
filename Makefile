# ============================================================
#  Makefile — Proyecto Final Programación de Sistemas 2026-2027
#  Servidor Concurrente TCP de Calificaciones IMDb
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -g -I.
LDFLAGS =
AR      = ar
ARFLAGS = rcs

BIN     = server client
LIB     = libmonitor.a
OBJS    = monitor.o

.PHONY: all clean run-test

all: $(LIB) $(BIN)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

monitor.o: monitor.c monitor.h
	$(CC) $(CFLAGS) -c $< -o $@

server: server.c monitor.h $(LIB)
	$(CC) $(CFLAGS) server.c -L. -lmonitor $(LDFLAGS) -o $@

client: client.c
	$(CC) $(CFLAGS) client.c $(LDFLAGS) -o $@

run-test: server
	./server --test-run

clean:
	rm -f *.o $(LIB) $(BIN)