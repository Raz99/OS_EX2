CC = gcc
CFLAGS = -g -Wall
TARGET_SERVER = atom_warehouse
TARGET_CLIENT = atom_supplier

.PHONY: all clean

all: $(TARGET_SERVER) $(TARGET_CLIENT)

$(TARGET_SERVER): atom_warehouse.o
	$(CC) $(CFLAGS) -o $(TARGET_SERVER) $<

$(TARGET_CLIENT): atom_supplier.o
	$(CC) $(CFLAGS) -o $(TARGET_CLIENT) $<

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(TARGET_SERVER) $(TARGET_CLIENT) *.o