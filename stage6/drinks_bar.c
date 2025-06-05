#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <getopt.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <sys/file.h>
#include <fcntl.h> // fcntl, open, struct flock
#include <sys/mman.h> // mmap, munmap
#include <sys/stat.h> // ftruncate, S_IRUSR and such
#include <sys/types.h> // off_t and such

#define BUFFER_SIZE 1024 // Size of the buffer for reading client requests

// Global variables
extern int optopt;
extern char *optarg;
int tcp_listener = -1, udp_listener = -1;
int uds_stream_listener = -1, uds_dgram_fd = -1;
char *stream_path = NULL, *datagram_path = NULL; // Paths for UDS sockets
struct pollfd *fds = NULL; // Array of file descriptors for polling
int nfds = 3; // Number of valid file descriptors;
int running = 1;

typedef struct
{
    unsigned long long carbon;
    unsigned long long oxygen;
    unsigned long long hydrogen;
} AtomWarehouse;

AtomWarehouse *warehouse = NULL; // will point to mapped memory
int fd = -1; // file descriptor for the save file
char *save_file_path = NULL; // path to the shared file (if provided)

// Clean up: close all client sockets and free resources
void cleanup()
{
    for (int i = 0; i < nfds; i++)
    {
        if (fds[i].fd >= 0)
        {
            close(fds[i].fd); // Close each socket
        }
    }
    free(fds); // Free the allocated memory for file descriptors

    if (stream_path)
        unlink(stream_path); // Remove the UDS stream socket file
    
    if (datagram_path)
        unlink(datagram_path); // Remove the UDS datagram socket file

    if (save_file_path) {
    munmap(warehouse, sizeof(AtomWarehouse));
    close(fd);
    }
    
    else {
        free(warehouse);
    }
}

// Signal handler for graceful shutdown
void handle_signal(int sig)
{
    running = 0;
}

void handle_timeout(int sig)
{
    if (tcp_listener >= 0)
        close(tcp_listener);
    if (udp_listener >= 0)
        close(udp_listener);

    if (fds != NULL)
    {
        cleanup(fds, 3); // Clean up the file descriptors
    }

    printf("Server shutting down after timeout.\n");
    exit(0);
}

void print_status() 
{
    struct flock lock;
    
    if (fd >= 0) {
        lock.l_type = F_RDLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = sizeof(AtomWarehouse);
        fcntl(fd, F_SETLKW, &lock);
    }
    
    printf("Atom Warehouse Status:\n");
    printf("Carbon: %llu\n", warehouse->carbon);
    printf("Oxygen: %llu\n", warehouse->oxygen);
    printf("Hydrogen: %llu\n", warehouse->hydrogen);

    if (fd>=0) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLKW, &lock);
    }
}

int add_atoms(const char *atom, unsigned long long amount)
{
    if (fd >= 0) {
        struct flock lock = {
            .l_type = F_WRLCK,
            .l_whence = SEEK_SET,
            .l_start = 0,
            .l_len = sizeof(AtomWarehouse)
        };

        fcntl(fd, F_SETLKW, &lock); // Lock the file for writing

        if (strcmp(atom, "CARBON") == 0)
            warehouse->carbon += amount;
        else if (strcmp(atom, "OXYGEN") == 0)
            warehouse->oxygen += amount;
        else if (strcmp(atom, "HYDROGEN") == 0)
            warehouse->hydrogen += amount;
        else
            return 1; // Unknown atom type

        // Unlock the file after writing
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLKW, &lock);

        return 0;     // Successfully added atoms
    }

    // If file descriptor is not set, just update the warehouse in memory
    else {
        if (strcmp(atom, "CARBON") == 0)
            warehouse->carbon += amount;
        else if (strcmp(atom, "OXYGEN") == 0)
            warehouse->oxygen += amount;
        else if (strcmp(atom, "HYDROGEN") == 0)
            warehouse->hydrogen += amount;
        else
            return 1; // Unknown atom type
        return 0;
    }
}

