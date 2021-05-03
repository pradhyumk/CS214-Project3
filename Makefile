all: server

server: server.c
	gcc -g -std=gnu99 -Wvla -Wall -pthread -fsanitize=address,undefined server.c -o server.out

clean:
	rm -f server.out send.out sendrev.out
