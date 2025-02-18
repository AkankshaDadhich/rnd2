#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include <signal.h>

#define PORT 8080
#define MAX_CONNECTIONS 10000
#define BUFFER_SIZE 65536
#define MAX_EVENTS 1000
#define MAX_OUT_OF_ORDER 1000000

int running = 1; 

#include <limits.h>

typedef struct
{
    int fd;
    struct sockaddr_in addr;
    char buffer[BUFFER_SIZE];
    size_t buf_used;
} Connection;

Connection *connections[MAX_CONNECTIONS] = {0};

int global_expected_seq = 1;
int out_of_order[MAX_OUT_OF_ORDER] = {0};
int out_of_order_count = 0;
void handle_connection(Connection *conn)
{
    ssize_t received = recv(conn->fd, conn->buffer, BUFFER_SIZE - 1, 0);

    if (received <= 0)
    {
        if (received == 0)
        {
            printf("Connection %d closed\n", conn->fd);
        }
        close(conn->fd);
        free(conn);
        return;
    }

    conn->buffer[received] = '\0'; // Null-terminate

    printf("Received data: %.*s\n", (int)received, conn->buffer);

    char *start = conn->buffer;
    char *end = conn->buffer + received;

    while (start < end)
    {
        char *newline = memchr(start, '\n', end - start); // Find newline
        if (!newline)
            break;

        *newline = '\0'; // Replace newline with NULL to isolate number

        int packet_seq;
        if (sscanf(start, "%d", &packet_seq) == 1) // Parse number
        {
            if (packet_seq == global_expected_seq)
            {
                // printf("Received expected packet %d global %d from connection %d\n", packet_seq, global_expected_seq, conn->fd);
                global_expected_seq++;
            }
            else if (packet_seq < global_expected_seq)
            {
                // printf("Received old packet %d global %d from connection %d\n", packet_seq, global_expected_seq, conn->fd);
            }
            else // packet_seq > global_expected_seq
            {
                for (int i = global_expected_seq; i < packet_seq; i++)
                {
                    if (out_of_order_count < MAX_OUT_OF_ORDER)
                    {
                        out_of_order[out_of_order_count++] = i;
                    }
                }
                global_expected_seq = packet_seq + 1;
                // printf("Out of order packets: packet %d global %d\n",packet_seq, global_expected_seq);
            }
        }

        start = newline + 1; // Move to next number
    }
}

// 🟢 Signal handler to stop the server
void handle_signal(int signal)
{
    printf("\nServer shutting down...\n");

    printf("Final out-of-order packets:\n");
    for (int i = 0; i < out_of_order_count; i++)
    {
        printf("%d ", out_of_order[i]);
    }

    running = 0; // Stop the main loop
}

int main()
{
    signal(SIGINT, handle_signal);  // 🟢 Capture Ctrl+C

    int server_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (server_fd < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
        };
        // inet_pton(AF_INET, "10.130.154.1", &addr.sin_addr);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("Bind failed");
        return 1;
    }

    if (listen(server_fd, MAX_CONNECTIONS) < 0)
    {
        perror("Listen failed");
        return 1;
    }

    int epoll_fd = epoll_create1(0);
    struct epoll_event event = {
        .events = EPOLLIN | EPOLLET,
        .data.fd = server_fd};
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    struct epoll_event events[MAX_EVENTS];
    printf("Server listening on port %d...\n", PORT);

    while (running)  // 🟢 Stop when `running = 0`
    {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        for (int i = 0; i < n; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                while (1)
                {
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    int client_fd = accept4(server_fd, (struct sockaddr *)&client_addr, &addr_len, SOCK_NONBLOCK);

                    if (client_fd < 0)
                        break;

                    Connection *conn = calloc(1, sizeof(Connection));
                    conn->fd = client_fd;
                    conn->addr = client_addr;
                    connections[client_fd] = conn;

                    struct epoll_event ev = {
                        .events = EPOLLIN | EPOLLET,
                        .data.fd = client_fd};
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

                    printf("New connection: %d\n", client_fd);
                }
            }
            else
            {
                Connection *conn = connections[events[i].data.fd];
                if (conn)
                    handle_connection(conn);
            }
        }
    }

    close(server_fd);
    return 0;
}