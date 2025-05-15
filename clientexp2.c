#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>

#define SERVER_IP "10.130.154.1"  
#define PORT 8080
#define NUM_MESSAGES 10000
#define BATCH_SIZE 1000 



// Pin thread to a specific CPU core to reduce context switches
void set_thread_affinity(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

// Persistent connection thread: one TCP connection, many messages.
void *persistent_connection(void *arg) {
    set_thread_affinity(0);

    int sock;
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed (persistent)");
        pthread_exit(NULL);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        close(sock);
        pthread_exit(NULL);
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed (persistent)");
        close(sock);
        pthread_exit(NULL);
    }
    
    for (int i = 0; i < NUM_MESSAGES; i++) {
        ssize_t sent = send(sock, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 60, 0);

        if (sent < 0) {
            perror("Send failed (persistent)");
            break;
        }
    }
    
    close(sock);
    pthread_exit(NULL);
}

// Frequent connection thread: sends 10 messages per connection before closing.
void *frequent_connection(void *arg) {
    set_thread_affinity(1);

    for (int i = 0; i < NUM_MESSAGES; i += BATCH_SIZE) {
        int sock;
        struct sockaddr_in server_addr;

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Socket creation failed (frequent)");
            continue;
        }
        
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
            perror("Invalid address / Address not supported");
            close(sock);
            continue;
        }
        
        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Connect failed (frequent)");
            close(sock);
            continue;
        }
        
        for (int j = 0; j < BATCH_SIZE && (i + j) < NUM_MESSAGES; j++) {
            ssize_t sent =  send(sock, "123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 60, 0);

            if (sent < 0) {
                perror("Send failed (frequent)");
                break;
            }
        }
       
        close(sock);
    }
    
    pthread_exit(NULL);
}

int main() {
    pthread_t persistent_tid, frequent_tid;
    
    if (pthread_create(&persistent_tid, NULL, persistent_connection, NULL) != 0) {
        perror("Persistent thread creation failed");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&frequent_tid, NULL, frequent_connection, NULL) != 0) {
        perror("Frequent thread creation failed");
        exit(EXIT_FAILURE);
    }

    pthread_join(persistent_tid, NULL);
    pthread_join(frequent_tid, NULL);
    
    printf("Execution completed\n");
    return 0;
}
