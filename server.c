/*
    HTTP Web Server using Socket TCP-based
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

// FUNCTIONS
void setup_socket(int *sockfd, struct sockaddr_in *servaddr, int port);
char *get_file_path(char *uri);
void exit_server();
char *get_400_header(char *version);
char *get_403_header(char *version);
char *get_404_header(char *version);
char *get_405_header(char *version);
char *get_505_header(char *version);
void *handle_connection(void *arg);
int handle_header(char *request, char *method, char *uri, char *version, int connfd);
int parse_header(char *header, char *method, char *uri, char *version);
char *get_mime_type(char *uri);

// DEFINES
#define BUFFER_SIZE 4096
#define HEADER_SIZE 512
#define PATH_SIZE 512
#define METHOD_SIZE 8
#define VERSION_SIZE 12
#define LISTENQ 8

// GLOBAL VARIABLES
int connfd = -1;
int sockfd = -1;

int main(int argc, char *argv[]) {
    signal(SIGINT, exit_server);
    signal(SIGTERM, exit_server);
    signal(SIGABRT, exit_server);

    // variables
    struct sockaddr_in servaddr, connaddr;
    socklen_t addrlen = sizeof(servaddr);
    int port = atoi(argv[1]);
    pthread_t thread;


    if (argc != 2) {
        perror("Error: specify port to host server\n"); 
        exit(1);
    }

    setup_socket(&sockfd, &servaddr, port);

    printf("%s\n", "Server running...waiting for connections.");

    // MAIN LOOP
    for ( ; ; ) {
        // ACCEPT INCOMING CONNECTION
        if ( (connfd = accept(sockfd, (struct sockaddr *)&connaddr, (socklen_t*)&addrlen) ) < 0) {
            perror("Error: Accept failed\n");
            continue;
        }

        printf("Connection accepted: %d\n", connfd);

        int *new_sock = malloc(sizeof(int));
        *new_sock = connfd;
        if (pthread_create(&thread, NULL, handle_connection, new_sock) != 0) {
            perror("Error: pthread_create failed\n");
            close(connfd);
            free(new_sock);
            continue;
        }

        pthread_detach(thread);
    }

    exit_server();
    return 0;
}

void setup_socket(int *sockfd, struct sockaddr_in *servaddr, int port) {
    // Create socket
    if( (*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Could no create socket\n");
        exit(2);
    }
    puts("Socket created");

    // Socket structure
    servaddr->sin_family = AF_INET;
    servaddr->sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr->sin_port = htons(port);

    // Bind socket
    if (bind(*sockfd, (struct sockaddr *)servaddr, sizeof(*servaddr)) < 0) {
        perror("bind failed\n");
        exit(3);
    }

    // Put in listening mode
    if (listen(*sockfd, LISTENQ) < 0) {
        perror("listen failed\n");
        exit(4);
    }
}

void exit_server() {
    printf("%s\n", "Server shutting down...");

    if (connfd != -1) close(connfd);
    if (sockfd != -1) close(sockfd);

    exit(0);
}

char *get_400_header(char *version) {
    char *response = malloc(HEADER_SIZE);
    const char *body = "400 Bad Request";
    size_t body_len = strlen(body);
    snprintf(response, HEADER_SIZE,
        "%s 400 Bad Request\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s", version, body_len, body);
    return response;
}

char *get_403_header(char *version) {
    char *response = malloc(HEADER_SIZE);
    const char *body = "403 Forbidden";
    size_t body_len = strlen(body);
    snprintf(response, HEADER_SIZE,
        "%s 403 Forbidden\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s", version, body_len, body);
    return response;
}

char *get_404_header(char *version) {
    char *response = malloc(HEADER_SIZE);
    const char *body = "404 Not Found";
    size_t body_len = strlen(body);
    snprintf(response, HEADER_SIZE,
        "%s 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s", version, body_len, body);
    return response;
}

char *get_405_header(char *version) {
    char *response = malloc(HEADER_SIZE);
    const char *body = "405 Method Not Allowed: Only GET is supported";
    size_t body_len = strlen(body);
    snprintf(response, HEADER_SIZE,
        "%s 405 Method Not Allowed\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s", version, body_len, body);
    return response;
}

char *get_505_header(char *version) {
    char *response = malloc(HEADER_SIZE);
    const char *body = "505 HTTP Version Not Supported";
    size_t body_len = strlen(body);
    snprintf(response, HEADER_SIZE,
        "%s 505 HTTP Version Not Supported\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: %zu\r\n"
        "\r\n"
        "%s", version, body_len, body);
    return response;
}

char *get_file_path(char *uri) {
    char *path = malloc(PATH_SIZE);
    if (strcmp(uri, "/") == 0) {
        strcpy(path, "./www/index.html");
    } else {
        snprintf(path, PATH_SIZE, "./www%s", uri);
    }
    return path;
}

void *handle_connection(void *arg) {
    int connfd = *(int *)arg;
    free(arg);

    // set up timeout
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    for ( ; ; ) {
        char method[METHOD_SIZE], uri[PATH_SIZE], version[VERSION_SIZE];
        char request[BUFFER_SIZE];
        char *file_type;
        ssize_t valread;
        int total_bytes = 0;

        // RECEIVE REQUEST FROM CLIENT
        while( (valread = recv(connfd, request + total_bytes, HEADER_SIZE-1-total_bytes, 0) ) > 0) {
            total_bytes += valread;
            request[total_bytes] = '\0';

            if (strstr(request, "\r\n\r\n") != NULL) {
                break;
            }
        }

        if (valread <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                perror("Error: recv timeout\n");
                goto done;
            }
            perror("Error: recv failed\n");
            goto done;
        }
        // PARSE REQUEST HEADER
        if (handle_header(request, method, uri, version, connfd) != 0) {
            break;
        }

        // KEEP CONNECTION OPEN
        int keep_alive = 0;
        if (strcmp(version, "HTTP/1.1") == 0) {
            keep_alive = (strstr(request, "Connection: close") == NULL);
        } else {
            keep_alive = (strstr(request, "Connection: keep-alive") != NULL);
        }


        // FILE OPERATIONS
        char *path = get_file_path(uri);
        int fd = open(path, O_RDONLY);
        free(path);

        if (fd < 0) {
            printf("File not found\n");
            if (errno == EACCES) {
                char *resp = get_403_header(version);
                send(connfd, resp, strlen(resp), 0);
                free(resp);
            } 
            else {
                char *resp = get_404_header(version);
                send(connfd, resp, strlen(resp), 0);
                free(resp);
            }
            if (!keep_alive) break;
            continue;
        }
        
        struct stat st;
        fstat(fd, &st);

        file_type = get_mime_type(uri);

        // BUILD HEADER RESPONSE
        char header_response[HEADER_SIZE];
        snprintf(header_response, HEADER_SIZE, 
            "%s 200 OK\r\n"
            "Content-Type: %s\r\n"
            "Content-Length: %lld\r\n"
            "Connection: %s\r\n"
            "\r\n",
            version, file_type, st.st_size, keep_alive ? "Keep-alive" : "Close");
        send(connfd, header_response, strlen(header_response), 0);

        printf("Header response sent:\n'%s'\n", header_response);

        // READ FILE CONTENT
        char content[BUFFER_SIZE];
        int n;
        while ( (n = read(fd, content, BUFFER_SIZE)) > 0) {
            send(connfd, content, n, 0);
        }
        close(fd);

        if (!keep_alive) break;
    }
    done:
    close(connfd);
    return NULL;
}

int handle_header(char *request, char *method, char *uri, char *version, int connfd) {
    char header[HEADER_SIZE];

    printf("\nHTTP Request received: \n");
    int header_len = strcspn(request, "\r\n");
    strncpy(header, request, header_len);
    header[header_len] = '\0';

    puts(header);

    if (sscanf(header, "%s %s %s", method, uri, version) != 3) {
        printf("Invalid request header\n");
        char *response_400_header = get_400_header(version);
        send(connfd, response_400_header, strlen(response_400_header), 0);
        free(response_400_header);
        return -1;
    }

    parse_header(header, method, uri, version);

    if (strcmp(method, "GET") != 0) {
        printf("Method requested not supported\n");
        char *response_405_header = get_405_header(version);
        send(connfd, response_405_header, strlen(response_405_header), 0);
        free(response_405_header);
        return -1;
    }

    if (strcmp(version, "HTTP/1.1") != 0 && strcmp(version, "HTTP/1.0") != 0) {
        printf("HTTP Version not supported\n");
        char *response_505_header = get_505_header(version);
        send(connfd, response_505_header, strlen(response_505_header), 0);
        free(response_505_header);
        return -1;
    }

    return 0;
}

int parse_header(char *header, char *method, char *uri, char *version) {
    if ( (method = strtok(header, " ")) == NULL) {
        return -1;
    }
    if ( (uri = strtok(NULL, " ")) == NULL) {
        return -1;
    }
    if ( (version = strtok(NULL, " ")) == NULL) {
        return -1;
    }

    return 0;
}

char *get_mime_type(char *uri) {
    if (strcmp(uri, "/") == 0) { return "text/html"; }

    const char *extension = strrchr(uri, '.');

    if (extension == NULL) { return "text/plain"; }
    if (strcmp(extension, ".html") == 0) { return "text/html"; }
    if (strcmp(extension, ".txt") == 0) { return "text/plain"; }
    if (strcmp(extension, ".png") == 0) { return "image/png"; }
    if (strcmp(extension, ".gif") == 0) { return "image/gif"; }
    if (strcmp(extension, ".jpg") == 0) { return "image/jpg"; }
    if (strcmp(extension, ".ico") == 0) { return "image/x-icon"; }
    if (strcmp(extension, ".css") == 0) { return "text/css"; }
    if (strcmp(extension, ".js") == 0) { return "application/javascript"; }
    return "text/plain";
}