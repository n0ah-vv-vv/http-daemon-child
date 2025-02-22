#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#define PORT 8080
#define SERVER_IP "127.0.0.1"
#define BUFFER_SIZE 4096

int create_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        return -1;
    }
    return sockfd;
}

int connect_server(int sockfd) {
    struct sockaddr_in serv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT)
    };

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("invalid address");
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        return -1;
    }
    return 0;
}

int send_request(int sockfd, const char* request) {
    ssize_t bytes_sent = send(sockfd, request, strlen(request), 0);
    if (bytes_sent < 0) {
        perror("send failed");
        return -1;
    }
    return 0;
}

char* receive_response(int sockfd) {
    char* buffer = malloc(BUFFER_SIZE);
    if (!buffer) {
        perror("malloc failed");
        return NULL;
    }

    ssize_t bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received < 0) {
        perror("recv failed");
        free(buffer);
        return NULL;
    }

    buffer[bytes_received] = '\0';
    return buffer;
}

int validate_response(const char* response, const char* expected_status) {
    char status_line[128];
    snprintf(status_line, sizeof(status_line), "HTTP/1.0 %s", expected_status);

    if (strstr(response, status_line) != NULL) {
        return 0;  // Status match found
    }

    fprintf(stderr, "Expected status: %s\nReceived response:\n%s\n",
            status_line, response);
    return -1;
}

int test_valid_request() {
    printf("Testing valid request...\n");

    int sockfd = create_socket();
    if (sockfd < 0) return -1;

    if (connect_server(sockfd) < 0) {
        close(sockfd);
        return -1;
    }

    const char* request = "GET /index.html HTTP/1.0\r\n\r\n";
    if (send_request(sockfd, request) < 0) {
        close(sockfd);
        return -1;
    }

    char* response = receive_response(sockfd);
    close(sockfd);

    if (!response) return -1;
    int result = validate_response(response, "200 OK");
    free(response);

    return result;
}

int test_invalid_request() {
    printf("Testing invalid request...\n");

    int sockfd = create_socket();
    if (sockfd < 0) return -1;

    if (connect_server(sockfd) < 0) {
        close(sockfd);
        return -1;
    }

    const char* request = "GET /nonexistent.file HTTP/1.0\r\n\r\n";
    if (send_request(sockfd, request) < 0) {
        close(sockfd);
        return -1;
    }

    char* response = receive_response(sockfd);
    close(sockfd);

    if (!response) return -1;
    int result = validate_response(response, "404 Not Found");
    free(response);

    return result;
}

int main() {
    int valid_test = test_valid_request();
    int invalid_test = test_invalid_request();

    printf("\nTest Results:\n");
    printf("Valid request test: %s\n", valid_test ? "FAIL" : "PASS");
    printf("Invalid request test: %s\n", invalid_test ? "FAIL" : "PASS");

    return (valid_test || invalid_test) ? EXIT_FAILURE : EXIT_SUCCESS;
}
