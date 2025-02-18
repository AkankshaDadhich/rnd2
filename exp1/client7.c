#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/tcp.h>  // Include this for TCP_NODELAY

#define SERVER_IP "127.0.0.1"
#define PORT 8080

typedef struct {
    int connection_id;
    int total_packets;
    int thread_packets;
} ThreadArgs;

pthread_mutex_t send_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex to control packet sending order
int current_packet = 1;  // Keeps track of the current packet number to send

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
        return NULL;
    }

    // Set TCP_NODELAY for low latency
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

    // Open a file to log the sent packets
    char filename[50];
    snprintf(filename, sizeof(filename), "client_%d_packets.log", targs->connection_id);
    FILE *logfile = fopen(filename, "w");
    if (!logfile) {
        perror("Failed to open file");
        close(sock);
        return NULL;
    }

    for (int i = 1; i <= targs->thread_packets; i++) {
        pthread_mutex_lock(&send_mutex);  // Lock before sending

        char packet[50];
        int len = snprintf(packet, sizeof(packet), "CONN:%d|PKT:%d\n", 
                         targs->connection_id, i);
        
        // Send packet to the server
        send(sock, packet, len, 0);
        
        // Log packet to the file
        fprintf(logfile, "CONN:%d|PKT:%d\n", targs->connection_id, i);

        pthread_mutex_unlock(&send_mutex);  // Unlock after sending
        
        usleep(100000); // Sleep for 100ms (adjust as needed to control packet sending rate)
    }

    fclose(logfile);
    close(sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <packets_per_connection> <iterations>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int packets_per_conn = atoi(argv[1]);
    int iterations = atoi(argv[2]);

    pthread_t threads[2];
    ThreadArgs args[2];

    printf("Starting 2 connections, sending %d packets alternately\n", packets_per_conn);

    for (int i = 0; i < 2; i++) {
        args[i].connection_id = i + 1;
        args[i].total_packets = 2 * packets_per_conn;  // Each thread will send `packets_per_conn` packets
        args[i].thread_packets = packets_per_conn;
        pthread_create(&threads[i], NULL, send_packets, &args[i]);
    }

    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All packets sent\n");
    return 0;
}
