#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define TCP_CONN 2   // Number of TCP connections
#define PACKETS 20   // Total packets to be sent

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int global_packet_id = 1;  // Global packet counter
int current_conn = 2;      // Start with TCP 2
int tcp1_count = 0;        // Counter for TCP 1 packets

void *send_packets(void *arg) {
    int conn_id = *(int *)arg;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = { .sin_family = AF_INET, .sin_port = htons(PORT) };
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        free(arg);
        return NULL;
    }

    for (int i = 0; i < PACKETS / TCP_CONN; i++) {
        pthread_mutex_lock(&lock);
        while (current_conn != conn_id) {
            pthread_cond_wait(&cond, &lock);
        }

        char msg[50];
        sprintf(msg, "TCP:%d | PACKET:%d\n", conn_id, global_packet_id++);
        send(sock, msg, strlen(msg), 0);
        printf("%s", msg);

        // Logic to alternate connections in the pattern 2 -> 1 -> 1 -> 2 -> 1 -> 1 ...
        if (current_conn == 2) {
            // After TCP 2, switch to TCP 1
            current_conn = 1;
            tcp1_count = 0;  // Reset counter for TCP 1
        } else if (current_conn == 1) {
            tcp1_count++;
            if (tcp1_count == 2) {
                // After two packets from TCP 1, switch to TCP 2
                current_conn = 2;
            }
        }

        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&lock);
    }

    close(sock);
    free(arg);
    return NULL;
}

