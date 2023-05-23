#include <arpa/inet.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define ERROR "\033[31m[Error]\033[0m "

#define BUFSIZE 1024

struct sockitem {
    int sockfd;
    int (*callback)(int fd, int events, void* arg);
    char recvbuf[BUFSIZE];
    char sendbuf[BUFSIZE];
    int recvlen;
    int sendlen;
};

struct reactor {
    int epfd;
    struct epoll_event events[512];
}* eventloop = NULL;

int send_cb(int fd, int events, void* arg);
int serve(char* buf, int len);

int recv_cb(int fd, int events, void* arg) {
    struct sockitem* si = (struct sockitem*)arg;
    struct epoll_event ev;  // 后面需要

    // 处理IO读事件
    int ret = recv(fd, si->recvbuf, BUFSIZE, 0);
    if (ret < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {  //
            return -1;
        } else {
        }

        // 出错了，从监视IO事件红黑树中移除结点，避免僵尸结点
        ev.events = EPOLLIN;
        epoll_ctl(eventloop->epfd, EPOLL_CTL_DEL, fd, &ev);

        close(fd);
        free(si);

    } else if (ret == 0) {
        // 对端断开连接
        printf("fd %d disconnect\n", fd);

        ev.events = EPOLLIN;
        epoll_ctl(eventloop->epfd, EPOLL_CTL_DEL, fd, &ev);
        // close同一断开连接，避免客户端大量的close_wait状态
        close(fd);
        free(si);

    } else {
        // 打印接收到的数据
        printf("recv: %s", si->recvbuf);
        int exit = 0;
        if (serve(si->recvbuf, ret) < 0) {
            exit = 1;
        }
        // 设置sendbuf
        si->recvlen = ret;
        memcpy(si->sendbuf, si->recvbuf, si->recvlen);
        si->sendlen = si->recvlen;
        // 注册写事件处理器
        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLET;

        si->sockfd = fd;
        si->callback = send_cb;
        ev.data.ptr = si;

        epoll_ctl(eventloop->epfd, EPOLL_CTL_MOD, fd, &ev);
    }
		return 0;
}

int accept_cb(int fd, int events, void* arg) {
    // 处理新的连接。 连接IO事件处理流程
    struct sockaddr_in cli_addr;
    memset(&cli_addr, 0, sizeof(cli_addr));
    socklen_t cli_len = sizeof(cli_addr);

    int cli_fd = accept(fd, (struct sockaddr*)&cli_addr, &cli_len);
    if (cli_fd <= 0) return -1;

    char cli_ip[INET_ADDRSTRLEN] = {0};  // 存储cli_ip

    printf("Recv from ip %s at port %d\n",
           inet_ntop(AF_INET, &cli_addr.sin_addr, cli_ip, sizeof(cli_ip)),
           ntohs(cli_addr.sin_port));
    // 注册接下来的读事件处理器
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    struct sockitem* si = (struct sockitem*)malloc(sizeof(struct sockitem));
    si->sockfd = cli_fd;
    si->callback = recv_cb;  // 设置事件处理器

    ev.data.ptr = si;
    epoll_ctl(eventloop->epfd, EPOLL_CTL_ADD, cli_fd, &ev);

    return cli_fd;
}

int send_cb(int fd, int events, void* arg) {
    // 处理send IO事件
    struct sockitem* si = (struct sockitem*)arg;
    send(fd, si->sendbuf, si->sendlen, 0);

    // 再次注册IO读事件处理器
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;

    si->sockfd = fd;
    si->callback = recv_cb;  // 设置事件处理器
    ev.data.ptr = si;

    epoll_ctl(eventloop->epfd, EPOLL_CTL_MOD, fd, &ev);
		return 0;
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
    if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)))
        err(1, ERROR "bind()");

    // start listening
    if (listen(sockfd, 5)) err(1, ERROR "listen()");

    printf("Start listening on port %d...\n", port);

    // init eventloop
    eventloop = (struct reactor*)malloc(sizeof(struct reactor));
    // 创建epoll句柄
    eventloop->epfd = epoll_create(1);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    struct sockitem* si = (struct sockitem*)malloc(sizeof(struct sockitem));
    si->sockfd = sockfd;
    si->callback = accept_cb;  // 设置事件处理器

    ev.data.ptr = si;
    // 将监视事件加入到reactor的epfd中
    epoll_ctl(eventloop->epfd, EPOLL_CTL_ADD, sockfd, &ev);

    while (1) {
        // 多路复用器监视多个IO事件
        int nready = epoll_wait(eventloop->epfd, eventloop->events, 512, -1);
        if (nready < -1) {
            break;
        }

        int i = 0;
        // 循环分发所有的IO事件给处理器
        for (i = 0; i < nready; ++i) {
            if (eventloop->events[i].events & EPOLLIN) {
                struct sockitem* si =
                    (struct sockitem*)eventloop->events[i].data.ptr;
                si->callback(si->sockfd, eventloop->events[i].events, si);
            }

            if (eventloop->events[i].events & EPOLLOUT) {
                struct sockitem* si =
                    (struct sockitem*)eventloop->events[i].data.ptr;
                si->callback(si->sockfd, eventloop->events[i].events, si);
            }
        }
    }
}
