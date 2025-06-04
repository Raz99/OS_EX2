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
int tcp_listener = -1;
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

int deliver_molecules(AtomWarehouse *w, const char *molecule, unsigned long long amount)
{
    if (strcmp(molecule, "WATER") == 0)
    {
        if (w->hydrogen < 2 * amount || w->oxygen < amount)
            return -1; // Not enough atoms to create water

        w->hydrogen -= 2 * amount;
        w->oxygen -= amount;
    }

    else if (strcmp(molecule, "CARBON DIOXIDE") == 0)
    {
        if (w->carbon < amount || w->oxygen < 2 * amount)
            return -1; // Not enough atoms to create carbon dioxide

        w->carbon -= amount;
        w->oxygen -= 2 * amount;
    }

    else if (strcmp(molecule, "ALCOHOL") == 0)
    {
        if (w->carbon < 2 * amount || w->hydrogen < 6 * amount || w->oxygen < amount)
            return -1; // Not enough atoms to create alcohol

        w->carbon -= 2 * amount;
        w->hydrogen -= 6 * amount;
        w->oxygen -= amount;
    }

    else if (strcmp(molecule, "GLUCOSE") == 0)
    {
        if (w->carbon < 6 * amount || w->hydrogen < 12 * amount || w->oxygen < 6 * amount)
            return -1; // Not enough atoms to create glucose

        w->carbon -= 6 * amount;
        w->hydrogen -= 12 * amount;
        w->oxygen -= 6 * amount;
    }

    else return 1; // Unknown molecule type
    return 0; // Successfully added molecules
}

void handle_udp_client(int fd, AtomWarehouse *warehouse) {
    char buffer[BUFFER_SIZE] = {0}; // Buffer to hold the incoming data
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    // Read data from the client (leaving space for null terminator)
    int bytes = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addrlen);

    if (bytes <= 0) {
        perror("recvfrom");
        close(fd);
        return;
    }

    buffer[bytes] = '\0'; // Ensure null-termination of the received string

    // Parse command for DELIVER
    char command[16], molecule[32];
    unsigned long long amount;

    // Used %[^0-9] to read everything that's not a digit as molecule name
    int parsed = sscanf(buffer, "%15s %31[^0-9] %llu", command, molecule, &amount);

    // Check if the command is valid
    if (parsed != 3 || strcmp(command, "DELIVER") != 0) {
        printf("Invalid UDP command: %s\n", buffer);
        const char *msg = "ERROR: Invalid command\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        return;
    }

    // Trim trailing spaces from molecule name
    int len = strlen(molecule);
    while (len > 0 && molecule[len-1] == ' ') {
        molecule[--len] = '\0';
    }

    // Attempt to deliver molecules
    int result = deliver_molecules(warehouse, molecule, amount);

    if (result == 0) {
        const char *msg = "DELIVERED\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        printf("UDP: Delivered %llu %s molecules\n", amount, molecule);
    }
    
    else if (result == 1) {
        const char *msg = "ERROR: Unknown molecule type\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        printf("UDP: Unknown molecule type: %s\n", molecule);
    }
    
    else if (result == -1) {
        const char *msg = "NOT ENOUGH ATOMS\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        printf("UDP: Not enough atoms for %llu %s molecules\n", amount, molecule);
    }

    print_status(warehouse); // Print the current status of the warehouse
}


int handle_tcp_client(int fd, AtomWarehouse *warehouse)
{
    char buffer[BUFFER_SIZE] = {0}; // Buffer to hold the incoming data
    int bytes_read = read(fd, buffer, BUFFER_SIZE - 1); // Read data from the client (leaving space for null terminator)

    // In case of an error or no data read, close the connection
    if (bytes_read <= 0)
    {
        close(fd);
        return 1; // Connection closed
    }

    char command[16], atom[16]; // Buffers for command and atom type
    unsigned long long amount; // Variable to hold the amount of atoms
    int parsed = sscanf(buffer, "%15s %15s %llu", command, atom, &amount); // Parse the command, atom type, and amount from the buffer

    // Check if the command is valid
    if (parsed != 3 || strcmp(command, "ADD") != 0)
    {
        printf("Invalid TCP command: %s", buffer);
        return 0; // Connection still open
    }

    // Check if the amount is valid
    if (add_atoms(warehouse, atom, amount))
    {
        printf("TCP: Unknown atom type: %s\n", atom);
        return 0; // Connection still open
    }

    print_status(warehouse); // Print the current status of the warehouse
    return 0; // Connection still open
}

