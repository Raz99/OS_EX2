
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Check if the correct number of arguments is provided
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *hostname = argv[1]; // Hostname or IP address of the server
    const char *port = argv[2]; // Port number to connect to
    char message[BUFFER_SIZE]; // Buffer to hold the message to send

    // Main loop to read commands from the user
    while (1) {
        printf("Enter a command (e.g., ADD HYDROGEN 3) or type \"q\" to quit:\n> ");
        
        if (!fgets(message, BUFFER_SIZE, stdin)) break; // Read user input (break on EOF)

        // Remove newline
        size_t len = strlen(message);
        if (len > 0 && message[len - 1] == '\n') message[len - 1] = '\0';

        if (strcasecmp(message, "q") == 0) break; // Exit if the user types "q"

        // Resolve hostname
        struct addrinfo hints = {0};
        struct addrinfo *res; // Pointer to hold the resolved address info
        hints.ai_family = AF_INET; // Use IPv4
        hints.ai_socktype = SOCK_STREAM; // Use TCP

        int status = getaddrinfo(hostname, port, &hints, &res); // Get address info
        
        // Check if getaddrinfo was successful
        if (status != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
            continue;
        }

        int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol); // Create a socket
        
        // Check if the socket was created successfully
        if (sockfd < 0) {
            perror("socket");
            freeaddrinfo(res); // Free the address info structure
            continue;
        }

        // Connect to the server
        if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
            perror("connect");
            close(sockfd); // Close the socket if connection failed
            freeaddrinfo(res); // Free the address info structure
            continue;
        }

        // Send the message to the server
        if (send(sockfd, message, strlen(message), 0) < 0) {
            perror("send");
        }

        close(sockfd); // Close the socket after sending the message
        freeaddrinfo(res); // Free the address info structure after use
    }

    return 0;
}