CC = gcc

CFLAGS = -pthread

server: server.o
	$(CC) $(CFLAGS) server.o -o server

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

run-server: server
	./server 3000

client: client.o
	$(CC) client.o -o client

client.o: client.c
	$(CC) -c client.c

clean:
	rm -f *.o server