int get_amount_of_molecules(const char *molecule)
{
    int res = 0; // Initialize the result to 0

    // Check the type of molecule and calculate the maximum number that can be created
    if (strcmp(molecule, "WATER") == 0)
    {
        while (warehouse->hydrogen >= 2 * (res + 1) && warehouse->oxygen >= (res + 1))
        {
            res++;
        }
    }

    else if (strcmp(molecule, "CARBON DIOXIDE") == 0)
    {
        while (warehouse->carbon >= (res + 1) && warehouse->oxygen >= 2 * (res + 1))
        {
            res++;
        }
    }

    else if (strcmp(molecule, "ALCOHOL") == 0)
    {
        while (warehouse->carbon >= 2 * (res + 1) && warehouse->hydrogen >= 6 * (res + 1) && warehouse->oxygen >= (res + 1))
        {
            res++;
        }
    }

    else if (strcmp(molecule, "GLUCOSE") == 0)
    {
        while (warehouse->carbon >= 6 * (res + 1) && warehouse->hydrogen >= 12 * (res + 1) && warehouse->oxygen >= 6 * (res + 1))
        {
            res++;
        }
    }

    else
        return -1; // Unknown molecule type

    return res; // Return the maximum number of molecules that can be created
}

int deliver_molecules(const char *molecule, unsigned long long amount)
{
    struct flock lock;
    if (fd >= 0) {
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = sizeof(AtomWarehouse);
        fcntl(fd, F_SETLKW, &lock); // Lock the file for writing
    }

    int potential_amount = get_amount_of_molecules(molecule); // Get the maximum number of molecules that can be created

    if (potential_amount == -1)
    {
        if (fd >= 0) {
            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLKW, &lock);
        }
        return 1; // Unknown molecule type
    }

    else if (potential_amount < amount)
    {
        if (fd >= 0) {
            lock.l_type = F_UNLCK;
            fcntl(fd, F_SETLKW, &lock);
        }
        return -1; // Not enough atoms to create the requested amount of molecules
    }

    if (strcmp(molecule, "WATER") == 0)
    {
        warehouse->hydrogen -= 2 * amount;
        warehouse->oxygen -= amount;
    }

    else if (strcmp(molecule, "CARBON DIOXIDE") == 0)
    {
        warehouse->carbon -= amount;
        warehouse->oxygen -= 2 * amount;
    }

    else if (strcmp(molecule, "ALCOHOL") == 0)
    {
        warehouse->carbon -= 2 * amount;
        warehouse->hydrogen -= 6 * amount;
        warehouse->oxygen -= amount;
    }

    else if (strcmp(molecule, "GLUCOSE") == 0)
    {
        warehouse->carbon -= 6 * amount;
        warehouse->hydrogen -= 12 * amount;
        warehouse->oxygen -= 6 * amount;
    }

    if (fd >= 0) {
        // Unlock the file after writing
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLKW, &lock);
    }

    return 0; // Successfully added molecules
}

void handle_udp_client(int fd)
{
    char buffer[BUFFER_SIZE] = {0}; // Buffer to hold the incoming data
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);

    // Read data from the client (leaving space for null terminator)
    int bytes = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addrlen);

    if (bytes <= 0)
    {
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
    if (parsed != 3 || strcmp(command, "DELIVER") != 0)
    {
        printf("UDP: Invalid command: %s\n", buffer);
        const char *msg = "ERROR: Invalid command\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        return;
    }

    // Trim trailing spaces from molecule name
    int len = strlen(molecule);
    while (len > 0 && molecule[len - 1] == ' ')
    {
        molecule[--len] = '\0';
    }

    // Attempt to deliver molecules
    int result = deliver_molecules(molecule, amount);

    if (result == 0)
    {
        const char *msg = "DELIVERED\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        printf("UDP: Delivered %llu %s molecules\n", amount, molecule);
    }

    else if (result == 1)
    {
        const char *msg = "ERROR: Unknown molecule type\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        printf("UDP: Unknown molecule type: %s\n", molecule);
    }

    else if (result == -1)
    {
        const char *msg = "NOT ENOUGH ATOMS\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        printf("UDP: Not enough atoms for %llu %s molecules\n", amount, molecule);
    }

    if(fd == -1) print_status(); // Print the current status of the warehouse
}

