#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/tcp.h>  // For TCP_NODELAY

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define TCP_CONN 4  // Ensure 4 connections are created

typedef struct {
    int connection_id;
    int thread_packets;
} ThreadArgs;

pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex for controlling order
pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;   // Mutex for logging
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;          // Condition variable for ordering

int current_packet = 1;  // Global counter to track the order
int current_conn = 1;    // Track which connection should send next

void *send_packets(void *args) {
    ThreadArgs *targs = (ThreadArgs *)args;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };
    
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);
    
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        free(targs);
        return NULL;
    }

    // Enable TCP_NODELAY for low latency
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

    for (int i = 1; i <= targs->thread_packets; i++) {
        pthread_mutex_lock(&send_mutex);

        // Wait until it's this connection's turn
        while (current_conn != targs->connection_id || current_packet != i) {
            pthread_cond_wait(&cond, &send_mutex);
        }

        char packet[50];
        int len = snprintf(packet, sizeof(packet), "CONN:%d|PKT:%d\n", 
                         targs->connection_id, i);
        
        // Send packet to the server

        // Log packet to a single file
        pthread_mutex_lock(&log_mutex);
        FILE *logfile = fopen("client_packets.log", "a");
        if (logfile) {
            fprintf(logfile, "CONN:%d|PKT:%d\n", targs->connection_id, i);
            fflush(logfile);
            fclose(logfile);
        } else {
            perror("Failed to open log file");
        }
        pthread_mutex_unlock(&log_mutex);

        // Update current connection for round-robin order
        if (current_conn == TCP_CONN) {
            current_conn = 1;
            current_packet++;  // Move to next packet round
        } else {
            current_conn++;
        }

        send(sock, packet, len, 0);


        pthread_cond_broadcast(&cond);  
        pthread_mutex_unlock(&send_mutex);

        usleep(100000); 
    }

    close(sock);
    free(targs);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <packets_per_connection> <iterations>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int packets_per_conn = atoi(argv[1]);
    int iterations = atoi(argv[2]);

    pthread_t threads[TCP_CONN];

    // Clear the log file before starting
    FILE *logfile = fopen("client_packets.log", "w");
    if (logfile) fclose(logfile);

    printf("Starting %d connections, each sending %d packets in order\n", TCP_CONN, packets_per_conn);

    for (int i = 0; i < TCP_CONN; i++) {
        ThreadArgs *targs = (ThreadArgs *)malloc(sizeof(ThreadArgs));
        if (!targs) {
            perror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        targs->connection_id = i + 1;
        targs->thread_packets = packets_per_conn;

        pthread_create(&threads[i], NULL, send_packets, targs);
    }

    for (int i = 0; i < TCP_CONN; i++) {  
        pthread_join(threads[i], NULL);
    }

    printf("All packets sent in order\n");
    return 0;
}
