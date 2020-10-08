CC= g++
FLAGS= -std=c++11 -pthread -lcrypto -lz

TARGETS= server

all: $(TARGETS)

server:
	$(CC) server.cpp $(FLAGS) -o chatserver

clean:
	rm -f chatserver