void handle_uds_datagram_client(int fd)
{
    char buffer[BUFFER_SIZE] = {0}; // Buffer to hold the incoming data
    struct sockaddr_un client_addr;
    socklen_t addrlen = sizeof(client_addr);

    // Read data from the client (leaving space for null terminator)
    int bytes = recvfrom(fd, buffer, BUFFER_SIZE - 1, 0, (struct sockaddr *)&client_addr, &addrlen);

    if (bytes <= 0)
    {
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
    if (parsed != 3 || strcmp(command, "DELIVER") != 0)
    {
        printf("UDS datagram: Invalid command: %s\n", buffer);
        const char *msg = "ERROR: Invalid command\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        return;
    }

    // Trim trailing spaces from molecule name
    int len = strlen(molecule);
    while (len > 0 && molecule[len - 1] == ' ')
    {
        molecule[--len] = '\0';
    }

    // Attempt to deliver molecules
    int result = deliver_molecules(molecule, amount);

    if (result == 0)
    {
        const char *msg = "DELIVERED\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        printf("UDS datagram: Delivered %llu %s molecules\n", amount, molecule);
    }

    else if (result == 1)
    {
        const char *msg = "ERROR: Unknown molecule type\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        printf("UDS datagram: Unknown molecule type: %s\n", molecule);
    }

    else if (result == -1)
    {
        const char *msg = "NOT ENOUGH ATOMS\n";
        sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&client_addr, addrlen);
        printf("UDS datagram: Not enough atoms for %llu %s molecules\n", amount, molecule);
    }

    print_status(); // Print the current status of the warehouse
}

int handle_tcp_or_uds_stream_client(int fd)
{
    char buffer[BUFFER_SIZE] = {0};                     // Buffer to hold the incoming data
    int bytes_read = read(fd, buffer, BUFFER_SIZE - 1); // Read data from the client (leaving space for null terminator)

    // In case of an error or no data read, close the connection
    if (bytes_read <= 0)
    {
        close(fd);
        return 1; // Connection closed
    }

    char command[16], atom[16];                                            // Buffers for command and atom type
    unsigned long long amount;                                             // Variable to hold the amount of atoms
    int parsed = sscanf(buffer, "%15s %15s %llu", command, atom, &amount); // Parse the command, atom type, and amount from the buffer

    // Check if the command is valid
    if (parsed != 3 || strcmp(command, "ADD") != 0)
    {
        printf("TCP / UDS stream: Invalid command: %s", buffer);
        return 0; // Connection still open
    }

    // Check if the amount is valid
    if (add_atoms(atom, amount))
    {
        printf("TCP / UDS stream: Unknown atom type: %s\n", atom);
        return 0; // Connection still open
    }

    if(fd == -1) print_status(); // Print the current status of the warehouse
    return 0; // Connection still open
}

int min3(int a, int b, int c)
{
    int min = a;
    if (b < min)
        min = b;
    if (c < min)
        min = c;
    return min;
}

int get_amount_to_gen(const char *drink)
{
    if (strcmp(drink, "SOFT DRINK") == 0)
    {
        return min3(get_amount_of_molecules("WATER"),
                    get_amount_of_molecules("CARBON DIOXIDE"),
                    get_amount_of_molecules("GLUCOSE"));
    }

    else if (strcmp(drink, "VODKA") == 0)
    {
        return min3(get_amount_of_molecules("WATER"),
                    get_amount_of_molecules("ALCOHOL"),
                    get_amount_of_molecules("GLUCOSE"));
    }

    else if (strcmp(drink, "CHAMPAGNE") == 0)
    {
        return min3(get_amount_of_molecules("WATER"),
                    get_amount_of_molecules("CARBON DIOXIDE"),
                    get_amount_of_molecules("ALCOHOL"));
    }

    else
        return -1; // Unknown drink type
}

