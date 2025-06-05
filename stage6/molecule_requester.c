#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <stdbool.h>

#define BUFFER_SIZE 1024

// Global variables for command line options
extern int optopt;
extern char *optarg;

int main(int argc, char *argv[]) {
    const char *hostname = NULL;
    const char *port = NULL;
    char *uds_path = NULL;
    bool seen_flags[256] = { false }; // Track seen flags to avoid duplicates

    while (1) {
        int ret = getopt(argc, argv, "h:p:f:");

        if (ret == -1)
        {
            break;
        }

        if (seen_flags[ret])
        {
            fprintf(stderr, "Error: Duplicate flag -%c\n", ret);
            exit(EXIT_FAILURE);
        }
        
        seen_flags[ret] = true; // Mark this flag as seen

        switch (ret) {
            case 'h':
                hostname = optarg;
                break;
            case 'p':
                port = optarg;
                break;
            case 'f':
                uds_path = strdup(optarg); // Duplicate the string so it can be used after optarg is modified
                // Append .socket if not already present
                if (!strstr(uds_path, ".socket")) {
                    char *new_path = malloc(strlen(uds_path) + 8); // +8 for ".socket\0"
                    if (new_path) {
                        sprintf(new_path, "%s.socket", uds_path);
                        free(uds_path);
                        uds_path = new_path;
                    }
                }
                break;
            case '?':
                printf("Usage: %s [-h <hostname/IP> -p <port>] | [-f <uds_path>]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if ((uds_path && (hostname || port)) || (!uds_path && (!hostname || !port))) {
        fprintf(stderr, "Error: Provide either -f <uds_path> or both -h <hostname> and -p <port>\n");
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
        printf("Connected to drinks_bar server at %s:%s (UDP)\n", hostname, port);
    }

    if (uds_path) {
        // Check if the server socket exists
        if (access(uds_path, F_OK) != 0) {
            fprintf(stderr, "Error: Server socket '%s' does not exist\n", uds_path);
            exit(EXIT_FAILURE);
        }

        sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
        
        if (sockfd < 0) {
            perror("UDS socket");
            exit(EXIT_FAILURE);
        }

        // Create a client-side socket path for responses
        char client_path[108];
        snprintf(client_path, sizeof(client_path), "molecule_client_%d.socket", getpid());
        unlink(client_path); // Remove socket if it exists
        
        // Bind client socket to its own path
        struct sockaddr_un client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        client_addr.sun_family = AF_UNIX;
        strncpy(client_addr.sun_path, client_path, sizeof(client_addr.sun_path) - 1);
        
        if (bind(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
            perror("bind");
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        
        // Set up server address (this part is the same as your original code)
        struct sockaddr_un *uds_addr = (struct sockaddr_un *)&addr;
        memset(uds_addr, 0, sizeof(struct sockaddr_un));
        uds_addr->sun_family = AF_UNIX;
        strncpy(uds_addr->sun_path, uds_path, sizeof(uds_addr->sun_path) - 1);
        addr_len = sizeof(struct sockaddr_un);
        printf("Connected to drinks_bar server via Unix socket: %s\n", uds_path);
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
        if (sendto(sockfd, message, strlen(message), 0, (struct sockaddr*)&addr, addr_len) < 0) {
            perror("sendto");
            continue;
        }

        else {
            // Receive response from server
            char response[BUFFER_SIZE] = {0}; // Buffer to hold the response
            struct sockaddr_storage source_addr;
            socklen_t source_len = sizeof(source_addr);

            int bytes_received = recvfrom(sockfd, response, BUFFER_SIZE - 1, 0, (struct sockaddr*)&source_addr, &source_len);
            
            // Check if the response was received successfully
            if (bytes_received > 0) {
                response[bytes_received] = '\0';
                printf("Server response: %s\n", response);
            }
        }
    }

    // Clean up: close the socket and unlink the UDS path if necessary
    if (uds_path) {
        // Get the bound path and unlink it
        struct sockaddr_un addr;
        socklen_t len = sizeof(addr);
        if (getsockname(sockfd, (struct sockaddr*)&addr, &len) == 0) {
            unlink(addr.sun_path);
        }
    }

    printf("Closing connection to server.\n");
    close(sockfd); // Close the socket
    return 0;
}