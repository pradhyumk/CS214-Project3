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

char* getKeyValue(struct keyValueList* list, char* sentKey, pthread_mutex_t lock) {

    pthread_mutex_lock(&lock);

    if (list == NULL){
        return NULL;
    }

    struct keyValueList* curr = list;

    while(curr != NULL){
        
        if(strcmp(curr->key, sentKey) == 0){
            return curr->value;
        }

        curr = curr->nextPair;
    }


    pthread_mutex_unlock(&lock);

    return NULL;   
}


struct keyValueList* deleteKeyValue(struct keyValueList* list, char* sentKey, pthread_mutex_t lock) {

    pthread_mutex_lock(&lock);

    struct keyValueList* curr = list;
    struct keyValueList* prev = NULL;

    while (curr != NULL) {

        if (strcmp(curr->key, sentKey) == 0) {
            if (prev == NULL) { // first node is the one we want to delete
                free(curr->key);
                free(curr->value);
                list = list->nextPair;

            } else if (curr->nextPair == NULL) { // if its the last one
                prev->nextPair = NULL;
                free(curr->key);
                free(curr->value);
                free(curr);
            } else { // its in the middle of two nodes
                prev->nextPair = curr->nextPair;

                free(curr->key);
                free(curr->value);
                free(curr);            
            }

            return list;

        }

        prev = curr;
        curr = curr->nextPair;
    }

    pthread_mutex_unlock(&lock);
    return list;
}

struct keyValueList* setKeyValue(struct keyValueList* list, char* sentKey, char* sentValue, pthread_mutex_t lock) {
    
    pthread_mutex_lock(&lock);
    
    if (list == NULL) { // no nodes yet
        list = malloc(sizeof(struct keyValueList));
         
        list->key = malloc(sizeof(char) * strlen(sentKey) + 1);
        strcpy(list->key, sentKey);

        list->value = malloc(sizeof(char) * strlen(sentValue) + 1);
        strcpy(list->value, sentValue);

        list->nextPair = NULL;

        return list;
    }

    struct keyValueList* curr = list;
    struct keyValueList* prev = NULL;

    while (curr != NULL) {
        if (strcmp(curr->key, sentKey) == 0) { // if the key already exists
            free(curr->value);
            curr->value = malloc(sizeof(char) * strlen(sentValue) + 1);
            strcpy(curr->value, sentValue);

            return list;

        } else if (strcmp(sentKey, curr->key) < 0) {
            struct keyValueList* temp = malloc(sizeof(struct keyValueList));
         
            temp->key = malloc(sizeof(char) * strlen(sentKey) + 1);
            strcpy(temp->key, sentKey);

            temp->value = malloc(sizeof(char) * strlen(sentValue) + 1);
            strcpy(temp->value, sentValue);

            if (prev == NULL) {
                temp->nextPair = curr;
                list = temp;

                return list;
            }

            prev->nextPair = temp;
            temp->nextPair = curr;

            return list;
        } else {
            prev = curr;

            if (curr->nextPair == NULL) {
                struct keyValueList* temp = malloc(sizeof(struct keyValueList));
            
                temp->key = malloc(sizeof(char) * strlen(sentKey) + 1);
                strcpy(temp->key, sentKey);

                temp->value = malloc(sizeof(char) * strlen(sentValue) + 1);
                strcpy(temp->value, sentValue);

                temp->nextPair = NULL;

                curr->nextPair = temp;

                return list;
            }

            curr = curr->nextPair;
        }
    }

    return list;

    pthread_mutex_unlock(&lock);

    
}

void freeList(struct keyValueList* list) {
    struct keyValueList* curr = list;
    struct keyValueList* prev = NULL;

    while (curr != NULL) {
        prev = curr;
        curr = curr->nextPair;

        free(prev->key);
        free(prev->value);
        free(prev);
    }
}

void printList(struct keyValueList* list) {
    struct keyValueList* curr = list;

    while (curr != NULL) {
        printf("%s | %s => ", curr->key, curr->value);
        curr = curr->nextPair;
    } 

    printf("\n");
    return;
}


int running = 1;

struct connection {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;
    pthread_mutex_t lock;
};

int server(char *port);
void *echo(void *arg);

