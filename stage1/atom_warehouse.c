
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>

#define MAX_CLIENTS 10 // Maximum number of concurrent clients
#define BUFFER_SIZE 1024 // Size of the buffer for reading client requests

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
    else return 0; // Unknown atom type
    return 1; // Successfully added atoms
}

void handle_client(int fd, AtomWarehouse *warehouse) {
    char buffer[BUFFER_SIZE] = {0}; // Buffer to hold the incoming data
    int bytes_read = read(fd, buffer, BUFFER_SIZE - 1); // Read data from the client (leaving space for null terminator)
    
    // In case of an error or no data read, close the connection
    if (bytes_read <= 0) {
        close(fd);
        return;
    }

    char command[16], atom[16]; // Buffers for command and atom type
    unsigned long long amount; // Variable to hold the amount of atoms
    int parsed = sscanf(buffer, "%15s %15s %llu", command, atom, &amount); // Parse the command, atom type, and amount from the buffer

    // Check if the command is valid
    if (parsed != 3 || strcmp(command, "ADD") != 0) {
        printf("Invalid command: %s", buffer);
        return;
    }

    // Check if the amount is valid
    if (!add_atoms(warehouse, atom, amount)) {
        printf("Unknown atom type: %s\n", atom);
        return;
    }

    print_status(warehouse); // Print the current status of the warehouse
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

    int listener = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket
    
    // Check if the socket was created successfully
    if (listener < 0) {
        perror("socket");
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
    if (listen(listener, MAX_CLIENTS) < 0) {
        perror("listen");
        close(listener);
        exit(EXIT_FAILURE);
    }

    struct pollfd fds[MAX_CLIENTS + 1]; // Array of poll file descriptors
    fds[0].fd = listener; // The first element is the listener socket
    fds[0].events = POLLIN; // Set the listener to poll for incoming connections
    for (int i = 1; i <= MAX_CLIENTS; i++) {
        fds[i].fd = -1; // Initialize the rest of the fds to -1 (no client connected)
    }

    printf("atom_warehouse server started on port %d\n", port); // Print server start message

    // Main loop to accept and handle client connections
    while (1) {
        int ready = poll(fds, MAX_CLIENTS + 1, -1); // Wait indefinitely for events on the file descriptors
        
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

            // Find an empty slot in the fds array for the new client
            for (int i = 1; i <= MAX_CLIENTS; i++) {
                if (fds[i].fd == -1) {
                    fds[i].fd = client_fd; // Assign the new client file descriptor
                    fds[i].events = POLLIN; // Set the new client to poll for incoming data
                    break;
                }
            }
        }

        // Iterate through the file descriptors to handle client requests
        for (int i = 1; i <= MAX_CLIENTS; i++) {
            // Check if the file descriptor is valid and has data to read
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
                handle_client(fds[i].fd, &warehouse); // Handle the client request
                fds[i].fd = -1; // Reset the file descriptor to -1 after handling the request
            }
        }
    }

    close(listener); // Close the listener socket (this line will never be reached in this infinite loop)
    return 0;
}