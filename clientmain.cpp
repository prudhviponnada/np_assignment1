#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>

#define BUFFER_SIZE 2000
#define DEBUG

#include <calcLib.h>

// Function Prototypes
void receiveMessage(int *socket_fd, char *response_buffer, unsigned int buffer_size);
void sendMessage(int *socket_fd, const char *message);
void computeAndSendResult(const char *server_instruction, int *socket_fd);
void parseHostPort(const char *host_port_arg, char **server_hostname, int *server_port);
void testComputeAndSendResult(); // Unit test prototype

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <host:port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Parse host and port
    char *server_hostname = NULL;
    int server_port;
    parseHostPort(argv[1], &server_hostname, &server_port);

    printf("Hostname: %s, Port: %d.\n", server_hostname, server_port);

    // Prepare socket and server connection
    struct addrinfo hints = {0}, *server_info;
    hints.ai_family = AF_UNSPEC; // Support IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;

    char port_str[6];
    snprintf(port_str, sizeof(port_str), "%d", server_port);

    if (getaddrinfo(server_hostname, port_str, &hints, &server_info) != 0) {
        perror("getaddrinfo");
        free(server_hostname);
        return EXIT_FAILURE;
    }

    int socket_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (socket_fd < 0) {
        perror("Socket creation failed");
        freeaddrinfo(server_info);
        free(server_hostname);
        return EXIT_FAILURE;
    }

    if (connect(socket_fd, server_info->ai_addr, server_info->ai_addrlen) < 0) {
        perror("Connection to server failed");
        close(socket_fd);
        freeaddrinfo(server_info);
        free(server_hostname);
        return EXIT_FAILURE;
    }

    freeaddrinfo(server_info);
    free(server_hostname);

    #ifdef DEBUG
    printf("Connected to server.\n");
    #endif

    char response_buffer[BUFFER_SIZE];
    receiveMessage(&socket_fd, response_buffer, sizeof(response_buffer));

    if (strstr(response_buffer, "TEXT TCP 1.0")) {
        sendMessage(&socket_fd, "OK\n");
        receiveMessage(&socket_fd, response_buffer, sizeof(response_buffer));
        computeAndSendResult(response_buffer, &socket_fd);
        receiveMessage(&socket_fd, response_buffer, sizeof(response_buffer));
        printf("%s", response_buffer);
        printf("Test OK\n");
    } else {
        fprintf(stderr, "Unexpected protocol or data received. Test ERROR\n");
    }

    close(socket_fd);
    return EXIT_SUCCESS;
}

// Parse <host:port> argument into hostname and port
void parseHostPort(const char *host_port_arg, char **server_hostname, int *server_port) {
    char *host_port_copy = strdup(host_port_arg);
    char *colon_position = strrchr(host_port_copy, ':');
    if (!colon_position) {
        fprintf(stderr, "Invalid format. Use <host:port>\n");
        free(host_port_copy);
        exit(EXIT_FAILURE);
    }

    *colon_position = '\0';
    *server_hostname = strdup(host_port_copy);
    *server_port = atoi(colon_position + 1);
    free(host_port_copy);
}

// Receive a message from the server
void receiveMessage(int *socket_fd, char *response_buffer, unsigned int buffer_size) {
    memset(response_buffer, 0, buffer_size);
    int bytes_received = recv(*socket_fd, response_buffer, buffer_size, 0);

    if (bytes_received < 0) {
        perror("Receive error");
        close(*socket_fd);
        exit(EXIT_FAILURE);
    } else if (bytes_received == 0) {
        fprintf(stderr, "Server closed the connection\n");
        close(*socket_fd);
        exit(EXIT_SUCCESS);
    }

    #ifdef DEBUG
    printf("Received: %s", response_buffer);
    #endif

    if (bytes_received > 100) {
        fprintf(stderr, "Received excessive data, closing connection.\n");
        close(*socket_fd);
        exit(EXIT_FAILURE);
    }
}

// Send a message to the server
void sendMessage(int *socket_fd, const char *message) {
    if (send(*socket_fd, message, strlen(message), 0) < 0) {
        perror("Send error");
        close(*socket_fd);
        exit(EXIT_FAILURE);
    }

    #ifdef DEBUG
    printf("Sent: %s", message);
    #endif
}

// Compute the result and send it to the server
void computeAndSendResult(const char *server_instruction, int *socket_fd) {
    int operand1, operand2, integer_result = 0;
    double float_operand1, float_operand2, float_result = 0.0;
    char operation[10];

    sscanf(server_instruction, "%s %lf %lf", operation, &float_operand1, &float_operand2);

    if (strcmp(operation, "fadd") == 0) {
        float_result = float_operand1 + float_operand2;
    } else if (strcmp(operation, "fsub") == 0) {
        float_result = float_operand1 - float_operand2;
    } else if (strcmp(operation, "fmul") == 0) {
        float_result = float_operand1 * float_operand2;
    } else if (strcmp(operation, "fdiv") == 0) {
        float_result = float_operand1 / float_operand2;
    } else {
        operand1 = (int)float_operand1;
        operand2 = (int)float_operand2;

        if (strcmp(operation, "add") == 0) {
            integer_result = operand1 + operand2;
        } else if (strcmp(operation, "sub") == 0) {
            integer_result = operand1 - operand2;
        } else if (strcmp(operation, "mul") == 0) {
            integer_result = operand1 * operand2;
        } else if (strcmp(operation, "div") == 0) {
            integer_result = operand1 / operand2;
        }
    }

    char response[50];
    if (operation[0] == 'f') {
        snprintf(response, sizeof(response), "%8.8g\n", float_result);
    } else {
        snprintf(response, sizeof(response), "%d\n", integer_result);
    }

    sendMessage(socket_fd, response);
}


void testComputeAndSendResult() {
   
    const char *test_input1 = "fadd 2.5 3.5";
    int fake_socket_fd = -1; // Not used in unit test
    computeAndSendResult(test_input1, &fake_socket_fd);
    // printf("Test 1 passed.\n");

   
    const char *test_input2 = "mul 7 3";
    computeAndSendResult(test_input2, &fake_socket_fd);
    // printf("Test 2 passed.\n");

   
}
