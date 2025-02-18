#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>

#define PORT 8080
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 2 // Number of TCP connections expected

char common_buffer[BUFFER_SIZE * MAX_CLIENTS]; // Store all received data
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void *print_buffer_periodically(void *arg) {
    while (1) {
        sleep(10); // Sleep for 10 minutes
        pthread_mutex_lock(&buffer_mutex);
        printf("Final Buffer Content:\n%s\n ", common_buffer);
        pthread_mutex_unlock(&buffer_mutex);
    }
    return NULL;
}

void set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    int server_fd, client_fd, epoll_fd, num_events;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    struct epoll_event event, events[MAX_EVENTS];
    char buffer[BUFFER_SIZE];

    memset(common_buffer, 0, sizeof(common_buffer));

    // Create server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind and listen
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1 failed");
        exit(EXIT_FAILURE);
    }

    // Add server socket to epoll
    event.events = EPOLLIN;
    event.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event);

    printf("Server listening on port %d...\n", PORT);

    // Start periodic buffer printing thread
    pthread_t print_thread;
    pthread_create(&print_thread, NULL, print_buffer_periodically, NULL);

    while (1) {
        // Wait for events
        num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events < 0) {
            perror("epoll_wait failed");
            exit(EXIT_FAILURE);
        }

        // Process events
        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == server_fd) {
                // Accept new connection
                client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen);
                if (client_fd < 0) {
                    perror("Accept failed");
                    continue;
                }
                set_nonblocking(client_fd);

                // Add client socket to epoll
                event.events = EPOLLIN;
                event.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);
                printf("New connection established: %d\n", client_fd);
            } else {
                // Read from client connection
                memset(buffer, 0, BUFFER_SIZE);
                int bytes_read = read(events[i].data.fd, buffer, BUFFER_SIZE - 1);
                if (bytes_read <= 0) {
                    printf("Client %d disconnected\n", events[i].data.fd);
                    close(events[i].data.fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
                } else {
                    // Append received data to common buffer
                    pthread_mutex_lock(&buffer_mutex);
                    strncat(common_buffer, buffer, BUFFER_SIZE * MAX_CLIENTS - strlen(common_buffer) - 1);
                    pthread_mutex_unlock(&buffer_mutex);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}
