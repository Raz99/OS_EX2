#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>

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
    
    // Resolve hostname
    struct addrinfo hints = {0};
    struct addrinfo *res; // Pointer to hold the resolved address info
    hints.ai_family = AF_INET; // Use IPv4
    hints.ai_socktype = SOCK_STREAM; // Use TCP

    int status = getaddrinfo(hostname, port, &hints, &res);
    
    // Check if getaddrinfo was successful
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return EXIT_FAILURE;
    }
    
    // Create the socket
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    
    // Check if the socket was created successfully
    if (sockfd < 0) {
        perror("socket");
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }

    // Connect to the server
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        close(sockfd);
        freeaddrinfo(res);
        return EXIT_FAILURE;
    }
    
    printf("Connected to atom_warehouse server at %s:%s\n", hostname, port);
    freeaddrinfo(res); // Free the address info structure after establishing the connection

    // Main loop to read commands from the user with persistent connection
    while (1) {
        printf("Enter a command (e.g., ADD HYDROGEN 3) or type \"q\" to quit:\n> ");
        
        if (!fgets(message, BUFFER_SIZE, stdin)) break; // Read user input (break on EOF)

        // Remove newline
        size_t len = strlen(message);
        if (len > 0 && message[len - 1] == '\n') message[len - 1] = '\0';

        if (strcasecmp(message, "q") == 0) break; // Exit if the user types "q"

        // Send the message to the server using the persistent connection
        if (send(sockfd, message, strlen(message), 0) < 0) {
            // If send fails, the connection might be broken
            if (errno == EPIPE || errno == ECONNRESET) {
                fprintf(stderr, "Connection lost. Server may have closed the connection.\n");
                break;
            } else {
                perror("send");
                break;
            }
        }
    }
    
    // Close the connection when done
    printf("Closing connection to server.\n");
    close(sockfd);
    
    return 0;
}