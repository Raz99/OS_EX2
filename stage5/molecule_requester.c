#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/socket.h>

#define BUFFER_SIZE 1024

// Global variables for command line options
extern int optopt;
extern char *optarg;

int main(int argc, char *argv[]) {
    const char *hostname = NULL;
    const char *port = NULL;
    const char *uds_path = NULL;

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
            case 'f':
                uds_path = optarg;
                break;
            case '?':
                printf("Usage: %s [-h <hostname/IP> -p <port>] | [-f <uds_path>]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if ((uds_path && (hostname || port)) || (!uds_path && (!hostname || !port))) {
        fprintf(stderr, "Error: Provide either -f <uds_path> or both -h <hostname> and -p <port>, but not both.\n");
        exit(EXIT_FAILURE);
    }

    int sockfd = -1;
    struct sockaddr_storage addr;
    socklen_t addr_len = 0;

    if (hostname && port) {
        struct addrinfo hints = {0};
        struct addrinfo *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        int status = getaddrinfo(hostname, port, &hints, &res);
        if (status != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
            return EXIT_FAILURE;
        }
        sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd < 0) {
            perror("UDP socket");
            freeaddrinfo(res);
            return EXIT_FAILURE;
        }
        memcpy(&addr, res->ai_addr, res->ai_addrlen);
        addr_len = res->ai_addrlen;
        freeaddrinfo(res);
        printf("Using UDP to send messages to %s:%s\n", hostname, port);
    }

    if (uds_path) {
        sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (sockfd < 0) {
            perror("UDS socket");
            exit(EXIT_FAILURE);
        }
        struct sockaddr_un *uds_addr = (struct sockaddr_un *)&addr;
        memset(uds_addr, 0, sizeof(struct sockaddr_un));
        uds_addr->sun_family = AF_UNIX;
        strncpy(uds_addr->sun_path, uds_path, sizeof(uds_addr->sun_path) - 1);
        addr_len = sizeof(struct sockaddr_un);
        printf("Using UDS to send messages to %s\n", uds_path);
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

        // Send the message and receive response
        if (sendto(sockfd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen) < 0) {
            perror("sendto");
            continue;
        }
        char response[BUFFER_SIZE] = {0};
        int bytes_received = recvfrom(sockfd, response, BUFFER_SIZE - 1, 0, NULL, NULL);
        if (bytes_received > 0) {
            response[bytes_received] = '\0';
            printf("Server response: %s\n", response);
        }
    }
        
        close(sockfd); // Close the UDP socket after sending the message
        freeaddrinfo(res); // Free the address info structure after use
    

    return 0;
    
}