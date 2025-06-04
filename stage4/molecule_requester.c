#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

// Global variables for command line options
extern int optopt;
extern char *optarg;

int main(int argc, char *argv[]) {
    const char *hostname = NULL;
    const char *port = NULL;

    while (1) {
        int ret = getopt(argc, argv, "h:p:");

        if (ret == -1)
        {
            break;
        }

        switch (ret) {
            case 'h':
                hostname = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case '?':
                printf("Usage: %s -h <hostname/IP> -p <port>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!hostname || !port) {
        printf("Error: -h <hostname/IP> and -p <port> are required.\n");
        exit(EXIT_FAILURE);
    }

    char message[BUFFER_SIZE]; // Buffer to hold the message to send

    // Main loop to read commands from the user
    while (1) {
        printf("Enter a command (e.g., DELIVER WATER 3) or type \"q\" to quit:\n> ");
        
        if (!fgets(message, BUFFER_SIZE, stdin)) break; // Read user input (break on EOF)

        // Remove newline
        size_t len = strlen(message);
        if (len > 0 && message[len - 1] == '\n') message[len - 1] = '\0';

        if (strcasecmp(message, "q") == 0) break; // Exit if the user types "q"

        // Resolve hostname
        struct addrinfo hints = {0};
        struct addrinfo *res; // Pointer to hold the resolved address info
        hints.ai_family = AF_INET; // Use IPv4
        hints.ai_socktype = SOCK_DGRAM; // Use UDP

        int status = getaddrinfo(hostname, port, &hints, &res); // Get address info
        
        // Check if getaddrinfo was successful
        if (status != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
            continue;
        }

        int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol); // Create a UDP socket
        
        // Check if the socket was created successfully
        if (sockfd < 0) {
            perror("socket");
            freeaddrinfo(res); // Free the address info structure
            continue;
        }

        // Send the message and receive response
        if (sendto(sockfd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen) < 0) {
            perror("sendto");
        }

        else {
            // Receive response from server
            char response[BUFFER_SIZE] = {0}; // Buffer to hold the response
            int bytes_received = recvfrom(sockfd, response, BUFFER_SIZE - 1, 0, NULL, NULL); // Receive response
            
            // Check if the response was received successfully
            if (bytes_received > 0) {
                response[bytes_received] = '\0';
                printf("Server response: %s", response);
            }
        }

        close(sockfd); // Close the UDP socket after sending the message
        freeaddrinfo(res); // Free the address info structure after use
    }

    return 0;
}