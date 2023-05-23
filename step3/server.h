#include <arpa/inet.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERROR "\033[31m[Error]\033[0m "

#define BUFSIZE 1024

struct clientitem;

struct clientitem *client_init(int connfd);
int serve(int fd, char *buf, int len, struct clientitem *cli);

typedef struct {               // Represents a pool of connected descriptors
    int maxfd;                 // Largest descriptor in read_set
    fd_set read_set;           // Set of all active descriptors
    fd_set ready_set;          // Subset of descriptors ready for reading
    int nready;                // Number of ready descriptors from select
    int maxi;                  // High water index into client array
    int clientfd[FD_SETSIZE];  // Set of active descriptors
    struct clientitem *client[FD_SETSIZE];
} pool;

void init_pool(int listenfd, pool *p) {
    p->maxi = 1;
    for (int i = 0; i < FD_SETSIZE; i++) p->clientfd[i] = -1;

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_clients(int connfd, pool *p) {
    int i;
    p->nready--;
    for (i = 0; i < FD_SETSIZE; i++) {
        if (p->clientfd[i] < 0) {
            p->clientfd[i] = connfd;
            p->client[i] = client_init(connfd);
            printf("New client: %d\n", connfd);
            FD_SET(connfd, &p->read_set);
            if (connfd > p->maxfd) p->maxfd = connfd;
            if (i > p->maxi) p->maxi = i;
            break;
        }
    }
    if (i == FD_SETSIZE) errx(1, ERROR "Too many clients");
}

char msg[4096];
char *msgtmp;

void check_clients(pool *p) {
    int i, connfd, n;
    char buf[BUFSIZE];

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;
            int exit = 1;
            if ((n = recv(connfd, buf, BUFSIZE, 0)) != 0) {
                printf("Server received %d bytes on fd %d\n", n, connfd);
                exit = 0;
                if (serve(connfd, buf, n, p->client[i]) < 0) exit = 1;
                send(connfd, msg, msgtmp - msg, 0);
            }
            if (exit) {
                close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
                free(p->client[i]);
            }
        }
    }
}

void init(int port) {
    // create listen socket
    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) err(1, ERROR "socket()");

    // bind the socket to ANY localhost address
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(port);
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)))
        err(1, ERROR "bind()");

    // start listening
    if (listen(sockfd, 5)) err(1, ERROR "listen()");

    printf("Start listening on port %d...\n", port);
    static pool pool;
    init_pool(sockfd, &pool);
    while (1) {
        pool.ready_set = pool.read_set;
        pool.nready = select(pool.maxfd + 1, &pool.ready_set, NULL, NULL, NULL);
        if (FD_ISSET(sockfd, &pool.ready_set)) {
            // handle new client
            int connfd = accept(sockfd, NULL, NULL);
            if (connfd < 0) err(1, ERROR "accept()");
            add_clients(connfd, &pool);
        }
        check_clients(&pool);
    }
}

// int main(int argc, char *argv[]) {
//     if (argc < 2) errx(1, "Usage: %s <Port>", argv[0]);
//     init(atoi(argv[1]));
// }