void handle_stdin()
{
    char buffer[BUFFER_SIZE] = {0}; // Buffer to hold the incoming data

    if (fgets(buffer, sizeof(buffer), stdin) == NULL)
    {
        if (ferror(stdin))
            perror("fgets");
        return;
    }

    // Parse command for GEN
    char command[16], drink[16];

    // Extract just the command
    if (sscanf(buffer, "%15s", command) != 1 || strcmp(command, "GEN") != 0)
    {
        printf("Invalid command: %s\n", buffer);
        return;
    }

    // Find where the drink name starts (after "GEN ")
    char *drink_part = buffer + strlen("GEN");
    while (*drink_part == ' ' && *drink_part != '\0')
    {
        drink_part++;
    }

    // Copy the drink name (everything until newline)
    strcpy(drink, drink_part);

    // Remove trailing newline and spaces
    int len = strlen(drink);
    while (len > 0 && (drink[len - 1] == '\n' || drink[len - 1] == ' ' || drink[len - 1] == '\r'))
    {
        drink[--len] = '\0';
    }

    if (len == 0)
    {
        printf("Missing drink name\n");
        return;
    }

    int result;
    struct flock lock;
    
    // Lock if using shared file
    if (fd >= 0) {
        lock.l_type = F_RDLCK;
        lock.l_whence = SEEK_SET;
        lock.l_start = 0;
        lock.l_len = sizeof(AtomWarehouse);

        fcntl(fd, F_SETLKW, &lock); // Lock the file for reading
    }

    result = get_amount_to_gen(drink); // Attempt to generate molecules
    
    
    if(fd >= 0) {
        lock.l_type = F_UNLCK;
        fcntl(fd, F_SETLKW, &lock); // Unlock the file after reading
    }

    if (result == -1)
    {
        printf("Unknown drink type: %s\n", drink);
        return;
    }

    if (result == 0)
    {
        printf("Not enough atoms to generate any %s.\n", drink);
    }

    else
    {
        printf("You can generate %d %s.\n", result, drink);
    }
}