int main(int argc, char *argv[]) {
    // Check if the correct number of arguments is provided
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <TCP port> <UDP port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int tcp_port = atoi(argv[1]); // Convert the port argument to an integer
    int udp_port = atoi(argv[2]); // Convert the port argument to an integer

    // Validate the port number
    if (tcp_port <= 0 || tcp_port > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    if (udp_port <= 0 || udp_port > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    AtomWarehouse warehouse = {0, 0, 0}; // Initialize the warehouse with zero atoms

    // Set up signal handlers for graceful shutdown
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    int tcp_listener = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket

    // Check if the socket was created successfully
    if (tcp_listener < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Set socket option to reuse address to avoid "address already in use"
    int opt = 1;
    if (setsockopt(tcp_listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(tcp_listener);
        exit(EXIT_FAILURE);
    }

    int udp_listener = socket(AF_INET, SOCK_DGRAM, 0); // Create a UDP socket

    // Check if the UDP socket was created successfully
    if (udp_listener < 0)
    {
        perror("socket");
        close(tcp_listener); // Close the TCP socket if UDP socket creation failed
        close(udp_listener); // Close the UDP socket
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in tcp_address = {0}; // Initialize the address structure
    socklen_t tcp_addrlen = sizeof(tcp_address);// Size of the address structure
    tcp_address.sin_family = AF_INET; // Set the address family to IPv4
    tcp_address.sin_addr.s_addr = INADDR_ANY; // Bind to any available address
    tcp_address.sin_port = htons(tcp_port); // Convert the port number to network byte order

    // Bind the socket to the address and port (TCP)
    if (bind(tcp_listener, (struct sockaddr *)&tcp_address, tcp_addrlen) < 0) {
        perror("bind");
        close(tcp_listener); // Close the TCP socket
        close(udp_listener); // Close the UDP socket
        exit(EXIT_FAILURE);
    }

    // For UDP binding, create a new address structure
    struct sockaddr_in udp_address = {0}; // Initialize UDP address structure
    socklen_t udp_addrlen = sizeof(udp_address);// Size of the address structure
    udp_address.sin_family = AF_INET;     // Set the address family to IPv4
    udp_address.sin_addr.s_addr = INADDR_ANY; // Bind to any available address
    udp_address.sin_port = htons(udp_port); // Use the UDP port number

    // Bind the socket to the address and port (UDP)
    if (bind(udp_listener, (struct sockaddr *)&udp_address, udp_addrlen) < 0)
    {
        perror("bind");
        close(tcp_listener); // Close the TCP socket
        close(udp_listener); // Close the UDP socket
        exit(EXIT_FAILURE);
    }

    // Set the socket to listen for incoming connections (TCP)
    if (listen(tcp_listener, SOMAXCONN) < 0) { // SOMAXCONN is the maximum queue length for pending connections
        perror("listen");
        close(tcp_listener); // Close the TCP socket
        close(udp_listener); // Close the UDP socket
        exit(EXIT_FAILURE);
    }

    // Allocate memory for the poll file descriptors
    int fds_capacity = 10;
    fds = malloc(fds_capacity * sizeof(struct pollfd));
    if (!fds) {
        perror("malloc");
        close(tcp_listener);
        close(udp_listener);
        exit(EXIT_FAILURE);
    }

    // Initialize the array to zero out all fields including revents
    memset(fds, 0, fds_capacity * sizeof(struct pollfd));

    int nfds = 2; // Number of valid file descriptors
    fds[0].fd = tcp_listener; // The first element is the listener socket
    fds[0].events = POLLIN; // Set the listener to poll for incoming connections
    fds[1].fd = udp_listener; // The second element is the UDP socket for incoming data
    fds[1].events = POLLIN; // Set the UDP listener to poll for incoming data

    printf("molecule_supplier server started on TCP port %d, UDP port %d (Use CTRL+C to shut down)\n", tcp_port, udp_port);

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
            int client_fd = accept(tcp_listener, NULL, NULL); // Accept a new client connection
            
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

        // Check if the UDP listener socket has incoming connections
        if (fds[1].revents & POLLIN)
        {
            handle_udp_client(udp_listener, &warehouse);
        }

        // Iterate through the file descriptors to handle client requests
        for (int i = 2; i < nfds; i++) {
            // Check if this fd has data to read
            if (fds[i].revents & POLLIN) {
                int connection_closed = handle_tcp_client(fds[i].fd, &warehouse); // Handle the client request
                
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
    printf("\nServer shut down successfully.\n");
    return 0;
}