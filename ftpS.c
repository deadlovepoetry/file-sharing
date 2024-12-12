#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <dirent.h>
#define SERVER_CONTROL_PORT 3000
#define CLIENT_DATA_PORT 3100
#define MAX 81
#define USER_DB "user_db.txt"

bool authenticateUser(char *username, char *password) {
    FILE *fp = fopen(USER_DB, "r");
    if (fp == NULL) {
        perror("Error opening user database");
        return false;
    }

    char dbUsername[50], dbPassword[50];
    while (fscanf(fp, "%s %s", dbUsername, dbPassword) != EOF) {
        if (strcmp(dbUsername, username) == 0 && strcmp(dbPassword, password) == 0) {
            fclose(fp);
            return true;
        }
    }

    fclose(fp);
    return false;
}

bool registerUser(char *username, char *password) {
    FILE *fp = fopen(USER_DB, "a");
    if (fp == NULL) {
        perror("Error opening user database");
        return false;
    }

    fprintf(fp, "%s %s\n", username, password);
    fclose(fp);
    return true;
}

void parseFileName(char *buffer, char *filename, int i) {
    strcpy(filename, buffer + i); // first i blocks contain command and space
}

int getServerDataSocket() {
    int sockFD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFD < 0) {
        perror("Failed to create server data socket");
        exit(EXIT_FAILURE);
    }
    return sockFD;
}
void handleUserCommands(int clientC_FD);