int main(int argc, char **argv)
{
	if (argc != 2) {
		exit(EXIT_FAILURE);
	}

    (void) server(argv[1]);
    return EXIT_SUCCESS;
}

void handler(int signal)
{
	running = 0;
}

struct keyValueList* list = NULL;

int server(char *port)
{
    struct addrinfo hint, *info_list, *info;
    struct connection *con;
    int error, sfd;
    pthread_t tid;

    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;

    error = getaddrinfo(NULL, port, &hint, &info_list);
    if (error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }

    for (info = info_list; info != NULL; info = info->ai_next) {
        sfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        
        if (sfd == -1) {
            continue;
        }

        if ((bind(sfd, info->ai_addr, info->ai_addrlen) == 0) &&
            (listen(sfd, BACKLOG) == 0)) {
            break;
        }

        close(sfd);
    }

    if (info == NULL) {
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

    pthread_mutex_t lock;

    printf("Waiting for connection\n");
	while (running) {

		con = malloc(sizeof(struct connection));
        con->addr_len = sizeof(struct sockaddr_storage);

        con->fd = accept(sfd, (struct sockaddr *) &con->addr, &con->addr_len);

        if (con->fd == -1) {
            perror("accept");
            continue;
        }
        
        error = pthread_sigmask(SIG_BLOCK, &mask, NULL);
        if (error != 0) {
        	fprintf(stderr, "sigmask: %s\n", strerror(error));
        	abort();
        }

        if (pthread_mutex_init(&lock, NULL) != 0) {
            write(con->fd, "ERR\nSRV\n", 8);

            return -1;
        }
        con->lock = lock;
        error = pthread_create(&tid, NULL, echo, con);

        if (error != 0) {
            fprintf(stderr, "Unable to create thread: %d\n", error);
            close(con->fd);
            free(con);
            continue;
        }

        pthread_detach(tid);

        error = pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
        if (error != 0) {
        	fprintf(stderr, "sigmask: %s\n", strerror(error));
        	abort();
        }

    }
    puts("No longer listening.");
	pthread_detach(pthread_self());

    if (!(list == NULL)) {
        freeList(list);
    }
    list = NULL;

    if(!(con == NULL)) {
        free(con);
    }
    con = NULL;

    pthread_mutex_destroy(&lock);

	pthread_exit(NULL);
    
    // never reach here
    return 0;
}

#define BUFSIZE 25

void *echo(void *arg)
{
    char host[100], port[10]; 
    struct connection *c = (struct connection *) arg;
    int error;

    error = getnameinfo((struct sockaddr *) &c->addr, c->addr_len, host, 100, port, 10, NI_NUMERICSERV);

    if (error != 0) {
        fprintf(stderr, "getnameinfo: %s", gai_strerror(error));
        close(c->fd);
        return NULL;
    }

    printf("[%s:%s] Connection requested\n", host, port);
    
    int sock2 = dup(c->fd);
    FILE* fin = fdopen(c->fd, "r");
    
    int cd;
    strbuff_t sb;
	sb.data = malloc(sizeof(char));
	sb.data[0] = '\0';
	sb.length = 1;
	sb.used   = 1;

    char command[4];
    int n = 0; 
    int i = 1; 
    int numCharacters = 0;
    int size;
    char* key = NULL;
    char* value = NULL;

    printf("[%s:%s] Waiting for input...\n", host, port);

    while ((cd = getc(fin)) != EOF) {
        if (cd == '\0') {
            write(sock2, "ERR\nBAD\n", 8);

            free(sb.data); 
            sb.data = malloc(sizeof(char));
            sb.data[0] = '\0';
            sb.length = 1;
            sb.used = 1;

            break;
         } else if(!isspace(cd)) { 

            if (i == 1) {
                cd = toupper(cd);
            }

            sb_append(&sb, cd);

            if (i >= 3) {
                numCharacters++;
            }
        } else if (cd == '\n') { 
            if (i == 1) { 
                if (strcmp(sb.data, "GET") == 0 || strcmp(sb.data, "DEL") == 0) {
                    strcpy(command, sb.data);
                    n = 4;
                    i++;
                    
                } else if (strcmp(sb.data, "SET") == 0) {
                    strcpy(command, sb.data);
                    n = 5;
                    i++;
                } else { 
                    write(sock2, "ERR\nBAD\n", 8);

                    free(sb.data); // reset the data string
                    sb.data = malloc(sizeof(char));
                    sb.data[0] = '\0';
                    sb.length = 1;
                    sb.used = 1;

                    break;
                }
            } else if (i == 2) { // need to save the size
                // throw error if this is not a number
                size = atoi(sb.data);

                if (size <= 0 ) {
                    n = 0;
                    i = 1;
                    numCharacters = 0;

                    write(sock2, "ERR\nLEN\n", 8);
                    
                    free(sb.data); // reset the data string
                    sb.data = malloc(sizeof(char));
                    sb.data[0] = '\0';
                    sb.length = 1;
                    sb.used = 1;
                    break;
                }       

                i++;
            } else if (i == 3) { // dealing with key
                key = malloc(sizeof(char) * sb.used + 1);
                strcpy(key, sb.data);
                i++;
                numCharacters++;
            } else if (i == 4) { // dealing with value
                value = malloc(sizeof(char) * sb.used + 1);
                strcpy(value, sb.data);
                i++;
                numCharacters++;
            }
            
            free(sb.data); // reset the data string
            sb.data = malloc(sizeof(char));
            sb.data[0] = '\0';
            sb.length = 1;
            sb.used = 1;
            
            if (n == i) { // execute the commands
                if (numCharacters != size) {
                    write(sock2, "ERR\nLEN\n", 8);

                    n = 0;
                    i = 1;
                    numCharacters = 0;
                    
                    free(sb.data); // reset the data string
                    sb.data = malloc(sizeof(char));
                    sb.data[0] = '\0';
                    sb.length = 1;
                    sb.used = 1;

                    break;
                }
                if (strcmp(command, "GET") == 0) {
                    printf("[%s:%s] G   |%s|\n", host, port, key);
                    char* val = getKeyValue(list, key, c->lock);
                    if (val == NULL) {
                        write(sock2, "KNF\n", 4);
                    } else {
                        int len = strlen(val) + 1;

                        write(sock2, "OKG\n", 4);

                        char tmp[12]={0x0};
                        sprintf(tmp,"%d", len);
                        write(sock2, tmp, sizeof(tmp));

                        write(sock2, "\n", 1);
                        write(sock2, val, strlen(val));
                        write(sock2, "\n", 1);
                        printf("[%s:%s] Waiting for input...\n", host, port);
                    }

                    free(key);
                    key = NULL;
                } else if (strcmp(command, "SET") == 0) {
                    printf("[%s:%s] S   |%s|    |%s|\n", host, port, key, value);
                    list = setKeyValue(list, key, value, c->lock);
                    free(key);
                    free(value);
                    key = NULL;
                    value = NULL;

                    //printList(list);
                    write(sock2, "OKS\n", 4);
                    printf("[%s:%s] Waiting for input...\n", host, port);
                } else if (strcmp(command, "DEL") == 0) {
                    printf("[%s:%s] D   |%s|\n", host, port, key);
                    char* val = getKeyValue(list, key, c->lock);

                    if (val == NULL) { // there is no key to delete
                        write(sock2, "KNF\n", 4);
                    } else {
                        int len = strlen(val) + 1;
                        char* temp = malloc(sizeof(char) * strlen(val) + 1);
                        strcpy(temp, val);

                        list = deleteKeyValue(list, key, c->lock);
        
                        write(sock2, "OKD\n", 4);

                        char tmp[12]={0x0};
                        sprintf(tmp,"%d", len);
                        write(sock2, tmp, sizeof(tmp));

                        write(sock2, "\n", 1);
                        write(sock2, temp, strlen(temp));
                        write(sock2, "\n", 1);

                        //printList(list);
                        free(temp);
                        printf("[%s:%s] Waiting for input...\n", host, port);
                    }

                    free(key);
                    key = NULL;
                }
                

                i = 1;
                numCharacters = 0;
            }
        }
    
    }
  
    printf("[%s:%s] got EOF\n", host, port);

    if (!(key == NULL)) {
        free(key);
    }

    if (!(value == NULL)) {
        free(value);
    }

    fclose(fin);
    free(sb.data);
    close(c->fd);
    free(c);
    return NULL;
}