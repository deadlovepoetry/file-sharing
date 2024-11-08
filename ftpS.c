// Updated server code with login and signup features
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/types.h>

#define SERVER_CONTROL_PORT 4000
#define CLIENT_DATA_PORT 5100
#define MAX 81
#define USER_DB "user_db.txt"

bool authenticateUser(char *username, char *password);
bool registerUser(char *username, char *password);
void handleClientRequest(int clientC_FD);
void handleUserCommands(int clientC_FD);

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

void handleUserCommands(int clientC_FD) {
    char buffer[MAX];
    while (1) {
        bzero(buffer, MAX);
        recv(clientC_FD, buffer, sizeof(buffer), 0);
        if (strcmp(buffer, "quit") == 0) {
            break;
        } else if (strncmp(buffer, "get", 3) == 0) {
            // Handle file transfer
        } else if (strncmp(buffer, "put", 3) == 0) {
            // Handle file upload
        } else {
            send(clientC_FD, "502 Command not implemented", 27, 0);
        }
    }
}

int main() {
    int sockC_FD, clientC_FD;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    sockC_FD = socket(AF_INET, SOCK_STREAM, 0);
    if (sockC_FD < 0) {
        perror("Socket creation failed");
        exit(0);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_CONTROL_PORT);

    if (bind(sockC_FD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(0);
    }

    if (listen(sockC_FD, 10) < 0) {
        perror("Listen failed");
        exit(0);
    }

    printf("Server listening on port %d...\n", SERVER_CONTROL_PORT);

    while ((clientC_FD = accept(sockC_FD, (struct sockaddr *)&client_addr, &client_len)) >= 0) {
        printf("Client connected\n");
        handleClientRequest(clientC_FD);
        close(clientC_FD);
    }

    close(sockC_FD);
    return 0;
}

