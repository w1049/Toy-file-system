//
// Server functions
//

#include "server.h"

#define BUFSIZE 4096

typedef struct {               // Represents a pool of connected descriptors
    int maxfd;                 // Largest descriptor in read_set
    fd_set read_set;           // Set of all active descriptors
    fd_set ready_set;          // Subset of descriptors ready for reading
    int nready;                // Number of ready descriptors from select
    int maxi;                  // High water index into client array
    int clientfd[FD_SETSIZE];  // Set of active descriptors
    void *client[FD_SETSIZE];
    int exit;
} pool;

void init_pool(int listenfd, pool *p) {
    p->maxi = 1;
    for (int i = 0; i < FD_SETSIZE; i++) p->clientfd[i] = -1;

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set);
}

void add_clients(int connfd, pool *p, void *(*client_init)(int)) {
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

void check_clients(pool *p, int (*serve)(int, char *, int, void *)) {
    int i, connfd, n;
    static char buf[BUFSIZE];

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];

        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
            p->nready--;
            int exit = 1;
            n = recv(connfd, buf, BUFSIZE, 0);
            if (n > 0) {
                buf[n] = 0;
                printf("Server received %d bytes on fd %d\n", n, connfd);
                exit = 0;
                char *ptr = NULL;
                char *s = strtok_r(buf, "\r\n", &ptr);
                while (s) {
                    if (serve(connfd, s, strlen(s), p->client[i]) < 0) exit = 1;
                    s = strtok_r(NULL, "\r\n", &ptr);
                }
            } else if (n < 0) {
                printf("recv() error\n");
                exit = 1;
            }
            if (exit) {
                close(connfd);
                FD_CLR(connfd, &p->read_set);
                p->clientfd[i] = -1;
                if (p->client[i]) free(p->client[i]);
            }
        }
    }
}

void mainloop(int port, void *(*client_init)(int),
              int (*serve)(int, char *, int, void *)) {
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
            add_clients(connfd, &pool, client_init);
        }
        check_clients(&pool, serve);
    }
    close(sockfd);
}