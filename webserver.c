#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 8192

typedef struct {
    int port;
    char document_root[256];
} Config;

// Function to load configuration from a file
int load_config(const char *filename, Config *config) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening config file");
        return -1;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char key[128];
        char value[128];
        if (sscanf(line, "%127[^=]=%127s", key, value) == 2) {
            if (strcmp(key, "port") == 0) {
                config->port = atoi(value);
            } else if (strcmp(key, "document_root") == 0) {
                strncpy(config->document_root, value, sizeof(config->document_root) - 1);
                config->document_root[sizeof(config->document_root) - 1] = '\0';
            }
        }
    }

    fclose(file);
    return 0;
}

// Function to handle client requests
void handle_request(int client_socket, const Config *config) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        char method[16], path[256], http_version[32];
        if (sscanf(buffer, "%15s %255s %31s", method, path, http_version) == 3) {

            if (strcmp(method, "GET") == 0) {
                char filepath[512];
                snprintf(filepath, sizeof(filepath), "%s%s", config->document_root, path);

                if (strcmp(path, "/") == 0) {
                    strcat(filepath, "/index.html");
                }

                FILE *file = fopen(filepath, "rb");

                if (file) {
                    char response_header[1024];
                    snprintf(response_header, sizeof(response_header),
                             "HTTP/1.1 200 OK\r\n"
                             "Content-Type: text/html\r\n"
                             "\r\n");

                    send(client_socket, response_header, strlen(response_header), 0);

                    size_t bytes_read;
                    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
                        send(client_socket, buffer, bytes_read, 0);
                    }
                    fclose(file);
                } else {
                    const char *not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
                    send(client_socket, not_found_response, strlen(not_found_response), 0);
                }
            } else {
                const char *not_implemented_response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n";
                send(client_socket, not_implemented_response, strlen(not_implemented_response), 0);
            }
        }
    }

    close(client_socket);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        return 1;
    }

    Config config;
    if (load_config(argv[1], &config) != 0) {
        return 1;
    }

    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len = sizeof(client_address);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating socket");
        return 1;
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(config.port);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Error binding socket");
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, 5) == -1) {
        perror("Error listening on socket");
        close(server_socket);
        return 1;
    }

    printf("Server listening on port %d, document root: %s\n", config.port, config.document_root);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

        handle_request(client_socket, &config);
    }

    close(server_socket);
    return 0;
}
