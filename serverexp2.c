#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sys/time.h>
#include <netinet/tcp.h>
#include <inttypes.h>
#include <inttypes.h>
#include <netinet/tcp.h>
#include <fcntl.h>
 
#include <sched.h>         // for CPU_SET, sched_setaffinity
#include <sys/types.h>     // sometimes required for CPU_SET
#include <sys/socket.h>    // for accept4
#include <fcntl.h>         

#ifndef SO_INCOMING_NAPI_ID
#define SO_INCOMING_NAPI_ID 56
#endif


#define PORT 8080
#define MAX_EVENTS 10
#define BUFFER_SIZE 1024
#define PACKET_SIZE 60

// Global Counters
int socket1_before_socket2 = 0;
int socket2_before_socket1 = 0;
int total_processed = 0;
int socket1_count = 0;
int socket2_count = 0;
int last_socket = -1;
int running = 1;
int first_socket = -1;
int second_socket = -1;
uint64_t total_data_bytes=0;
double start_time = 0;
double end_time = 0;
uint64_t conn1_bytes = 0;
uint64_t conn2_bytes = 0;
uint64_t conn1_packets = 0; // New: Packets received on Conn1
uint64_t conn2_packets = 0;
int socket1total_count = 0;
int socket2total_count = 0;


int both = 0;


struct timeval conn1_start, conn1_end;
struct timeval conn2_start, conn2_end;
int conn1_started = 0, conn2_started = 0;


typedef struct
{
    int fd;
    char buffer[BUFFER_SIZE];
} Connection;

void setnonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl get");
        exit(EXIT_FAILURE);
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl set");
        exit(EXIT_FAILURE);
    }
}
double get_time()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1000000.0;
}

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

//         // Record start time on first packet
//     if (total_processed == 0)
//     {
//         gettimeofday(&start_time, NULL);
//     }

//     // Update end time on every packet
//     gettimeofday(&end_time, NULL);


//     // printf("Received data from socket %d: %.*s\n", conn->fd, (int)received, conn->buffer);

//     // Track Order and Count
//     // total_processed++;

//     // const int MSG_SIZE = 2; // "1 " is 2 bytes
//     // while (received >= MSG_SIZE)
//     // {
//     //     total_processed++;
//     //     received -= MSG_SIZE;
//     // }
//     total_data_bytes += received; 

//     if (conn->fd == second_socket && last_socket == first_socket)
//     {
//         socket1_before_socket2++;
//     }
//     else if (conn->fd == first_socket && last_socket == second_socket)
//     {
//         socket2_before_socket1++;
//     }

//     last_socket = conn->fd;
// }

