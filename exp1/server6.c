#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#ifndef SO_REUSEPORT
#define SO_REUSEPORT 0x0200
#endif

#define PORT 8080
#define MAX_CONNECTIONS 1000
#define BUFFER_SIZE 65536
#define MAX_EVENTS 1000

typedef struct {
    int fd;
    struct sockaddr_in addr;
    char buffer[BUFFER_SIZE];
    size_t buf_used;
    int packet_count;
} Connection;

Connection *connections[MAX_CONNECTIONS] = {0};

void handle_connection(Connection *conn, FILE *output) {
    ssize_t received = recv(conn->fd, conn->buffer + conn->buf_used, 
                          BUFFER_SIZE - conn->buf_used, 0);
    
    if (received <= 0) {
        if (received == 0) {
            printf("Connection %d closed\n", conn->fd);
        }
        close(conn->fd);
        free(conn);
        return;
    }

    conn->buf_used += received;
    char *start = conn->buffer;
    char *end = conn->buffer + conn->buf_used;

    while (start < end) {
        char *newline = memchr(start, '\n', end - start);
        if (!newline) break;

        *newline = '\0';

        // Write to file and flush
        fprintf(output, "CONN:%d|PKT:%d|DATA:%s\n", 
                conn->fd, ++conn->packet_count, start);

        // fprintf(output, "%s\n",  start);

        // CONN:7|PKT:1|DATA:CONN:2|PKT:1
        fflush(output);  // Ensure immediate writing

        printf("Logged packet %d from connection %d\n", conn->packet_count, conn->fd);

        start = newline + 1;
    }

    conn->buf_used = end - start;
    memmove(conn->buffer, start, conn->buf_used);
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    // int reuse = 1;
    // setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("Listen failed");
        return 1;
    }

    int epoll_fd = epoll_create1(0);
    struct epoll_event event = {
        .events = EPOLLIN | EPOLLET,
        .data.fd = server_fd
    };
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    FILE *output = fopen("multiconn_packets.log", "w");
    if (!output) {
        perror("Error opening log file");
        return 1;
    }

    struct epoll_event events[MAX_EVENTS];
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == server_fd) {
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    int client_fd = accept4(server_fd, (struct sockaddr*)&client_addr,
                                          &addr_len, SOCK_NONBLOCK);
                    
                    if (client_fd < 0) break;

                    Connection *conn = calloc(1, sizeof(Connection));
                    conn->fd = client_fd;
                    conn->addr = client_addr;
                    conn->buf_used = 0;
                    conn->packet_count = 0;
                    connections[client_fd] = conn;

                    struct epoll_event ev = {
                        .events = EPOLLIN | EPOLLET,
                        .data.fd = client_fd
                    };
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                    
                    printf("New connection: %d\n", client_fd);
                }
            } else {
                Connection *conn = connections[events[i].data.fd];
                if (conn) handle_connection(conn, output);
            }
        }
    }

    fclose(output);
    close(server_fd);
    return 0;
}
