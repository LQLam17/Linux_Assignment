server: server.c
	gcc -o server server.c -lpthread -I.

client: client.c
	gcc -o client client.c -lpthread -I.

all: server client

clean: 
	rm -f server client
