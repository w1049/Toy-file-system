#include "client.h"

int main(int argc, char *argv[]) {
    if (argc < 2) errx(1, "Usage: %s <FSPort>", argv[0]);
    int port = atoi(argv[1]);
    int fd = init_client(port);
    static char buf[4096];
    while (1) {
        fgets(buf, sizeof(buf), stdin);
        if (feof(stdin)) break;
        buf[strlen(buf) - 1] = 0;
        send(fd, buf, strlen(buf), 0);
        int n = recv(fd, buf, sizeof(buf), 0);
        if (n < 0) err(1, ERROR "recv");
        buf[n] = 0;
        printf("%s", buf);
        if (strcmp(buf, "Goodbye!\n") == 0) break;
    }
    close(fd);
}