
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>

#define MAX_CLIENTS 10 // Maximum number of concurrent clients
#define BUFFER_SIZE 1024 // Size of the buffer for reading client requests

typedef struct
{
    unsigned long long carbon;
    unsigned long long oxygen;
    unsigned long long hydrogen;
} AtomWarehouse;

void print_status(AtomWarehouse *w)
{
    printf("Atom Warehouse Status:\n");
    printf("Carbon: %llu\n", w->carbon);
    printf("Oxygen: %llu\n", w->oxygen);
    printf("Hydrogen: %llu\n", w->hydrogen);
}

int add_atoms(AtomWarehouse *w, const char *atom, unsigned long long amount)
{
    if (strcmp(atom, "CARBON") == 0)
        w->carbon += amount;
    else if (strcmp(atom, "OXYGEN") == 0)
        w->oxygen += amount;
    else if (strcmp(atom, "HYDROGEN") == 0)
        w->hydrogen += amount;
    else return 1; // Unknown atom type
    return 0; // Successfully added atoms
}

int deliver_molecules(AtomWarehouse *w, const char *molecule, unsigned long long amount)
{
    if (strcmp(molecule, "WATER") == 0)
    {
        if (w->hydrogen < 2 * amount && w->oxygen < amount)
            return 0; // Not enough atoms to create water

        w->hydrogen -= 2 * amount;
        w->oxygen -= amount;
    }

    else if (strcmp(molecule, "CARBON DIOXIDE") == 0)
    {
        if (w->carbon < amount || w->oxygen < 2 * amount)
            return 0; // Not enough atoms to create carbon dioxide

        w->carbon -= amount;
        w->oxygen -= 2 * amount;
    }

    else if (strcmp(molecule, "ALCOHOL") == 0)
    {
        if (w->carbon < 2 * amount || w->hydrogen < 6 * amount || w->oxygen < amount)
            return 0; // Not enough atoms to create alcohol

        w->carbon -= 2 * amount;
        w->hydrogen -= 6 * amount;
        w->oxygen -= amount;
    }

    else if (strcmp(molecule, "GLUCOSE") == 0)
    {
        if (w->carbon < 6 * amount || w->hydrogen < 12 * amount || w->oxygen < 6 * amount)
            return 0; // Not enough atoms to create glucose

        w->carbon -= 6 * amount;
        w->hydrogen -= 12 * amount;
        w->oxygen -= 6 * amount;
    }

    else return 1; // Unknown molecule type
    return 0; // Successfully added molecules
}

void handle_udp_client(int fd, AtomWarehouse *warehouse) {
    char buffer[BUFFER_SIZE] = {0};
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    int bytes = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addrlen);

    if (bytes <= 0) {
        perror("recvfrom");
        return;
    }

    buffer[bytes] = '\0'; // Ensure null-termination of the received string

    // Parse command for DELIVER
    char command[16], molecule[32];
    unsigned long long amount;
    int parsed = sscanf(buffer, "%15s %30s %llu", command, molecule, &amount);

    if (parsed != 3 || strcmp(command, "DELIVER") != 0) {
        printf("Invalid UDP command: %s\n", buffer);
        const char *msg = "ERROR: Invalid command\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        return;
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
    
    else {
        const char *msg = "NOT ENOUGH ATOMS\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        printf("UDP: Not enough atoms for %llu %s molecules\n", amount, molecule);
    }

    print_status(warehouse); // Print the current status of the warehouse
}


void handle_tcp_client(int fd, AtomWarehouse *warehouse)
{
    char buffer[BUFFER_SIZE] = {0}; // Buffer to hold the incoming data
    int bytes_read = read(fd, buffer, BUFFER_SIZE - 1); // Read data from the client (leaving space for null terminator)

    // In case of an error or no data read, close the connection
    if (bytes_read <= 0)
    {
        close(fd);
        return;
    }

    char command[16], atom[16]; // Buffers for command and atom type
    unsigned long long amount; // Variable to hold the amount of atoms
    int parsed = sscanf(buffer, "%15s %15s %llu", command, atom, &amount); // Parse the command, atom type, and amount from the buffer

    // Check if the command is valid
    if (parsed != 3 || strcmp(command, "ADD") != 0)
    {
        printf("Invalid TCP command: %s", buffer);
        return;
    }

    // Check if the amount is valid
    if (add_atoms(warehouse, atom, amount))
    {
        printf("TCP: Unknown atom type: %s\n", atom);
        return;
    }

    print_status(warehouse); // Print the current status of the warehouse
}