// Function to send a file to the client
void sendFileFunc(FILE *fp, const char *filename) {
    char buffer[MAX];
    int bytes_read;
    struct sockaddr_in client_addr;
    int serverD_FD = getServerDataSocket();
    bzero(&client_addr, sizeof(client_addr));

    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(CLIENT_DATA_PORT);

    if (connect(serverD_FD, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Error connecting with client");
        close(serverD_FD);
        return;
    }

    // Get file information (size and modification time)
    struct stat file_info;
    if (stat(filename, &file_info) < 0) {
        perror("Error getting file stats");
        close(serverD_FD);
        return;
    }

    // Prepare the metadata string
    char metadata[MAX];
    snprintf(metadata, MAX, "Name:%s|Size:%ld|Time:%ld|", filename, file_info.st_size, file_info.st_mtime);
    printf("Sending metadata: %s\n", metadata);

    // Send the metadata
    if (write(serverD_FD, metadata, strlen(metadata)) < 0) {
        perror("Failed to send metadata");
        close(serverD_FD);
        return;
    }

    // Send file data
    while ((bytes_read = fread(buffer + 3, sizeof(char), MAX - 3, fp)) > 0) {
        buffer[0] = (feof(fp)) ? 'L' : '*';  // 'L' for last block, '*' for regular
        short data_length = htons(bytes_read);  // Store the length of the data
        memcpy(buffer + 1, &data_length, sizeof(short));

        int bytes_sent = write(serverD_FD, buffer, bytes_read + 3);  // Send data with metadata size
        if (bytes_sent < 0) {
            perror("Send failed");
            break;
        }
    }

    close(serverD_FD);
}


// Function to receive a file from the client
// Function to receive a file from the client
void getFile(char *filename) {
    char buffer[MAX];
    struct sockaddr_in client_addr;
    int serverD_FD = getServerDataSocket();
    bzero(&client_addr, sizeof(client_addr));

    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    client_addr.sin_port = htons(CLIENT_DATA_PORT);

    if (connect(serverD_FD, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Error connecting with client");
        close(serverD_FD);
        return;
    }

    FILE *fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("Error opening file");
        close(serverD_FD);
        return;
    }

    // Receive metadata first
    int bytes_received = recv(serverD_FD, buffer, MAX, 0);
    if (bytes_received <= 0) {
        perror("Failed to receive metadata");
        close(serverD_FD);
        fclose(fp);
        return;
    }
    buffer[bytes_received] = '\0';  // Null-terminate metadata
    printf("Metadata received: %s\n", buffer);

    // Parse metadata
    char filename_received[100];
    long file_size, file_time;
    sscanf(buffer, "Name:%[^|]|Size:%ld|Time:%ld|", filename_received, &file_size, &file_time);
    printf("Filename: %s, Size: %ld, Time: %ld\n", filename_received, file_size, file_time);

    while (1) {
        int bytes_recv = recv(serverD_FD, buffer, MAX, 0);
        if (bytes_recv <= 0) {
            break;  // End of file or error
        }

        char flag = buffer[0];
        short data_length = ntohs(*(short *)(buffer + 1));
        fwrite(buffer + 3, sizeof(char), data_length, fp);  // Write file data
        if (flag == 'L') {  // 'L' means last block
            break;
        }
    }

    fclose(fp);
    close(serverD_FD);
}


// Handle client requests for signup, login, and command handling
void handleClientRequest(int clientC_FD) {
    char buffer[MAX], response[MAX];
    bzero(buffer, MAX);

    recv(clientC_FD, buffer, sizeof(buffer), 0);

    if (strncmp(buffer, "signup", 6) == 0) {
        char *username = strtok(buffer + 7, " ");
        char *password = strtok(NULL, " ");
        if (registerUser(username, password)) {
            strcpy(response, "User registered successfully");
        } else {
            strcpy(response, "Failed to register user");
        }
    } else if (strncmp(buffer, "login", 5) == 0) {
        char *username = strtok(buffer + 6, " ");
        char *password = strtok(NULL, " ");
        if (authenticateUser(username, password)) {
            strcpy(response, "200 OK");
            send(clientC_FD, response, strlen(response), 0);
            handleUserCommands(clientC_FD);
            return;
        } else {
            strcpy(response, "401 Unauthorized");
        }
    } else {
        strcpy(response, "400 Bad Request");
    }

    send(clientC_FD, response, strlen(response), 0);
}

// Handle user commands like "get", "put", and "quit" after login
void handleUserCommands(int clientC_FD) {
    char buffer[MAX], server_response[MAX];
    while (1) {
        bzero(buffer, MAX);
        recv(clientC_FD, buffer, sizeof(buffer), 0);

        if (strncmp(buffer, "cd", 2) == 0) {
            char cmd[10];
            strcpy(cmd, buffer + 3);
            if (chdir(cmd) < 0) {
                strcpy(server_response, "501");
            } else {
                strcpy(server_response, "200");
            }
            write(clientC_FD, server_response, sizeof(server_response));
        } else if (strncmp(buffer, "get", 3) == 0) {
            char filename[20];
            parseFileName(buffer, filename, 4);
            FILE *fp = fopen(filename, "rb");
            if (fp == NULL) {
                strcpy(server_response, "550");
            } else {
                strcpy(server_response, "201");
                write(clientC_FD, server_response, sizeof(server_response));
                sleep(1);
                sendFileFunc(fp, filename);  // Provide both fp and filename here
                fclose(fp);
            }
        } else if (strncmp(buffer, "put", 3) == 0) {
            char filename[20];
            parseFileName(buffer, filename, 4);
            sleep(1);
            getFile(filename);
        } else if (strcmp(buffer, "quit") == 0) {
            strcpy(server_response, "421");
            write(clientC_FD, server_response, sizeof(server_response));
            break;
        } else {
            strcpy(server_response, "502 Command not implemented");
            write(clientC_FD, server_response, sizeof(server_response));
        }
    }
}


int main() {
    int sockC_FD, clientC_FD;
    struct sockaddr_in server_addr, client_addr;
    sockC_FD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockC_FD < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sockC_FD, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_CONTROL_PORT);

    if (bind(sockC_FD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(sockC_FD, 10) < 0) {
        perror("Listening error");
        exit(EXIT_FAILURE);
    }
    printf("Server listening...\n");

    int len = sizeof(client_addr);
    while ((clientC_FD = accept(sockC_FD, (struct sockaddr *)&client_addr, &len)) >= 0) {
        handleClientRequest(clientC_FD);
        close(clientC_FD);
    }

    if (clientC_FD < 0) {
        perror("Error accepting client connection");
    }

    close(sockC_FD);
    return 0;
}

