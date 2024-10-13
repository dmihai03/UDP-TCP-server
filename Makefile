CC = g++

CFLAGS = -Wall -g -Wno-unused-variable

all: server subscriber

common.o: common.cpp

server: server.cpp common.o

subscriber: subscriber.cpp common.o

.phony: clean

clean:
	rm -f *.o server subscriber *.log