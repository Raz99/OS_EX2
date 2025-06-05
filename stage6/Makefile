CC = gcc
CFLAGS = -g -Wall -fprofile-arcs -ftest-coverage
TARGET_SERVER = drinks_bar
TARGET_TCP_CLIENT = atom_supplier
TARGET_UDP_CLIENT = molecule_requester

.PHONY: all clean

all: $(TARGET_SERVER) $(TARGET_TCP_CLIENT) $(TARGET_UDP_CLIENT)

$(TARGET_SERVER): drinks_bar.o
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $<

$(TARGET_TCP_CLIENT): atom_supplier.o
	$(CC) $(CFLAGS) -o $(TARGET_TCP_CLIENT) $<

$(TARGET_UDP_CLIENT): molecule_requester.o
	$(CC) $(CFLAGS) -o $(TARGET_UDP_CLIENT) $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET_SERVER) $(TARGET_TCP_CLIENT) $(TARGET_UDP_CLIENT) *.o *.socket *.dat *.gcda *.gcno *.gcov