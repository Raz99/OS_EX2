#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>

#define BUFFER_SIZE 1024 // Size of the buffer for reading client requests

// Global variables for cleanup
struct pollfd *fds = NULL;
int listener = -1;
int running = 1;

// Signal handler for graceful shutdown
void handle_signal(int sig) {
    running = 0;
}

typedef struct {
    unsigned long long carbon;
    unsigned long long oxygen;
    unsigned long long hydrogen;
} AtomWarehouse;

void print_status(AtomWarehouse *w) {
    printf("Atom Warehouse Status:\n");
    printf("Carbon: %llu\n", w->carbon);
    printf("Oxygen: %llu\n", w->oxygen);
    printf("Hydrogen: %llu\n", w->hydrogen);
}

int add_atoms(AtomWarehouse *w, const char *atom, unsigned long long amount) {
    if (strcmp(atom, "CARBON") == 0) w->carbon += amount;
    else if (strcmp(atom, "OXYGEN") == 0) w->oxygen += amount;
    else if (strcmp(atom, "HYDROGEN") == 0) w->hydrogen += amount;
    else return 1; // Unknown atom type
    return 0; // Successfully added atoms
}

int handle_client(int fd, AtomWarehouse *warehouse) {
    char buffer[BUFFER_SIZE] = {0}; // Buffer to hold the incoming data
    int bytes_read = read(fd, buffer, BUFFER_SIZE - 1); // Read data from the client (leaving space for null terminator)
    
    // In case of an error or no data read, close the connection
    if (bytes_read <= 0) {
        close(fd);
        return 1; // Connection closed
    }

    char command[16], atom[16]; // Buffers for command and atom type
    unsigned long long amount; // Variable to hold the amount of atoms
    int parsed = sscanf(buffer, "%15s %15s %llu", command, atom, &amount); // Parse the command, atom type, and amount from the buffer

    // Check if the command is valid
    if (parsed != 3 || strcmp(command, "ADD") != 0) {
        printf("Invalid command: %s", buffer);
        return 0; // Connection still open
    }

    // Check if the amount is valid
    if (add_atoms(warehouse, atom, amount)) {
        printf("Unknown atom type: %s\n", atom);
        return 0; // Connection still open
    }

    print_status(warehouse); // Print the current status of the warehouse
    return 0; // Connection still open
}

int main(int argc, char *argv[]) {
    // Check if the correct number of arguments is provided
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]); // Convert the port argument to an integer

    // Validate the port number
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    AtomWarehouse warehouse = {0, 0, 0}; // Initialize the warehouse with zero atoms

    // Set up signal handlers for graceful shutdown
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    listener = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket
    
    // Check if the socket was created successfully
    if (listener < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set socket option to reuse address to avoid "address already in use"
    int opt = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(listener);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address = {0}; // Initialize the address structure
    socklen_t addrlen = sizeof(address); // Size of the address structure
    address.sin_family = AF_INET; // Set the address family to IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Bind to any available address
    address.sin_port = htons(port); // Convert the port number to network byte order

    // Bind the socket to the address and port
    if (bind(listener, (struct sockaddr *)&address, addrlen) < 0) {
        perror("bind");
        close(listener);
        exit(EXIT_FAILURE);
    }

    // Set the socket to listen for incoming connections
    if (listen(listener, SOMAXCONN) < 0) { // SOMAXCONN is the maximum queue length for pending connections
        perror("listen");
        close(listener);
        exit(EXIT_FAILURE);
    }

    // Allocate memory for the poll file descriptors
    int fds_capacity = 10;
    fds = malloc(fds_capacity * sizeof(struct pollfd));
    if (!fds) {
        perror("malloc");
        close(listener);
        exit(EXIT_FAILURE);
    }

    int nfds = 1; // Number of valid file descriptors
    fds[0].fd = listener; // The first element is the listener socket
    fds[0].events = POLLIN; // Set the listener to poll for incoming connections

    printf("atom_warehouse server started on port %d (Use CTRL+C to shut down)\n", port); // Print server start message

    // Main loop to accept and handle client connections
    while (running) {
        int ready = poll(fds, nfds, 1000); // Poll with a timeout so we can check running flag
        
        if (!running) break; // Check if we need to exit

        // Check if poll was successful
        if (ready < 0) {
            perror("poll");
            continue;
        }

        // Check if the listener socket has incoming connections
        if (fds[0].revents & POLLIN) {
            int client_fd = accept(listener, NULL, NULL); // Accept a new client connection
            
            // Check if the accept was successful
            if (client_fd < 0) {
                perror("accept");
                continue;
            }

            // Resize the array if we're out of space
            if (nfds >= fds_capacity) {
                fds_capacity *= 2;  // Double the capacity
                struct pollfd *new_fds = realloc(fds, fds_capacity * sizeof(struct pollfd));
                if (!new_fds) {
                    perror("realloc");
                    close(client_fd);
                    continue;
                }
                fds = new_fds;
            }

            // Add the new client to the end of the array
            fds[nfds].fd = client_fd;
            fds[nfds].events = POLLIN;
            nfds++;
        }

        // Iterate through the file descriptors to handle client requests
        for (int i = 1; i < nfds; i++) {
            // Check if this fd has data to read
            if (fds[i].revents & POLLIN) {
                int connection_closed = handle_client(fds[i].fd, &warehouse); // Handle the client request
                
                if (connection_closed) {
                    // Shift remaining elements to fill the gap
                    for (int j = i; j < nfds - 1; j++) {
                        fds[j] = fds[j + 1];
                    }
                    nfds--;
                    i--; // Adjust index since we removed an element
                }
            }
        }
    }

    // Clean up: close all client sockets and free resources
    for (int i = 0; i < nfds; i++) {
        if (fds[i].fd >= 0) {
            close(fds[i].fd); // Close each socket
        }
    }
    free(fds); // Free the allocated memory for file descriptors
    printf("Server shut down successfully.\n");
    return 0;
}