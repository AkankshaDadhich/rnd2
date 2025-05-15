#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
// #define SERVER_IP "10.130.154.1"
#define PORT 8080
#define TCP_CONN 2 // Number of TCP connections
#define PACKETS 20 // Packets per connection

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int global_packet_id = 1; // Global packet counter
int current_conn = 1;     // Tracks which connection should send next

void *send_packets(void *arg)
{
    int conn_id = *(int *)arg;
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = {.sin_family = AF_INET, .sin_port = htons(PORT)};
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("Connection failed");
        free(arg);
        return NULL;
    }
    char msg[50];
    int *tcp_con = (((int *)msg) + 1);
    int *tcp_pac = (((int *)msg) + 4);
    // conn_id, global_packet_id++
    sprintf(msg, "TCP:    |PACKET:    \n");
    for (int i = 0; i < PACKETS; i++)
    {
        pthread_mutex_lock(&lock);
        while (current_conn != conn_id)
        {
            pthread_cond_wait(&cond, &lock);
        }
        *tcp_con = conn_id;            // Convert to network byte order
        *tcp_pac = global_packet_id++; // Convert and increment
        printf("tcp_con: %d, tcp_pac: %d\n", *tcp_con, *tcp_pac);
        send(sock, msg, 22, 0);

        for (int i = 0; i<=22; i++)
        {
            printf("%c", msg[i]);
        }

        // printf("%c", *msg);
        // fwrite(msg, sizeof(char), 22, stdout);

        current_conn = (current_conn % TCP_CONN) + 1; // Move to the next connection
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&lock);
    }

    close(sock);
    free(arg);
    return NULL;
}

int main()
{
    pthread_t threads[TCP_CONN];

    for (int i = 0; i < TCP_CONN; i++)
    {
        int *conn_id = malloc(sizeof(int));
        *conn_id = i + 1;
        pthread_create(&threads[i], NULL, send_packets, conn_id);
    }

    for (int i = 0; i < TCP_CONN; i++)
    {
        pthread_join(threads[i], NULL);
    }

    printf("All packets sent in strict TCP connection order.\n");
    return 0;
}
