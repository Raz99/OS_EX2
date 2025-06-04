#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>

#define BUFFER_SIZE 1024

// Global variables for command line options
extern int optopt;
extern char *optarg;

int main(int argc, char *argv[]) {
    const char *hostname = NULL;
    const char *port = NULL;
    const char *uds_path = NULL;

    while (1) {
        int ret = getopt(argc, argv, "h:p:f");

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
            case 'f':
                uds_path = optarg;
                break;    
            case '?':
                printf("Usage: %s -h <hostname/IP> -p <port> | -f <unix_socket_path>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }
    char message[BUFFER_SIZE]; // Buffer to hold the message to send
    int sockfd;
    
    if ((uds_path && (hostname || port)) || (!uds_path && (!hostname || !port))) {
        fprintf(stderr, "Error: Provide either -f <uds_path> or both -h <hostname> and -p <port>, but not both.\n");
        exit(EXIT_FAILURE);
    }

    if (uds_path){
        struct sockaddr_un addr;
        sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
        }
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, uds_path, sizeof(addr.sun_path) - 1);

        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("connect (UDS)");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        printf("Connected to drinks_bar server via Unix socket: %s\n", uds_path);

    }
    

    else if (hostname && port) {
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
    
    printf("Connected to drinks_bar server at %s:%s\n", hostname, port);
    freeaddrinfo(res); // Free the address info structure after establishing the connection

    } else {
        fprintf(stderr, "Error: you must specify either -h <host> and -p <port> OR -f <socket_path>\n");
        exit(EXIT_FAILURE);
    }

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