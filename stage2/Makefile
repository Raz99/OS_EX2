CC = gcc
CFLAGS = -g -Wall
TARGET_SERVER = molecule_supplier
TARGET_TCP_CLIENT = atom_supplier
TARGET_UDP_CLIENT = molecule_requester

.PHONY: all clean

all: $(TARGET_SERVER) $(TARGET_TCP_CLIENT) $(TARGET_UDP_CLIENT)

$(TARGET_SERVER): molecule_supplier.o
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $<

$(TARGET_TCP_CLIENT): atom_supplier.o
	$(CC) $(CFLAGS) -o $(TARGET_TCP_CLIENT) $<

$(TARGET_UDP_CLIENT): molecule_requester.o
	$(CC) $(CFLAGS) -o $(TARGET_UDP_CLIENT) $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

# Valgrind targets
valgrind: valgrind-server valgrind-tcp-client valgrind-udp-client

# Individual Valgrind targets
valgrind-server: $(TARGET_SERVER)
	@echo "Running Valgrind on server - press Ctrl+C to stop after testing"
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(TARGET_SERVER) 5555

valgrind-tcp-client: $(TARGET_TCP_CLIENT)
	@echo "Running Valgrind on TCP client - make sure server is already running"
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(TARGET_TCP_CLIENT) localhost 5555

valgrind-udp-client: $(TARGET_UDP_CLIENT)
	@echo "Running Valgrind on UDP client - make sure server is already running"
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose ./$(TARGET_UDP_CLIENT) localhost 5555

clean:
	rm -f $(TARGET_SERVER) $(TARGET_TCP_CLIENT) $(TARGET_UDP_CLIENT) *.o