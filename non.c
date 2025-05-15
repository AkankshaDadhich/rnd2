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
#include <signal.h>
#include <sys/time.h>

#define SERVER_IP "10.130.153.2"
#define PORT 8080
#define BUFFER_SIZE 1024
#define DEFAULT_MSGS_PER_CONN 100000
#define RUNTIME 1// Hardcoded runtime in seconds (1 hour)

unsigned long total_msgs_sent = 0;
unsigned long total_packets_sent = 0;
volatile sig_atomic_t running = 1;
unsigned long total_connections = 0;
int non_complete = 0;

void handle_signal(int sig) {
    running = 0;
    printf("\nReceived shutdown signal, completing current connection...\n");
}

int main(int argc, char *argv[]) {
    // Parse MSGS_PER_CONN from command-line argument
    const char *server_ip = SERVER_IP;  // Default
    int runtime = RUNTIME;
    int msgs_per_conn = DEFAULT_MSGS_PER_CONN;

    // Argument parsing
    if (argc > 1) {
        server_ip = argv[1];
    }

    if (argc > 2) {
        runtime = atoi(argv[2]);
        if (runtime <= 0) {
            fprintf(stderr, "Invalid runtime, using default: %d seconds\n", RUNTIME);
            runtime = RUNTIME;
        }
    }

    if (argc > 3) {
        msgs_per_conn = atoi(argv[3]);
        if (msgs_per_conn <= 0) {
            fprintf(stderr, "Invalid MSGS_PER_CONN, using default: %d\n", DEFAULT_MSGS_PER_CONN);
          
        }
    }

    printf("Connecting to %s\n", server_ip);
    printf("Running with MSGS_PER_CONN=%d for %d seconds\n", msgs_per_conn, runtime);


    // int msgs_per_conn = DEFAULT_MSGS_PER_CONN;
    // if (argc > 1) {
    //     msgs_per_conn = atoi(argv[1]);
    //     if (msgs_per_conn <= 0) {
    //         fprintf(stderr, "Invalid MSGS_PER_CONN, using default: %d\n", DEFAULT_MSGS_PER_CONN);
    //         msgs_per_conn = DEFAULT_MSGS_PER_CONN;
    //     }
    // }
    // printf("Running with MSGS_PER_CONN=%d for %d seconds\n", msgs_per_conn, RUNTIME);

    signal(SIGINT, handle_signal);

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    char *msg_buffer = malloc(BUFFER_SIZE);
    if (!msg_buffer) {
        perror("Failed to allocate message buffer");
        exit(EXIT_FAILURE);
    }
    memset(msg_buffer, 'N', BUFFER_SIZE); // Non-persistent identifier

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);
    double elapsed = 0;

    while (running && elapsed < runtime)
    {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Socket creation failed");
            continue;
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

        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0) {
            int msgs_sent = 0;
            size_t bytes_to_send = BUFFER_SIZE;
            char *buf_ptr = msg_buffer;

            // Send MSGS_PER_CONN messages, even if time is up
            while (msgs_sent < msgs_per_conn && running) {
                ssize_t sent = send(sock, buf_ptr, bytes_to_send, 0);
                if (sent > 0) {
                    bytes_to_send -= sent;
                    buf_ptr += sent;
                    if (bytes_to_send == 0) {
                        msgs_sent++;
                        total_msgs_sent++;
                        total_packets_sent++; // Each 1024-byte send is one packet
                        bytes_to_send = BUFFER_SIZE;
                        buf_ptr = msg_buffer;
                    }
                } else if (sent < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        continue;
                    }
                    printf("Send error at message %d: %s\n", msgs_sent + 1, strerror(errno));
                    break;
                }
            }

            if (msgs_sent == msgs_per_conn) {
                total_connections++;
            } else {
                non_complete++;
                printf("Non-complete connection: %d messages sent\n", msgs_sent);
            }

            struct linger lo = {1, 0};
            setsockopt(sock, SOL_SOCKET, SO_LINGER, &lo, sizeof(lo));
            shutdown(sock, SHUT_RDWR);
            close(sock);
            usleep(100000); // 100ms delay for server cleanup
        } else {
           // perror("Connection failed");
            close(sock);
        }

        // Update elapsed time
        clock_gettime(CLOCK_MONOTONIC, &now);
        elapsed = (now.tv_sec - start.tv_sec) + (now.tv_nsec - start.tv_nsec) / 1e9;
    }

    // Final statistics
    printf("\nFinal Statistics:\n");
    printf("Total connections: %lu\n", total_connections);
    printf("Total messages sent: %lu\n", total_msgs_sent);
    printf("Total packets sent: %lu\n", total_packets_sent);
    printf("Average messages per connection: %.2f\n", total_connections > 0 ? (double)total_msgs_sent / total_connections : 0.0);
    printf("Running time: %.2f seconds\n", elapsed);
    printf("Messages per second: %.2f\n", elapsed > 0 ? total_msgs_sent / elapsed : 0.0);
    printf("Non-complete connections: %d\n", non_complete);
    printf("Terminated %s\n", elapsed >= runtime ? "due to reaching time limit" : "by user interrupt");

    free(msg_buffer);
    return 0;
}