#Makefile
CC = gcc
CFLAGS = -Wall -g -O0
OBJS = config.o response.o

webserver: $(OBJS) webserver.c
	$(CC) $(CFLAGS) $(OBJS) webserver.c -o webserver

config.o: config.c config.h
	$(CC) $(CFLAGS) -c config.c -o config.o

response.o: response.c response.h http_codes.h
	$(CC) $(CFLAGS) -c response.c -o response.o


all: webserver
.PHONY: all

clean:
	rm -rf *.o webserver core
.PHONY: clean