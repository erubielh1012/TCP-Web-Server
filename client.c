#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) 
{
    int sockfd;
    struct sockaddr_in servaddr;
    char sendline[BUFFER_SIZE], recvline[BUFFER_SIZE];
    int n;
	
    if (argc !=3) {
        perror("Error: specify port & IP address"); 
        exit(1);
    }

    int port = atoi(argv[1]);

    //Create a socket for the client
    if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Problem in creating the socket");
        exit(2);
    }
	
    //Creation of the socket
    // memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr= inet_addr(argv[2]);
    servaddr.sin_port =  htons(port); //convert to big-endian order
	
     //Connection of the client to the socket 
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        perror("Problem in connecting to the server");
        exit(3);
    }

    printf("Connected to server...ready to send\n");
	
    while (1) {
	    printf("Send message: ");
        if (!fgets(sendline, BUFFER_SIZE, stdin)) break;
        sendline[strcspn(sendline, "\n")] = 0;

        char header[] = "/ HTTP/1.1\r\nHost: localhost:8080\r\nConnection: keep-alive\r\n\r\n";
        char request[BUFFER_SIZE];
        snprintf(request, BUFFER_SIZE, "%s%s", header, sendline);

        printf("sending: '%s' (len=%zu)\n", request, strlen(request));
        if (send(sockfd, request, strlen(request), 0) < 0) {
            perror("Send failed");
            exit(EXIT_FAILURE);
        }
		
        if ((n = recv(sockfd, recvline, BUFFER_SIZE-1, 0)) < 0){
            //error: server terminated prematurely
            perror("Recv failed"); 
            exit(EXIT_FAILURE);
        }

        if ( n == 0 ) { printf("server closed connection\n"); break; }

        recvline[n] = '\0';
        printf("Server reply: '%s'\n", recvline);
    }

    close(sockfd);
    exit(0);
}