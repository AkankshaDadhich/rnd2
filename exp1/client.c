#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"

void send_packet(int sock, const char *label, int num) {
    char message[32];
    snprintf(message, sizeof(message), "%s-%d\n", label, num);
    send(sock, message, strlen(message), 0);
    printf("Sent: %s\n", message);
   // sleep(1);  // Simulate delay
}

int main() {
    int sock1, sock2, sock3, sock4;
    struct sockaddr_in server_addr;

    // Create first TCP connection
    sock1 = socket(AF_INET, SOCK_STREAM, 0);
    if (sock1 == -1) {
        perror("Socket 1 creation failed");
        exit(EXIT_FAILURE);
    }

    // Create second TCP connection
    sock2 = socket(AF_INET, SOCK_STREAM, 0);
    if (sock2 == -1) {
        perror("Socket 2 creation failed");
        exit(EXIT_FAILURE);
    }

    sock3 = socket(AF_INET, SOCK_STREAM, 0);
    if (sock3 == -1) {
        perror("Socket 2 creation failed");
        exit(EXIT_FAILURE);
    }

    sock4 = socket(AF_INET, SOCK_STREAM, 0);
    if (sock4 == -1) {
        perror("Socket 2 creation failed");
        exit(EXIT_FAILURE);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Connect first socket
    if (connect(sock1, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection 1 failed");
        exit(EXIT_FAILURE);
    }

    // Connect second socket
    if (connect(sock2, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection 2 failed");
        exit(EXIT_FAILURE);
    }


    if (connect(sock3, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection 3 failed");
        exit(EXIT_FAILURE);
    }

       if (connect(sock4, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection 4 failed");
        exit(EXIT_FAILURE);
    }



    printf("Client connected with two TCP connections\n");

    // Enforce the specific order
    send_packet(sock1, "client 1: TCP1 ", 1);
    send_packet(sock1, "client 1: TCP1 ", 2);

    send_packet(sock2, "client 1: TCP2 ", 1);
    send_packet(sock2, "client 1: TCP2 ", 2);

    send_packet(sock1, "client 1: TCP1 ", 3);
    send_packet(sock1, "client 1: TCP1 ", 4);

    send_packet(sock2, "client 1:  TCP2", 3);
    send_packet(sock2, "client 1:  TCP2", 4);

    send_packet(sock3, "client 1:  TCP3", 1);
    send_packet(sock3, "client 1:  TCP3", 2);

    send_packet(sock4, "client 1:  TCP4", 1);
    send_packet(sock4, "client 1:  TCP4", 2);
    
    send_packet(sock3, "client 1:  TCP3", 3);
    send_packet(sock3, "client 1:  TCP3", 4);

    send_packet(sock4, "client 1:  TCP4", 3);
    send_packet(sock4, "client 1:  TCP4", 4);


    close(sock1);
    close(sock2);
    close(sock3);
    close(sock4);

    return 0;
}