int main(int argc, char *argv[])
{
    // Check if the correct number of arguments is provided
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]); // Convert the port argument to an integer

    // Validate the port number
    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "Invalid port number: %s\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    AtomWarehouse warehouse = {0, 0, 0}; // Initialize the warehouse with zero atoms

    int tcp_listener = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket

    // Check if the socket was created successfully
    if (tcp_listener < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    int udp_listener = socket(AF_INET, SOCK_DGRAM, 0); // Create a UDP socket

    // Check if the UDP socket was created successfully
    if (udp_listener < 0)
    {
        perror("socket");
        close(tcp_listener); // Close the TCP socket if UDP socket creation failed
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in address = {0}; // Initialize the address structure
    socklen_t addrlen = sizeof(address);// Size of the address structure
    address.sin_family = AF_INET; // Set the address family to IPv4
    address.sin_addr.s_addr = INADDR_ANY; // Bind to any available address
    address.sin_port = htons(port); // Convert the port number to network byte order

    // Bind the socket to the address and port (TCP)
    if (bind(tcp_listener, (struct sockaddr *)&address, addrlen) < 0)
    {
        perror("bind");
        close(tcp_listener); // Close the TCP socket
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the address and port (UDP)
    if (bind(udp_listener, (struct sockaddr *)&address, addrlen) < 0)
    {
        perror("bind");
        close(tcp_listener); // Close the TCP socket
        close(udp_listener); // Close the UDP socket
        exit(EXIT_FAILURE);
    }

    // Set the socket to listen for incoming connections (TCP)
    if (listen(tcp_listener, MAX_CLIENTS) < 0)
    {
        perror("listen");
        close(tcp_listener); // Close the TCP socket
        close(udp_listener); // Close the UDP socket
        exit(EXIT_FAILURE);
    }
    
    struct pollfd fds[MAX_CLIENTS + 2]; // Array of poll file descriptors
    fds[0].fd = tcp_listener; // The first element is the listener socket for TCP connections
    fds[0].events = POLLIN; // Set the TCP listener to poll for incoming connections
    fds[1].fd = udp_listener; // The second element is the UDP socket for incoming data
    fds[1].events = POLLIN; // Set the UDP listener to poll for incoming data

    for (int i = 2; i <= MAX_CLIENTS; i++)
    {
        fds[i].fd = -1; // Initialize the rest of the fds to -1 (no client connected)
    }

    printf("molecule_supplier server started on port %d\n", port); // Print server start message

    // Main loop to accept and handle client connections
    while (1)
    {
        int ready = poll(fds, MAX_CLIENTS + 1, -1); // Wait indefinitely for events on the file descriptors

        // Check if poll was successful
        if (ready < 0)
        {
            perror("poll");
            continue;
        }

        // Check if the TCP listener socket has incoming connections
        if (fds[0].revents & POLLIN)
        {
            int client_fd = accept(tcp_listener, NULL, NULL); // Accept a new client connection

            // Check if the accept was successful
            if (client_fd < 0)
            {
                perror("accept");
                continue;
            }

            // Find an empty slot in the fds array for the new client
            for (int i = 2; i <= MAX_CLIENTS; i++)
            {
                if (fds[i].fd == -1)
                {
                    fds[i].fd = client_fd;  // Assign the new client file descriptor
                    fds[i].events = POLLIN; // Set the new client to poll for incoming data
                    break;
                }
            }
        }

        // Check if the UDP listener socket has incoming connections
        if (fds[1].revents & POLLIN)
        {
            handle_udp_client(udp_listener, &warehouse);
        }

        // Iterate through the file descriptors to handle client requests
        for (int i = 2; i <= MAX_CLIENTS; i++)
        {
            // Check if the file descriptor is valid and has data to read
            if (fds[i].fd != -1 && (fds[i].revents & POLLIN))
            {
                handle_tcp_client(fds[i].fd, &warehouse); // Handle the TCP client request
                fds[i].fd = -1; // Reset the file descriptor to -1 after handling the request
            }
        }
    }

    // Close the listener sockets (these lines will never be reached in this infinite loop)
    close(tcp_listener); // Close the TCP listener socket
    close(udp_listener); // Close the UDP listener socket
    return 0;
}