#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>

#define PORT 8080
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024

// Global Counters
int socket1_before_socket2 = 1;
int total_processed = 0;
int last_socket = -1;
int running = 1;
int first_socket = -1;
int second_socket = -1;

typedef struct
{
    int fd;
    char buffer[BUFFER_SIZE];
} Connection;

void handle_connection(Connection *conn)
{
    ssize_t received = recv(conn->fd, conn->buffer, BUFFER_SIZE, 0);

    if (received <= 0)
    {
        if (received == 0)
        {
            printf("Connection %d closed\n", conn->fd);
        }
        else
        {
            perror("Receive failed");
        }
        close(conn->fd);
        free(conn);
        return;
    }

    printf("Received data from socket %d: %.*s\n", conn->fd, (int)received, conn->buffer);

    // Track Order and Count
    total_processed++;
if (last_socket == first_socket && conn->fd == second_socket) {
    socket1_before_socket2++;
}
last_socket = conn->fd;

   
}

void print_order_ratio()
{
    printf("\n--- Order Ratio Analysis ---\n");
    printf("Total Connections Processed: %d\n", total_processed);
    printf("Socket 1 before Socket 2: %d times\n", socket1_before_socket2);

    if (total_processed > 0)
    {
        double ratio = (double)socket1_before_socket2 / total_processed;
        printf("Ratio (Socket 1 before Socket 2 / Total): %.2f\n", ratio);
    }
}

void handle_signal(int signal)
{
    printf("\nServer shutting down...\n");
    print_order_ratio();
    running = 0;
}

int main()
{
    int server_fd, epoll_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct epoll_event event, events[MAX_EVENTS];
    int num_events, i;

    signal(SIGINT, handle_signal);

    // Create server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port %d\n", PORT);

    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd < 0)
    {
        perror("Epoll creation failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Add server socket to epoll
    event.data.fd = server_fd;
    event.events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0)
    {
        perror("Epoll CTL ADD failed for server socket");
        close(server_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }

    // Event loop
    while (running)
    {
        num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        for (i = 0; i < num_events; i++)
        {
            if (events[i].data.fd == server_fd)
            {
                // Accept new client
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                if (client_fd < 0)
                {
                    perror("Accept failed");
                    continue;
                }
                printf("Accepted connection from client (FD: %d)\n", client_fd);

                // Create connection object
                Connection *conn = (Connection *)malloc(sizeof(Connection));
                if (!conn)
                {
                    perror("Memory allocation failed");
                    close(client_fd);
                    continue;
                }
                if (first_socket==-1)
                first_socket = client_fd;

                else
                second_socket = client_fd;
                conn->fd = client_fd;
                memset(conn->buffer, 0, BUFFER_SIZE);

                // Add new client to epoll
                event.data.ptr = conn;
                event.events = EPOLLIN;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0)
                {
                    perror("Epoll CTL ADD failed for client socket");
                    close(client_fd);
                    free(conn);
                    continue;
                }
            }
            else
            {
                // Handle client data

                Connection *conn = (Connection *)events[i].data.ptr;
                if (events[i].events & EPOLLIN)
                {
                    printf("Handling data from client (FD: %d)\n", events[i].data.fd);
                    handle_connection(conn);
                }
            }
        }
    }

    // Cleanup
    close(server_fd);
    close(epoll_fd);

    return 0;
}
