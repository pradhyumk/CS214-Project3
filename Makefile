all: server send sendrev

server: server.c
	gcc -g -std=gnu99 -Wvla -Wall -pthread -fsanitize=address,undefined server.c -o server.out

send: send.c
	gcc -g -std=gnu99 -Wvla -Wall -pthread -fsanitize=address,undefined send.c -o send.out

sendrev: sendrev.c
	gcc -g -std=gnu99 -Wvla -Wall -pthread -fsanitize=address,undefined server.c -o sendrev.out

clean:
	rm -f server.out send.out sendrev.out
