#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <signal.h>

#define SERVER_IP "10.130.153.2"
#define PORT 8080
#define BUFFER_SIZE 1024
#define DEFAULT_RUNTIME 3600 // Default runtime in seconds (5 minutes)

volatile sig_atomic_t running = 1;
unsigned long total_msgs_sent = 0;
unsigned long total_packets_sent = 0;

void handle_signal(int sig) {
    running = 0;
}

int main(int argc, char *argv[]) {
    // Parse runtime from command-line argument
 
        // Parse server IP and runtime from command-line arguments
        const char *server_ip = "10.130.153.2"; // Default IP
        double max_runtime = DEFAULT_RUNTIME;
    
        if (argc > 1) {
            server_ip = argv[1]; // First argument: IP
        }
    
        if (argc > 2) {
            max_runtime = atoi(argv[2]); // Second argument: runtime in seconds
            if (max_runtime <= 0) {
                fprintf(stderr, "Invalid runtime, using default: %d seconds\n", DEFAULT_RUNTIME);
                max_runtime = DEFAULT_RUNTIME;
            }
        }
    
        printf("Connecting to %s\n", server_ip);
        printf("Running for %.0f seconds\n", max_runtime);


    // double max_runtime = DEFAULT_RUNTIME;
    // if (argc > 1) {
    //     max_runtime = atoi(argv[1]);
    //     if (max_runtime <= 0) {
    //         fprintf(stderr, "Invalid runtime, using default: %d seconds\n", DEFAULT_RUNTIME);
    //         max_runtime = DEFAULT_RUNTIME;
    //     }
    // }
    // printf("Running for %.0f seconds\n", max_runtime);

    unsigned long total_bytes_sent = 0;

    signal(SIGINT, handle_signal);

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    int sndbuf = 8 * 1024 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    //inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

    char *msg_buffer = malloc(BUFFER_SIZE);
    if (!msg_buffer) {
        perror("Failed to allocate message buffer");
        close(sock);
        exit(EXIT_FAILURE);
    }
    memset(msg_buffer, 'P', BUFFER_SIZE);

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    double elapsed = 0;

    size_t bytes_to_send = BUFFER_SIZE;
    char *buf_ptr = msg_buffer;

    while (running && elapsed < max_runtime) {
        ssize_t sent = send(sock, buf_ptr, bytes_to_send, MSG_DONTWAIT);
        if (sent > 0) {
            total_bytes_sent += sent;
            bytes_to_send -= sent;
            buf_ptr += sent;
            if (bytes_to_send == 0) {
                total_msgs_sent++;
                total_packets_sent++; // Each 1024-byte send is one packet
                bytes_to_send = BUFFER_SIZE;
                buf_ptr = msg_buffer;
            }
        } else if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            printf("Send error: %s\n", strerror(errno));
            break;
        }

        // Update elapsed time (check less frequently to reduce overhead)
        if (total_msgs_sent % 100 == 0) {
            clock_gettime(CLOCK_MONOTONIC, &now);
            elapsed = (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;
        }
    }

    // Final statistics
    clock_gettime(CLOCK_MONOTONIC, &now);
    elapsed = (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;

    printf("\nPersistent Connection Stats:\n");
    printf("Total messages: %lu\n", total_msgs_sent);
    printf("Total packets: %lu\n", total_packets_sent);
    printf("Total bytes sent: %lu\n", total_bytes_sent);
    printf("Throughput: %.2f msgs/sec\n", elapsed > 0 ? total_msgs_sent / elapsed : 0.0);
    printf("Throughput: %.2f Mbps\n", elapsed > 0 ? (total_bytes_sent * 8.0) / (elapsed * 1e6) : 0.0);
    printf("Terminated %s\n", elapsed >= max_runtime ? "due to reaching time limit" : "by user interrupt");

    free(msg_buffer);
    close(sock);
    return 0;
}