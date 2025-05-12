#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sched.h>
#include <netinet/tcp.h>
#include <sys/time.h>

#define SERVER_IP "10.130.154.1"
#define PORT 8080
#define TCP_CONN 2 // Number of TCP connections
#define EXPERIMENT_DURATION 60 // 1 hour in seconds
//#define MIN_PACKETS 16000 // Minimum packets per connection
//#define EXPERIMENT_DURATION 120 // 1 hour in seconds
#define MIN_PACKETS 16000000 // Minimum packets per connection



int successful_responses[TCP_CONN] = {0}; // Track packets per connection
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int global_packet_id = 1; 
int current_conn = 1;       
char msg[63];

void set_thread_affinity(int conn_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        perror("pthread_setaffinity_np");
    }
}

void *send_packets(void *arg) {
    int conn_id = *(int *)arg;
    int conn_index = conn_id - 1; // For array indexing
    set_thread_affinity(conn_index);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr = { .sin_family = AF_INET, .sin_port = htons(PORT) };
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        free(arg);
        return NULL;
    }
    int flag = 1;
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
    int sndbuf = 1024 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    struct timeval start_time;
    gettimeofday(&start_time, NULL);

    while (1) {
        // Check if 1 hour has elapsed
        struct timeval current_time;
        gettimeofday(&current_time, NULL);
        double elapsed = (current_time.tv_sec - start_time.tv_sec) + 
                         (current_time.tv_usec - start_time.tv_usec) / 1000000.0;
        if (elapsed >= EXPERIMENT_DURATION) {
            printf("Connection %d: Stopping after 1 hour with %d packets sent\n", 
                   conn_id, successful_responses[conn_index]);
            break;
        }

        pthread_mutex_lock(&lock);
        while (current_conn != conn_id) {
            pthread_cond_wait(&cond, &lock);
        }
 
       //send(sock, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 1024, 0);
      // send(sock, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 480, 0);
      //send(sock, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 240, 0); 
     // send(sock, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012", 120, 0);
      send(sock, "123456789012345678901234567890123456789012345678901234567890", 60, 0);
 
      successful_responses[conn_index]++;
        current_conn = (current_conn % TCP_CONN) + 1; 
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&lock);
    }

    close(sock);
    free(arg);
    return NULL;
}

int main() {
    pthread_t threads[TCP_CONN];
    struct timeval total_start, total_end;
    gettimeofday(&total_start, NULL);

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) != 0) {
        perror("sched_setaffinity (main)");
    }

    for (int i = 0; i < TCP_CONN; i++) {
        int *conn_id = malloc(sizeof(int));
        *conn_id = i + 1;
        pthread_create(&threads[i], NULL, send_packets, conn_id);
    }

    for (int i = 0; i < TCP_CONN; i++) {
        pthread_join(threads[i], NULL);
    }

    gettimeofday(&total_end, NULL);
    double total_time = (total_end.tv_sec - total_start.tv_sec) + 
                        (total_end.tv_usec - total_start.tv_usec) / 1000000.0;

    // Calculate total packets and verify minimum
    int total_responses = successful_responses[0] + successful_responses[1];
    printf("Successful Responses (Conn1) = %d\n", successful_responses[0]);
    printf("Successful Responses (Conn2) = %d\n", successful_responses[1]);
    printf("Total Successful Responses  = %d\n", total_responses);
    if (successful_responses[0] < MIN_PACKETS || successful_responses[1] < MIN_PACKETS) {
        printf("Warning: One or both connections sent fewer than %d packets\n", MIN_PACKETS);
    }

    uint64_t total_data_bytes = total_responses * 480; // 60 bytes per packet
    double throughput_mbps = (total_data_bytes * 8) / (total_time * 1e6);
    printf("Total experiment time = %f seconds\n", total_time);
    printf("Throughput            = %f Mbps\n", throughput_mbps);
    printf("Throughput            = %f packets/sec\n", (total_responses / total_time));
    printf("All packets sent in strict TCP connection order.\n");

    return 0;
}