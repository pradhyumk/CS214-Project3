#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <ctype.h>
#include <pthread.h>

#define BACKLOG 5

typedef struct {
    size_t length;
	size_t used;
    char *data;
} strbuff_t;

void sb_destroy(char *L) {
    free(L);
}

int sb_append(strbuff_t *sb, char item)
{
    if (sb->used == sb->length) {
	   size_t size = sb->length * 2;
	   char *p = realloc(sb->data, sizeof(char) * size);
	   if (!p) return 1;

	   sb->data = p;
	   sb->length = size;
    }

    sb->data[sb->used] = sb->data[sb->used-1];
    sb->data[sb->used-1] = item;

    ++sb->used;
    
    return 0;
}

struct keyValueList {
    char* key;
    char* value;
    struct keyValueList* nextPair;
};

// void insertKeyValue(struct keyValueList, char* key, char* value) {

// }


int running = 1;

// the argument we will pass to the connection-handler threads
struct connection {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;
};

int server(char *port);
void *echo(void *arg);

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("Usage: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

    (void) server(argv[1]);
    return EXIT_SUCCESS;
}

void handler(int signal)
{
	running = 0;
}


int server(char *port)
{
    struct addrinfo hint, *info_list, *info;
    struct connection *con;
    int error, sfd;
    pthread_t tid;

    // initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;
    	// setting AI_PASSIVE means that we want to create a listening socket

    // get socket and address info for listening port
    // - for a listening socket, give NULL as the host name (because the socket is on
    //   the local host)
    error = getaddrinfo(NULL, port, &hint, &info_list);
    if (error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }

    // attempt to create socket
    for (info = info_list; info != NULL; info = info->ai_next) {
        sfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        
        // if we couldn't create the socket, try the next method
        if (sfd == -1) {
            continue;
        }

        // if we were able to create the socket, try to set it up for
        // incoming connections;
        // 
        // note that this requires two steps:
        // - bind associates the socket with the specified port on the local host
        // - listen sets up a queue for incoming connections and allows us to use accept
        if ((bind(sfd, info->ai_addr, info->ai_addrlen) == 0) &&
            (listen(sfd, BACKLOG) == 0)) {
            break;
        }

        // unable to set it up, so try the next method
        close(sfd);
    }

    if (info == NULL) {
        // we reached the end of result without successfuly binding a socket
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(info_list);

	struct sigaction act;
	act.sa_handler = handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);
	
	sigset_t mask;
	
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);


    // at this point sfd is bound and listening
    printf("Waiting for connection\n");
	while (running) {
    	// create argument struct for child thread
		con = malloc(sizeof(struct connection));
        con->addr_len = sizeof(struct sockaddr_storage);
        	// addr_len is a read/write parameter to accept
        	// we set the initial value, saying how much space is available
        	// after the call to accept, this field will contain the actual address length
        
        // wait for an incoming connection
        con->fd = accept(sfd, (struct sockaddr *) &con->addr, &con->addr_len);
        	// we provide
        	// sfd - the listening socket
        	// &con->addr - a location to write the address of the remote host
        	// &con->addr_len - a location to write the length of the address
        	//
        	// accept will block until a remote host tries to connect
        	// it returns a new socket that can be used to communicate with the remote
        	// host, and writes the address of the remote hist into the provided location
        
        // if we got back -1, it means something went wrong
        if (con->fd == -1) {
            perror("accept");
            continue;
        }
        
        // temporarily block SIGINT (child will inherit mask)
        error = pthread_sigmask(SIG_BLOCK, &mask, NULL);
        if (error != 0) {
        	fprintf(stderr, "sigmask: %s\n", strerror(error));
        	abort();
        }

		// spin off a worker thread to handle the remote connection
        error = pthread_create(&tid, NULL, echo, con);

		// if we couldn't spin off the thread, clean up and wait for another connection
        if (error != 0) {
            fprintf(stderr, "Unable to create thread: %d\n", error);
            close(con->fd);
            free(con);
            continue;
        }

		// otherwise, detach the thread and wait for the next connection request
        pthread_detach(tid);

        // unblock SIGINT
        error = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        if (error != 0) {
        	fprintf(stderr, "sigmask: %s\n", strerror(error));
        	abort();
        }

    }

	puts("No longer listening.");
	pthread_detach(pthread_self());
	pthread_exit(NULL);

    // never reach here
    return 0;
}

