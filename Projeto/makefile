all: server client proxy

server: server.c
	gcc server.c -D_REENTRANT -Wall -o server

client: client.c
	gcc client.c -D_REENTRANT -Wall -o client

proxy: proxy.c
	gcc proxy.c -D_REENTRANT -Wall -o proxy -pthread
