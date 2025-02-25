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
int pattern[] = {2, 1, 1, 1, 2, 1, 2, 1, 2, 1, 1}; // The desired pattern
int pattern_length = sizeof(pattern) / sizeof(pattern[0]);
int pattern_index = 0;

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
        while (pattern[pattern_index] != conn_id) {
            pthread_cond_wait(&cond, &lock);
        }

        char msg[50];
        sprintf(msg, "TCP:%d | PACKET:%d\n", conn_id, global_packet_id++);
        send(sock, msg, strlen(msg), 0);
        printf("%s", msg);

        // Move to the next index in the pattern
        pattern_index = (pattern_index + 1) % pattern_length;

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