int main() {
    pthread_t threads[TCP_CONN];

    for (int i = 0; i < TCP_CONN; i++) {
        int *conn_id = malloc(sizeof(int));
        *conn_id = i + 1;
        pthread_create(&threads[i], NULL, send_packets, conn_id);
    }

    for (int i = 0; i < TCP_CONN; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All packets sent in the custom pattern.\n");
    return 0;
}
// =============


// #define _POSIX_C_SOURCE 200809L
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <sys/epoll.h>
// #include <signal.h>
// #include <sys/time.h>

// #define PORT 8080
// #define MAX_EVENTS 10
// #define BUFFER_SIZE 1024

// // Global Counters
// int socket1_before_socket2 = 0;
// int socket2_before_socket1 = 0;
// int total_processed = 0;
// int socket1_count = 0;
// int socket2_count = 0;
// int last_socket = -1;
// int running = 1;
// int first_socket = -1;
// int second_socket = -1;

// struct timeval start_time, end_time;

// typedef struct {
//     int fd;
//     char buffer[BUFFER_SIZE];
// } Connection;

// double get_time()
// {
//     struct timeval tv;
//     gettimeofday(&tv, NULL);
//     return tv.tv_sec + tv.tv_usec / 1000000; 
// }

// void handle_connection(Connection *conn)
// {
//     ssize_t received = recv(conn->fd, conn->buffer, BUFFER_SIZE, 0);

//     if (received <= 0)
//     {
//         if (received == 0)
//         {
//             printf("Connection %d closed\n", conn->fd);
//         }
//         else
//         {
//             perror("Receive failed");
//         }
//         close(conn->fd);
//         free(conn);
//         return;
//     }

//     printf("Received data from socket %d: %.*s\n", conn->fd, (int)received, conn->buffer);

//     // Track Order and Count
//     total_processed++;

//     if (conn->fd == second_socket)
//     {
        
//         if (last_socket == first_socket)
//         {
//             socket1_before_socket2++;
//         }
//     }

//         if (conn->fd == first_socket)
//     {
        
//         if (last_socket == second_socket)
//         {
//             socket2_before_socket1++;
//         }
//     }

//     last_socket = conn->fd;
// }

// void print_order_ratio()
// {
//     gettimeofday(&end_time, NULL);

//     double total_time = (end_time.tv_sec - start_time.tv_sec) +
//                         (end_time.tv_usec - start_time.tv_usec) / 1e6;

//     printf("\n--- Order Ratio Analysis ---\n");
//     printf("Total Connections Processed: %d\n", total_processed);
//     printf("Socket 1 before Socket 2: %d times\n", socket1_before_socket2);
//     printf("Socket 2 before Socket 1: %d times\n", socket2_before_socket1);
//     printf("First Socket Count: %d\n", socket1_count);
//     printf("Second Socket Count: %d\n", socket2_count);

//     if (total_processed > 0)
//     {
//         printf("Ratio (Socket 1 before Socket 2 / Total): %.2f\n", (double)socket1_before_socket2 / total_processed);
//     }


//     if (total_time > 0)
//     {
//         printf("Total Time: %.2f sec\n", total_time);
//         printf("Total Throughput: %.2f packets/sec\n", total_processed / total_time);
//     }
// }

// void handle_signal(int signal)
// {
//     printf("\nServer shutting down...\n");
//     // print_order_ratio();
//     running = 0;
// }

// int main()
// {
//     int server_fd, epoll_fd;
//     struct sockaddr_in server_addr, client_addr;
//     socklen_t client_addr_len = sizeof(client_addr);
//     struct epoll_event event, events[MAX_EVENTS];
//     int num_events, i;

//     signal(SIGINT, handle_signal);

//     // Start the timer
//     gettimeofday(&start_time, NULL);

//     // Create server socket
//     server_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (server_fd < 0)
//     {
//         perror("Socket creation failed");
//         exit(EXIT_FAILURE);
//     }

//     int opt = 1;
//     setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

//     // Configure server address
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(PORT);
//     server_addr.sin_addr.s_addr = INADDR_ANY;
//     // Bind the socket
//     if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
//     {
//         perror("Bind failed");
//         close(server_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Listen for incoming connections
//     if (listen(server_fd, 10) < 0)
//     {
//         perror("Listen failed");
//         close(server_fd);
//         exit(EXIT_FAILURE);
//     }
//     printf("Server listening on port %d\n", PORT);

//     // Create epoll instance
//     epoll_fd = epoll_create1(0);
//     if (epoll_fd < 0)
//     {
//         perror("Epoll creation failed");
//         close(server_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Add server socket to epoll
//     event.data.fd = server_fd;
//     event.events = EPOLLIN;
//     if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0)
//     {
//         perror("Epoll CTL ADD failed for server socket");
//         close(server_fd);
//         close(epoll_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Event loop
//     while (running)
//     {
//         num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

//         for (i = 0; i < num_events; i++)
//         {
//             if (events[i].data.fd == server_fd)
//             {
//                 // Accept new client
//                 int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
//                 if (client_fd < 0)
//                 {
//                     perror("Accept failed");
//                     continue;
//                 }
//                 printf("Accepted connection from client (FD: %d)\n", client_fd);

//                 // Create connection object
//                 Connection *conn = (Connection *)malloc(sizeof(Connection));
//                 if (!conn)
//                 {
//                     perror("Memory allocation failed");
//                     close(client_fd);
//                     continue;
//                 }

//                 if (first_socket == -1)
//                     first_socket = client_fd;
//                 else
//                     second_socket = client_fd;

//                 conn->fd = client_fd;
//                 memset(conn->buffer, 0, BUFFER_SIZE);

//                 // Add new client to epoll
//                 // event.data.ptr = conn;
//                 event.data.fd = client_fd;
//                 event.events = EPOLLIN;
//                 if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0)
//                 {
//                     perror("Epoll CTL ADD failed for client socket");
//                     close(client_fd);
//                     free(conn);
//                     continue;
//                 }
//             }
//             else
//             {
//                 // Handle client data
//                 Connection *conn = (Connection *)events[i].data.ptr;
//                 if (events[i].events & EPOLLIN)
//                 {   printf("Handling data from client (FD: %d)\n", events[i].data.fd );
//                     printf("First Socket: %d\n", first_socket);

//                     if (events[i].data.fd == first_socket)
//                     {  printf("First Socket\n");
//                         socket1_count++;
//                     }#define _POSIX_C_SOURCE 200809L
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <sys/epoll.h>
// #include <signal.h>
// #include <sys/time.h>

// #define PORT 8080
// #define MAX_EVENTS 10
// #define BUFFER_SIZE 1024

// // Global Counters
// int socket1_before_socket2 = 0;
// int socket2_before_socket1 = 0;
// int total_processed = 0;
// int socket1_count = 0;
// int socket2_count = 0;
// int last_socket = -1;
// int running = 1;
// int first_socket = -1;
// int second_socket = -1;

// struct timeval start_time, end_time;

// typedef struct {
//     int fd;
//     char buffer[BUFFER_SIZE];
// } Connection;

// double get_time()
// {
//     struct timeval tv;
//     gettimeofday(&tv, NULL);
//     return tv.tv_sec + tv.tv_usec / 1000000; 
// }

// void handle_connection(Connection *conn)
// {
//     ssize_t received = recv(conn->fd, conn->buffer, BUFFER_SIZE, 0);

//     if (received <= 0)
//     {
//         if (received == 0)
//         {
//             printf("Connection %d closed\n", conn->fd);
//         }
//         else
//         {
//             perror("Receive failed");
//         }
//         close(conn->fd);
//         free(conn);
//         return;
//     }

//     printf("Received data from socket %d: %.*s\n", conn->fd, (int)received, conn->buffer);

//     // Track Order and Count
//     total_processed++;

//     if (conn->fd == second_socket)
//     {
        
//         if (last_socket == first_socket)
//         {
//             socket1_before_socket2++;
//         }
//     }

//         if (conn->fd == first_socket)
//     {
        
//         if (last_socket == second_socket)
//         {
//             socket2_before_socket1++;
//         }
//     }

//     last_socket = conn->fd;
// }

// void print_order_ratio()
// {
//     gettimeofday(&end_time, NULL);

//     double total_time = (end_time.tv_sec - start_time.tv_sec) +
//                         (end_time.tv_usec - start_time.tv_usec) / 1e6;

//     printf("\n--- Order Ratio Analysis ---\n");
//     printf("Total Connections Processed: %d\n", total_processed);
//     printf("Socket 1 before Socket 2: %d times\n", socket1_before_socket2);
//     printf("Socket 2 before Socket 1: %d times\n", socket2_before_socket1);
//     printf("First Socket Count: %d\n", socket1_count);
//     printf("Second Socket Count: %d\n", socket2_count);

//     if (total_processed > 0)
//     {
//         printf("Ratio (Socket 1 before Socket 2 / Total): %.2f\n", (double)socket1_before_socket2 / total_processed);
//     }


//     if (total_time > 0)
//     {
//         printf("Total Time: %.2f sec\n", total_time);
//         printf("Total Throughput: %.2f packets/sec\n", total_processed / total_time);
//     }
// }

// void handle_signal(int signal)
// {
//     printf("\nServer shutting down...\n");
//     // print_order_ratio();
//     running = 0;
// }

// int main()
// {
//     int server_fd, epoll_fd;
//     struct sockaddr_in server_addr, client_addr;
//     socklen_t client_addr_len = sizeof(client_addr);
//     struct epoll_event event, events[MAX_EVENTS];
//     int num_events, i;

//     signal(SIGINT, handle_signal);

//     // Start the timer
//     gettimeofday(&start_time, NULL);

//     // Create server socket
//     server_fd = socket(AF_INET, SOCK_STREAM, 0);
//     if (server_fd < 0)
//     {
//         perror("Socket creation failed");
//         exit(EXIT_FAILURE);
//     }

//     int opt = 1;
//     setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

//     // Configure server address
//     server_addr.sin_family = AF_INET;
//     server_addr.sin_port = htons(PORT);
//     server_addr.sin_addr.s_addr = INADDR_ANY;
//     // Bind the socket
//     if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
//     {
//         perror("Bind failed");
//         close(server_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Listen for incoming connections
//     if (listen(server_fd, 10) < 0)
//     {
//         perror("Listen failed");
//         close(server_fd);
//         exit(EXIT_FAILURE);
//     }
//     printf("Server listening on port %d\n", PORT);

//     // Create epoll instance
//     epoll_fd = epoll_create1(0);
//     if (epoll_fd < 0)
//     {
//         perror("Epoll creation failed");
//         close(server_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Add server socket to epoll
//     event.data.fd = server_fd;
//     event.events = EPOLLIN;
//     if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0)
//     {
//         perror("Epoll CTL ADD failed for server socket");
//         close(server_fd);
//         close(epoll_fd);
//         exit(EXIT_FAILURE);
//     }

//     // Event loop
//     while (running)
//     {
//         num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

//         for (i = 0; i < num_events; i++)
//         {
//             if (events[i].data.fd == server_fd)
//             {
//                 // Accept new client
//                 int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
//                 if (client_fd < 0)
//                 {
//                     perror("Accept failed");
//                     continue;
//                 }
//                 printf("Accepted connection from client (FD: %d)\n", client_fd);

//                 // Create connection object
//                 Connection *conn = (Connection *)malloc(sizeof(Connection));
//                 if (!conn)
//                 {
//                     perror("Memory allocation failed");
//                     close(client_fd);
//                     continue;
//                 }

//                 if (first_socket == -1)
//                     first_socket = client_fd;
//                 else
//                     second_socket = client_fd;

//                 conn->fd = client_fd;
//                 memset(conn->buffer, 0, BUFFER_SIZE);

//                 // Add new client to epoll
//                 // event.data.ptr = conn;
//                 event.data.fd = client_fd;
//                 event.events = EPOLLIN;
//                 if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0)
//                 {
//                     perror("Epoll CTL ADD failed for client socket");
//                     close(client_fd);
//                     free(conn);
//                     continue;
//                 }
//             }
//             else
//             {
//                 // Handle client data
//                 Connection *conn = (Connection *)events[i].data.ptr;
//                 if (events[i].events & EPOLLIN)
//                 {   printf("Handling data from client (FD: %d)\n", events[i].data.fd );
//                     printf("First Socket: %d\n", first_socket);

//                     if (events[i].data.fd == first_socket)
//                     {  printf("First Socket\n");
//                         socket1_count++;
//                     }
//                     else
//                     {
//                         socket2_count++;
//                     }
//                     printf("Handling data from client (FD: %d)\n", conn->fd);
//                     handle_connection(conn);
//                 }

//             }
//         }
//     }

//     print_order_ratio();

//     // Cleanup
//     close(server_fd);
//     close(epoll_fd);

//     return 0;
// }








//                     else
//                     {
//                         socket2_count++;
//                     }
//                     printf("Handling data from client (FD: %d)\n", conn->fd);
//                     handle_connection(conn);
//                 }

//             }
//         }
//     }

//     print_order_ratio();

//     // Cleanup
//     close(server_fd);
//     close(epoll_fd);

//     return 0;
// }


