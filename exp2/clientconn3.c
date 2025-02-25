#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define TCP_CONN 3       // Number of TCP connections
#define PACKETS 9        // Packets for the initial pattern (123, 231, 321)
#define REPEAT_COUNT 12  // 123 repetition count (4 cycles of 123)

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int global_packet_id = 1;
int current_conn = 1;
int order_index = 0;
int pattern_phase = 1;

// Predefined order sequence for the pattern phase
int sequence[] = {1, 2, 3, 2, 3, 1, 3, 2, 1};
int seq_len = sizeof(sequence) / sizeof(sequence[0]);

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

    for (int i = 0; i < (PACKETS + REPEAT_COUNT); i++) {
        pthread_mutex_lock(&lock);

        // Handle the pattern phase
        if (pattern_phase) {
            if (current_conn != sequence[order_index]) {
                pthread_cond_wait(&cond, &lock);
            }
        }
        // Handle the repeating 123 phase
        else {
            if (current_conn != conn_id) {
                pthread_cond_wait(&cond, &lock);
            }
        }

        char msg[50];
        sprintf(msg, "TCP:%d | PACKET:%d\n", conn_id, global_packet_id++);
        send(sock, msg, strlen(msg), 0);
        printf("%s", msg);

        // Update the order for pattern phase
        if (pattern_phase) {
            order_index++;
            if (order_index >= seq_len) {
                pattern_phase = 0;  // Switch to repeating 123 phase
                current_conn = 1;   // Start 123 from 1
            } else {
                current_conn = sequence[order_index];
            }
        }
        // Update the order for repeating 123 phase
        else {
            current_conn = (current_conn % TCP_CONN) + 1;
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

    return 0;
}