int main(int argc, char *argv[])
{
    int tcp_port = -1, udp_port = -1;
    int timeout = -1;
    unsigned long long oxygen = 0, carbon = 0, hydrogen = 0;
    bool seen_flags[256] = { false }; // Track seen flags to avoid duplicates

    struct option long_options[] = {
        {"tcp-port", required_argument, NULL, 'T'},
        {"udp-port", required_argument, NULL, 'U'},
        {"oxygen", required_argument, NULL, 'o'},
        {"carbon", required_argument, NULL, 'c'},
        {"hydrogen", required_argument, NULL, 'h'},
        {"timeout", required_argument, NULL, 't'},
        {"stream-path", required_argument, 0, 's'},
        {"datagram-path", required_argument, 0, 'd'},
        {"save-file", required_argument, NULL, 'f'},
        {0, 0, 0, 0}};
    
    while (1)
    {
        int ret = getopt_long(argc, argv, "T:U:o:c:h:t:s:d:f:", long_options, NULL);

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

        switch (ret)
        {
        case 'T':
            tcp_port = atoi(optarg);
            break;
        case 'U':
            udp_port = atoi(optarg);
            break;
        case 'o':
            oxygen = strtoull(optarg, NULL, 10);
            break;
        case 'c':
            carbon = strtoull(optarg, NULL, 10);
            break;
        case 'h':
            hydrogen = strtoull(optarg, NULL, 10);
            break;
        case 't':
            timeout = atoi(optarg);
            break;
        case 's':
            stream_path = strdup(optarg); // Duplicate the string so it can be used after optarg is modified

            // Append .socket if not already present
            if (!strstr(stream_path, ".socket"))
            {
                char *new_path = malloc(strlen(stream_path) + 8); // +8 for ".socket\0"
                if (new_path)
                {
                    sprintf(new_path, "%s.socket", stream_path);
                    free(stream_path);
                    stream_path = new_path;
                }
            }
            break;
        case 'd':
            datagram_path = strdup(optarg); // Duplicate the string so it can be used after optarg is modified

            // Append .socket if not already present
            if (!strstr(datagram_path, ".socket"))
            {
                char *new_path = malloc(strlen(datagram_path) + 8); // +8 for ".socket\0"
                if (new_path)
                {
                    sprintf(new_path, "%s.socket", datagram_path);
                    free(datagram_path);
                    datagram_path = new_path;
                }
            }
            break;
        case 'f':
            save_file_path = strdup(optarg);

            if (!strstr(save_file_path, ".dat"))
            {
                char *new_path = malloc(strlen(save_file_path) + 5); // +5 for ".dat\0"
                if (new_path)
                {
                    sprintf(new_path, "%s.dat", save_file_path);
                    free(save_file_path);
                    save_file_path = new_path;
                }
            }
            break;
        case '?':
            printf("Unknown option: %c\n", optopt);
            exit(EXIT_FAILURE);
        }
    }

    if ((tcp_port == -1 && stream_path == NULL) || (udp_port == -1 && datagram_path == NULL))
    {
        // If neither TCP nor UDP ports are specified, print an error message and exit
        printf("You must specify at least one of the following (from each line):\n");
        printf(" -T / --tcp-port <port> or -s / --stream-path <path>\n");
        printf(" -U / --udp-port <port> or -d / --datagram-path <path>\n");
        exit(EXIT_FAILURE);
    }

    else if (tcp_port != -1 && stream_path != NULL)
    {
        // If both TCP port and UDS stream path are specified, print an error message and exit
        printf("You cannot specify both a TCP port and a UDS stream path.\n");
        exit(EXIT_FAILURE);
    }

    else if (udp_port != -1 && datagram_path != NULL)
    {
        // If both UDP port and UDS datagram path are specified, print an error message and exit
        printf("You cannot specify both a UDP port and a UDS datagram path.\n");
        exit(EXIT_FAILURE);
    }

    // Validate that TCP and UDP ports are not the same
    if (tcp_port != -1 && udp_port != -1 && tcp_port == udp_port) {
        printf("Error: TCP and UDP cannot use the same port.\n");
        exit(EXIT_FAILURE);
    }

    // Validate that stream and datagram paths are not the same
    if (stream_path && datagram_path && strcmp(stream_path, datagram_path) == 0) {
        printf("Error: Stream and datagram sockets cannot use the same path.\n");
        exit(EXIT_FAILURE);
    }

    if (tcp_port != -1)
    {
        // Validate the port number
        if (tcp_port <= 0 || tcp_port > 65535)
        {
            fprintf(stderr, "Invalid port number: %d\n", tcp_port);
            exit(EXIT_FAILURE);
        }

        tcp_listener = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket

        // Check if the socket was created successfully
        if (tcp_listener < 0)
        {
            perror("socket");
            exit(EXIT_FAILURE);
        }

        // Set socket option to reuse address to avoid "address already in use"
        int opt = 1;
        if (setsockopt(tcp_listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        {
            perror("setsockopt");
            close(tcp_listener);
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in tcp_address = {0};        // Initialize the address structure
        socklen_t tcp_addrlen = sizeof(tcp_address); // Size of the address structure
        tcp_address.sin_family = AF_INET;            // Set the address family to IPv4
        tcp_address.sin_addr.s_addr = INADDR_ANY;    // Bind to any available address
        tcp_address.sin_port = htons(tcp_port);      // Convert the port number to network byte order

        // Bind the socket to the address and port (TCP)
        if (bind(tcp_listener, (struct sockaddr *)&tcp_address, tcp_addrlen) < 0)
        {
            perror("bind");
            close(tcp_listener); // Close the TCP socket
            exit(EXIT_FAILURE);
        }

        // Set the socket to listen for incoming connections (TCP)
        if (listen(tcp_listener, SOMAXCONN) < 0)
        { // SOMAXCONN is the maximum queue length for pending connections
            perror("listen");
            close(tcp_listener);
            exit(EXIT_FAILURE);
        }
    }

    if (udp_port != -1)
    {
        // Validate the port number
        if (udp_port <= 0 || udp_port > 65535)
        {
            fprintf(stderr, "Invalid port number: %d\n", udp_port);
            exit(EXIT_FAILURE);
        }

        udp_listener = socket(AF_INET, SOCK_DGRAM, 0); // Create a UDP socket

        // Check if the UDP socket was created successfully
        if (udp_listener < 0)
        {
            perror("socket");
            if (tcp_listener >= 0)
            {
                close(tcp_listener); // Close the TCP socket if UDP socket creation failed
            }
            close(udp_listener); // Close the UDP socket
            exit(EXIT_FAILURE);
        }

        // For UDP binding, create a new address structure
        struct sockaddr_in udp_address = {0};        // Initialize UDP address structure
        socklen_t udp_addrlen = sizeof(udp_address); // Size of the address structure
        udp_address.sin_family = AF_INET;            // Set the address family to IPv4
        udp_address.sin_addr.s_addr = INADDR_ANY;    // Bind to any available address
        udp_address.sin_port = htons(udp_port);      // Use the UDP port number

        // Bind the socket to the address and port (UDP)
        if (bind(udp_listener, (struct sockaddr *)&udp_address, udp_addrlen) < 0)
        {
            perror("bind");
            if (tcp_listener >= 0)
            {
                close(tcp_listener); // Close the TCP socket if UDP socket creation failed
            }
            close(udp_listener); // Close the UDP socket
            exit(EXIT_FAILURE);
        }
    }

    if (stream_path)
    {
        int stream_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (stream_fd < 0)
        {
            perror("socket (UDS stream)");
            // Clean up any previously created sockets
            if (tcp_listener >= 0)
                close(tcp_listener);
            if (udp_listener >= 0)
                close(udp_listener);
            exit(EXIT_FAILURE);
        }

        struct sockaddr_un stream_addr = {0};
        stream_addr.sun_family = AF_UNIX;

        // Check if path is too long
        if (strlen(stream_path) >= sizeof(stream_addr.sun_path))
        {
            fprintf(stderr, "UDS stream path too long: %s\n", stream_path);
            close(stream_fd);
            if (tcp_listener >= 0)
                close(tcp_listener);
            if (udp_listener >= 0)
                close(udp_listener);
            exit(EXIT_FAILURE);
        }

        strncpy(stream_addr.sun_path, stream_path, sizeof(stream_addr.sun_path) - 1);
        unlink(stream_path); // Remove existing socket file (ignore errors here)

        if (bind(stream_fd, (struct sockaddr *)&stream_addr, sizeof(stream_addr)) < 0)
        {
            perror("bind (UDS stream)");
            close(stream_fd);
            if (tcp_listener >= 0)
                close(tcp_listener);
            if (udp_listener >= 0)
                close(udp_listener);
            exit(EXIT_FAILURE);
        }

        if (listen(stream_fd, SOMAXCONN) < 0)
        {
            perror("listen (UDS stream)");
            close(stream_fd);
            unlink(stream_path); // Clean up the socket file
            if (tcp_listener >= 0)
                close(tcp_listener);
            if (udp_listener >= 0)
                close(udp_listener);
            exit(EXIT_FAILURE);
        }

        uds_stream_listener = stream_fd; // Store the socket descriptor
    }

    if (datagram_path)
    {
        int dgram_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
        if (dgram_fd < 0)
        {
            perror("socket (UDS datagram)");
            // Clean up any previously created sockets
            if (tcp_listener >= 0)
                close(tcp_listener);
            if (udp_listener >= 0)
                close(udp_listener);
            if (uds_stream_listener >= 0)
                close(uds_stream_listener);
            exit(EXIT_FAILURE);
        }

        struct sockaddr_un dgram_addr = {0};
        dgram_addr.sun_family = AF_UNIX;

        // Check if path is too long
        if (strlen(datagram_path) >= sizeof(dgram_addr.sun_path))
        {
            fprintf(stderr, "UDS datagram path too long: %s\n", datagram_path);
            close(dgram_fd);
            if (tcp_listener >= 0)
                close(tcp_listener);
            if (udp_listener >= 0)
                close(udp_listener);
            if (uds_stream_listener >= 0)
                close(uds_stream_listener);
            exit(EXIT_FAILURE);
        }

        strncpy(dgram_addr.sun_path, datagram_path, sizeof(dgram_addr.sun_path) - 1); // Ensure null termination
        unlink(datagram_path);                                                        // Remove the socket file if it already exists (must be done before binding)

        if (bind(dgram_fd, (struct sockaddr *)&dgram_addr, sizeof(dgram_addr)) < 0)
        { // Bind the socket to the address
            perror("bind (UDS datagram)");
            close(dgram_fd);
            if (tcp_listener >= 0)
                close(tcp_listener);
            if (udp_listener >= 0)
                close(udp_listener);
            if (uds_stream_listener >= 0)
                close(uds_stream_listener);
            unlink(datagram_path); // Clean up the socket file
            exit(EXIT_FAILURE);
        }

        uds_dgram_fd = dgram_fd; // Store the socket descriptor
    }

    if (save_file_path) {
        fd = open(save_file_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
        if (fd == -1) {
            perror("open");
            exit(EXIT_FAILURE);
        }

        AtomWarehouse zero = {0};
        off_t size = lseek(fd, 0, SEEK_END);  // Move to end to check size
        if (size < sizeof(AtomWarehouse)) {
            // File is empty or too small – initialize once
            lseek(fd, 0, SEEK_SET);
            write(fd, &zero, sizeof(AtomWarehouse));
        }
        lseek(fd, 0, SEEK_SET);  // Always reset to start before mmap

        warehouse = mmap(NULL, sizeof(AtomWarehouse),
                        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (warehouse == MAP_FAILED) {
            perror("mmap");
            close(fd);
            exit(EXIT_FAILURE);
        }
    } 

    else {
        warehouse = malloc(sizeof(AtomWarehouse));
        warehouse->carbon = carbon;
        warehouse->oxygen = oxygen;
        warehouse->hydrogen = hydrogen;
    }

    // Set up signal handlers for graceful shutdown
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // Allocate memory for the poll file descriptors
    int fds_capacity = 10;
    fds = malloc(fds_capacity * sizeof(struct pollfd));
    if (!fds)
    {
        perror("malloc");

        if (tcp_listener >= 0)
            close(tcp_listener); // Close the TCP listener if it was created
        else if (stream_path)
            unlink(stream_path); // Remove the UDS stream socket file if it was created

        if (udp_listener >= 0)
            close(udp_listener); // Close the UDP listener if it was created
        else if (datagram_path)
            unlink(datagram_path); // Remove the UDS datagram socket file if it was created

        exit(EXIT_FAILURE);
    }

    // Initialize the array to zero out all fields including revents
    memset(fds, 0, fds_capacity * sizeof(struct pollfd));

    if (tcp_listener >= 0)
    {
        fds[0].fd = tcp_listener; // The first element is the listener socket
        fds[0].events = POLLIN;   // Set the listener to poll for incoming connections
    }

    else if (uds_stream_listener >= 0)
    {
        fds[0].fd = uds_stream_listener; // The fourth element is the UDS stream socket for incoming data
        fds[0].events = POLLIN;          // Set the UDS stream listener to poll for incoming data
    }

    if (udp_listener >= 0)
    {
        fds[1].fd = udp_listener; // The second element is the UDP socket for incoming data
        fds[1].events = POLLIN;   // Set the UDP listener to poll for incoming data
    }

    else if (uds_dgram_fd >= 0)
    {
        fds[1].fd = uds_dgram_fd; // The fifth element is the UDS datagram socket for incoming data
        fds[1].events = POLLIN;   // Set the UDS datagram listener to poll for incoming data
    }

    fds[2].fd = STDIN_FILENO; // The third element is the standard input for commands
    fds[2].events = POLLIN;   // Set the stdin to poll for incoming data

    printf("drinks_bar server started (Use CTRL+C to shut down):\n");
    if (tcp_listener >= 0)
    {
        printf("TCP server started on port: %d\n", tcp_port);
    }

    else if (stream_path)
    {
        printf("UDS stream server started on path: %s\n", stream_path);
    }

    if (udp_listener >= 0)
    {
        printf("UDP server started on port: %d\n", udp_port);
    }

    else if (datagram_path)
    {
        printf("UDS datagram server started on path: %s\n", datagram_path);
    }
    print_status(); // Print the initial status of the warehouse

    if (timeout > 0)
    {
        signal(SIGALRM, handle_timeout);
        alarm(timeout);
    }

    // Main loop to accept and handle client connections
    while (running)
    {
        int ready = poll(fds, nfds, 1000); // Poll with a timeout so we can check running flag

        if (!running)
            break; // Check if we need to exit

        // Check if poll was successful
        if (ready < 0)
        {
            perror("poll");
            continue;
        }

        static AtomWarehouse prev_snapshot = {0};
        static int first_time = 1;

        if (first_time) {
            prev_snapshot = *warehouse;
            first_time = 0;
        }
        
        else if (memcmp(&prev_snapshot, warehouse, sizeof(AtomWarehouse)) != 0) {
            printf("[Update detected] Warehouse changed\n");
            print_status();
            prev_snapshot = *warehouse;
        }

        // Check if the TCP or UDS stream listener socket has incoming connections
        if (fds[0].revents & POLLIN)
        {
            if (timeout > 0)
                alarm(timeout);                            // Reset the alarm for timeout
            int client_fd = accept(fds[0].fd, NULL, NULL); // Accept a new client connection

            // Check if the accept was successful
            if (client_fd < 0)
            {
                perror("accept");
                continue;
            }

            // Resize the array if we're out of space
            if (nfds >= fds_capacity)
            {
                fds_capacity *= 2; // Double the capacity
                struct pollfd *new_fds = realloc(fds, fds_capacity * sizeof(struct pollfd));
                if (!new_fds)
                {
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
        if (fds[1].revents & POLLIN && udp_listener >= 0)
        {
            if (timeout > 0)
                alarm(timeout); // Reset the alarm for timeout
            handle_udp_client(udp_listener);
        }

        // Check if the UDS stream listener socket has incoming connections
        else if (fds[1].revents & POLLIN && uds_dgram_fd >= 0)
        {
            if (timeout > 0)
                alarm(timeout); // Reset the alarm for timeout
            handle_uds_datagram_client(uds_dgram_fd);
        }

        // Check if the stdin has data to read
        if (fds[2].revents & POLLIN)
        {
            if (timeout > 0)
                alarm(timeout); // Reset the alarm for timeout
            handle_stdin();
        }

        // Iterate through the file descriptors to handle client requests
        for (int i = 3; i < nfds; i++)
        {
            // Check if this fd has data to read
            if (fds[i].revents & POLLIN)
            {
                alarm(timeout);
                int connection_closed = handle_tcp_or_uds_stream_client(fds[i].fd); // Handle the client request

                if (connection_closed)
                {
                    // Shift remaining elements to fill the gap
                    for (int j = i; j < nfds - 1; j++)
                    {
                        fds[j] = fds[j + 1];
                    }
                    nfds--;
                    i--; // Adjust index since we removed an element
                }
            }
        }
    }

    cleanup(); // Clean up: close all client sockets and free resources
    printf("\nServer shut down successfully.\n");
    return 0;
}