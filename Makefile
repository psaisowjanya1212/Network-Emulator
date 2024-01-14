CC = gcc
CFLAGS = -ggdb3

all: bridge station 
	@echo "bridge station compiled"

bridge: bridge.c ether.h ip.h utils.h
	$(CC) $(CFLAGS) bridge.c -o bridge

station: station.c ether.h ip.h utils.h
	$(CC) $(CFLAGS) station.c -o station

.PHONY: clean

clean:
	rm -rf bridge station *.o .*.addr .*.port 