void print_order_ratio()
{
 

    double time_diff(struct timeval start, struct timeval end) {
        return (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;
    }
    double conn1_duration = time_diff(conn1_start, conn1_end);
    double conn2_duration = time_diff(conn2_start, conn2_end);
    double total_time = conn1_duration + conn2_duration;

    printf("Throughput Conn1 = %.2f Mbps\n", (conn1_bytes * 8.0) / (conn1_duration * 1e6));
    printf("Throughput Conn2 = %.2f Mbps\n", (conn2_bytes * 8.0) / (conn2_duration * 1e6));



    printf("\n--- Order Ratio Analysis ---\n");
    // printf("Total Connections Processed: %d\n", total_processed);
    printf("Socket 1 before Socket 2 = %d times\n", socket1_before_socket2);
    printf("Socket 2 before Socket 1 = %d times\n", socket2_before_socket1);
    printf("Socket 2 and Socket 1 = %d times\n", both);
    printf("First Socket Count = %d\n", socket1total_count-(socket1_before_socket2+socket2_before_socket1 ) );
    printf("Second Socket Count = %d\n", socket2total_count  -(socket1_before_socket2+socket2_before_socket1 ) );

    printf("Conn1 bytes = %" PRIu64 "\n", conn1_bytes);
    printf("Conn2 bytes = %" PRIu64 "\n", conn2_bytes);
    printf("Conn1 packets = %" PRIu64 "\n", conn1_packets); // New
    printf("Conn2 packets = %" PRIu64 "\n", conn2_packets); // New
    printf("Conn1 duration = %f sec\n", conn1_duration);
    printf("Conn2 duration = %f sec\n", conn2_duration);
    printf("Total Time = %f sec\n", total_time);
    
    // if (total_processed > 0)
    // {
        printf("Ratio (Socket 1 before Socket 2 / Total) = %f\n", (double)socket1_before_socket2 / (socket1_before_socket2 + socket2_before_socket1));
        printf("Ratio (Socket 2 before Socket 1 / Total) = %f\n", (double)socket2_before_socket1 / (socket1_before_socket2 + socket2_before_socket1));
    // }
    
    if (total_time > 0)
    {
        

        double throughput_mbps = (total_data_bytes * 8) / (total_time * 1e6);
        //printf("Total Throughput: %f packets/sec\n", total_processed / total_time);
        printf("Throughput = %f Mbps\n", throughput_mbps);
    }
}

void handle_signal(int signal)
{
    printf("\nServer shutting down...\n");
    running = 0;
}

int main()
{
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("sched_setaffinity (main)");
    }
    int server_fd, epoll_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    struct epoll_event event, events[MAX_EVENTS];

    signal(SIGINT, handle_signal);

    

    // Create server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    //Disable Nagle’s Algorithm (Low-Latency Optimization)
    int flag = 1;
    setsockopt(server_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    //Increase Socket Buffer Sizes (Prevents Dropped Packets)
    int rcvbuf = 1024 * 1024; // 1MB
    int sndbuf = 1024 * 1024;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
    setsockopt(server_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    //TCP_DEFER_ACCEPT to Avoid Wakeups Until Data Arrives
    // int defer = 3;
    // setsockopt(server_fd, IPPROTO_TCP, TCP_DEFER_ACCEPT, &defer, sizeof(defer));

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, "10.130.154.1", &server_addr.sin_addr);

    //server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 1000) < 0)
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
    // event.events = EPOLLIN | EPOLLET;  // Edge-triggered
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) < 0)
    {
        perror("Epoll CTL ADD failed for server socket");
        close(server_fd);
        close(epoll_fd);
        exit(EXIT_FAILURE);
    }
    int count = 0;
    // Event loop
    int pick = -1;
    int active_clients = 0;

    while (running)
    {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

        //printf("Number of events: %d\n", num_events);
        int picked = -1;
        for (int i = 0; i < num_events; i++)
        {   
            //printf("pick value %d Received FD %d\n", pick, events[i].data.fd);
             //printf("Userspace received FD %d (index %d)\n", events[i].data.fd, i);
            fflush(stdout);
            if (events[i].data.fd == server_fd)
            {   //printf("hi");
                count++;
                // Accept new client
                int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
                int napi_id;
                socklen_t len = sizeof(napi_id);
                if (getsockopt(client_fd, SOL_SOCKET,SO_INCOMING_NAPI_ID,&napi_id, &len) == 0) {
                  //  printf("Socket %d got NAPI ID %d\n", client_fd, napi_id);
                }
                if (client_fd < 0)
                {
                    perror("Accept failed");
                    continue;
                }
                active_clients++;
                int flag = 1;
                setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

               // printf("Accepted connection from client (FD: %d)\n", client_fd);
                setnonblocking(client_fd);
                // Create connection object
                Connection *conn = (Connection *)malloc(sizeof(Connection));
                if (!conn)
                {
                    perror("Memory allocation failed");
                    close(client_fd);
                    continue;
                }
                conn->fd = client_fd;
                memset(conn->buffer, 0, BUFFER_SIZE);

                if (first_socket == -1)
                {
                    first_socket = client_fd;
                }
                else if (second_socket == -1)
                {
                    second_socket = client_fd;
                }

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
                if (!conn)
                    continue;
                //printf("%d\n",conn->fd);
               //printf("Received FD %d (index %d)\n", conn->fd, i);
                ssize_t received = recv(conn->fd, conn->buffer, BUFFER_SIZE, 0);
            
                if(conn->fd == first_socket && received > 0  ){
                    if (!conn1_started) {
                        gettimeofday(&conn1_start, NULL);
                    conn1_started = 1;
                    //conn1_bytes += received;
                    }
                    gettimeofday(&conn1_end, NULL);
                    
                   
                        socket1total_count++;
                       // printf("Socket 1\n");
                
                }

                if(conn->fd == second_socket && received > 0 ){
                    if (!conn2_started) {
                        gettimeofday(&conn2_start, NULL);
                    conn2_started = 1;
                    //conn2_bytes += received;
                    }
                    gettimeofday(&conn2_end, NULL);
                    
                   
                        socket2total_count++;
                       // printf("Socket 2\n");
                
                }


                if (received > 0 &&  count ==2) {    
            
                   // printf("Received data from socket %d: %.*s\n", conn->fd, (int)received, conn->buffer);
                    total_data_bytes += received;
                    
                    if (conn->fd == first_socket) {
                        if (!conn1_started) {
                            gettimeofday(&conn1_start, NULL);
                        conn1_started = 1;
                        }
                        gettimeofday(&conn1_end, NULL);
                        conn1_bytes += received;
                        conn1_packets += received / PACKET_SIZE;
                        // if(num_events==1){
                        //     socket1_count++;
                        //     printf("Socket 1\n");
                        // }
                        if(pick==-1){
                            pick=1;
                           
                        }
                            
                        
                        else if (pick == 2) {
                            socket2_before_socket1++;
                            pick = -1;
                            both++;
                           // printf("Counted2\n");
                        }
                    }

                    else if (conn->fd == second_socket) {
                        if (!conn2_started) {
                            gettimeofday(&conn2_start, NULL);
                            conn2_started = 1;
                        }
                        gettimeofday(&conn2_end, NULL);
                        conn2_bytes += received;
                        conn2_packets += received / PACKET_SIZE;

                        // if(num_events==1){
                        //         socket2_count++;
                        //         printf("Socket 2\n");
                        // }
                        if(pick==-1){
                            pick=2;
                            
                        }
                        
                        else if (pick == 1) {
                            socket1_before_socket2++;
                            pick = -1;

                            both++;
                          //  printf("Counted\n");
                        }
                    }
                    
                    // printf("Handling data from client (FD: %d)\n", conn->fd);


                    // if(num_events ==2){
                    //     if (conn->fd == first_socket)
                    //     {   if(picked==-1)
                    //             socket1_before_socket2++;
                    //         picked=1;
                    //         conn1_bytes +=received;
                    //     }

                    //     if (conn->fd == second_socket)
                    //     {   if(picked==-1)
                    //             socket2_before_socket1++;
                    //         picked=1;
                    //         conn2_bytes +=received;
                    //     }
                    // }
                    
                }

                else {
                    if (received == 0) {
                        active_clients--;
                  // printf("Connection %d closed\n", conn->fd);
                    } 
                    else {
                        perror("Receive failed");
                    }

                    if(active_clients==0){
                        running = 0;
                    }
                    close(conn->fd);
                    free(conn);
                }

            }
        }
    }

    print_order_ratio();

    // Cleanup
    close(server_fd);
    close(epoll_fd);

    return 0;
}