#define BUFSIZE 25

void *echo(void *arg)
{
    char host[100], port[10], buf[BUFSIZE + 1];
    struct connection *c = (struct connection *) arg;
    int error, nread;

	// find out the name and port of the remote host
    error = getnameinfo((struct sockaddr *) &c->addr, c->addr_len, host, 100, port, 10, NI_NUMERICSERV);
    	// we provide:
    	// the address and its length
    	// a buffer to write the host name, and its length
    	// a buffer to write the port (as a string), and its length
    	// flags, in this case saying that we want the port as a number, not a service name
    if (error != 0) {
        fprintf(stderr, "getnameinfo: %s", gai_strerror(error));
        close(c->fd);
        return NULL;
    }

    printf("[%s:%s] connection\n", host, port);

    int sock2 = dup(c->fd);
    FILE* fin = fdopen(c->fd, "r");
    FILE* fout = fdopen(sock2, "w");
    
    int cd;
    strbuff_t sb;
	sb.data = malloc(sizeof(char));
	sb.data[0] = '\0';
	sb.length = 1;
	sb.used   = 1;

    char command[4];
    int n = 0; // number of payloads
    int i = 1; // which payload we are on.
    int numCharacters = 0;
    int size;
    char* key;
    char* value;

    while ((cd = getc(fin)) != EOF) {
        printf("Character: %c\n", cd);



        if(!isspace(cd)) { // if its a character
            sb_append(&sb, cd);

            if (i >= 3) {
                numCharacters++;
            }
        } else if (cd == '\n') { // now we add this to the word
            if (i == 1) { //first payload (command)
                if (sb.used != 3) { // the index of null terminator is not 
                    // throw error to socket saying invalid command
                } // SET\n8\nabc\nxyz\n

                if (strcmp(sb.data, "GET") == 0 || strcmp(sb.data, "DEL") == 0) {
                    strcpy(command, sb.data);
                    n = 4;
                    i++;
                    
                } else if (strcmp(sb.data, "SET") == 0) {
                    strcpy(command, sb.data);
                    n = 5;
                    i++;
                }
                printf("Command: %s\n", command);
            } else if (i == 2) { // need to save the size
                // throw error if this is not a number
                size = atoi(sb.data);
                i++;
                printf("Size: %d\n", size);
            } else if (i == 3) { // dealing with key
                key = malloc(sizeof(char) * sb.used + 1);
                strcpy(key, sb.data);
                i++;
                printf("Key: %s\n", key);
            } else if (i == 4) { // dealing with value
                value = malloc(sizeof(char) * sb.used + 1);
                strcpy(value, sb.data);
                i++;
                printf("Value: %s\n", value);
            }
            
            free(sb.data); // reset the data string
            sb.data = malloc(sizeof(char));
            sb.data[0] = '\0';
            sb.length = 1;
            sb.used = 1;
            
            printf("n: %d | i: %d\n", n, i);
            
            if (n == i) { // execute the commands
                
                if (numCharacters > size){
                    printf("Payload is greater than provided size!");
                }

                printf("Executing commands and resetting\n");

                if (strcmp(command, "GET") == 0) {
                    
                } else if (strcmp(command, "SET") == 0) {

                } else if (strcmp(command, "DEL") == 0) {
                    
                }

                i = 1;
                numCharacters = 0;
            }
        }


    
    }
  
    printf("[%s:%s] got EOF\n", host, port);

    close(c->fd);
    free(c);
    return NULL;
}
