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

int main(int argc, char *argv[]) {
    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) errx(1, ERROR "socket()");
    struct sockaddr_in serv_addr;
    struct hostent *host;
    serv_addr.sin_family = AF_INET;
    host = gethostbyname("localhost");
    if (host == NULL) errx(1, ERROR "gethostbyname()");
    memcpy(&serv_addr.sin_addr.s_addr, host->h_addr, host->h_length);
    serv_addr.sin_port = htons(atoi(argv[1]));
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        err(1, ERROR "connect()");
    char buf[1024];
    int n;
    while (1) {
        fgets(buf, 1024, stdin);
        n = send(sockfd, buf, strlen(buf), 0);
        if (n < 0) err(1, ERROR "send()");
        n = recv(sockfd, buf, 1024, 0);
        if (n < 0) err(1, ERROR "recv()");
        buf[n] = '\0';
        printf("%s", buf);
    }
}