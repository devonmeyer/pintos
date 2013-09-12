CC = gcc
CFLAGS = -g -I. -D FINE_LOCK #-D__RUN_WINDOW_SCRIPT__
LDFLAGS = -pthread

all:	server interface

server: server.o db.o window.o
	$(CC) $(CFLAGS) $(LDFLAGS) server.o db.o window.o \
		-o server

interface: interface.o
	$(CC) $(CFLAGS) interface.o -o interface

*.o: *.c
	$(CC) $(CFLAGS) -c $@

clean:
	/bin/rm -f *.o server interface
