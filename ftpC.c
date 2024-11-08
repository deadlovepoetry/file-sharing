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

void displayMenu();
void signup(int clientC_FD);
void login(int clientC_FD);
void handleClientConnection(int clientC_FD);
void checkStatusCode(char *response, int clientC_FD);
void getFile(int clientC_FD, const char *filename);
void sendFile(int clientC_FD, const char *filename);
void parseFileName(const char *command, char *filename, int offset);

void signup(int clientC_FD) {
    char buffer[MAX];
    char username[25], password[25];

    printf("Sign Up\n");
    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;

    printf("Enter password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    snprintf(buffer, MAX, "signup %s %s", username, password);
    send(clientC_FD, buffer, strlen(buffer), 0);
    recv(clientC_FD, buffer, MAX, 0);
    printf("%s\n", buffer);
}

void login(int clientC_FD) {
    char buffer[MAX];
    char username[50], password[50];

    printf("Login\n");
    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;

    printf("Enter password: ");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = 0;

    // Limit the length to avoid buffer overflow
    snprintf(buffer, MAX, "login %.30s %.30s", username, password);

    printf("Sending login data: %s\n", buffer);  // Debugging statement
    if (send(clientC_FD, buffer, strlen(buffer), 0) == -1) {
        perror("Error sending login data");
        return;
    }

    // Clear buffer and receive response
    memset(buffer, 0, sizeof(buffer));
    int bytesReceived = recv(clientC_FD, buffer, MAX, 0);

    if (bytesReceived < 0) {
        perror("Error receiving server response");
        return;
    } else if (bytesReceived == 0) {
        printf("Server disconnected\n");
        return;
    }

    printf("Received response: %s\n", buffer);  // Debugging statement

    // Check response for successful login
    if (strcmp(buffer, "200 OK") == 0) {
        printf("Login successful!\n");
        handleClientConnection(clientC_FD);
    } else {
        printf("Login failed: %s\n", buffer);
    }
}

void displayMenu() {
    printf("1. Sign Up\n");
    printf("2. Login\n");
    printf("3. Quit\n");
}

int main() {
    int clientC_FD;
    struct sockaddr_in server_addr;

    clientC_FD = socket(AF_INET, SOCK_STREAM, 0);
    if (clientC_FD < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_CONTROL_PORT);

    if (connect(clientC_FD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        close(clientC_FD);
        exit(EXIT_FAILURE);
    }

    printf("Connected to server\n");

    int choice;
    while (1) {
        displayMenu();
        printf("Select option: ");
        scanf("%d", &choice);
        getchar();

        if (choice == 1) {
            signup(clientC_FD);
        } else if (choice == 2) {
            login(clientC_FD);
        } else if (choice == 3) {
            printf("Quitting...\n");
            break;
        } else {
            printf("Invalid option\n");
        }
    }

    close(clientC_FD);
    return 0;
}

void handleClientConnection(int clientC_FD) {
    char buffer[MAX];
    char response[MAX];

    bzero(buffer, MAX);
    snprintf(buffer, sizeof(buffer), "%d", CLIENT_DATA_PORT);
    write(clientC_FD, buffer, sizeof(buffer));
    bzero(response, MAX);

    recv(clientC_FD, response, sizeof(response), 0);
    checkStatusCode(response, clientC_FD);

    while (1) {
        bool flag = false;
        int ch;
        printf(" 1. To change server directory\n 2. Get file from server\n 3. Send file to server\n 4. Quit\n");
        printf("Enter your choice: ");
        scanf("%d", &ch);
        getchar();
        bzero(buffer, MAX);
        
        switch (ch) {
            case 1:
                printf("Enter the command to change directory (like > cd path):\n");
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = 0;
                write(clientC_FD, buffer, sizeof(buffer));
                recv(clientC_FD, response, sizeof(response), 0);
                checkStatusCode(response, clientC_FD);
                break;

            case 2:
                printf("Enter the command to get file (like > get filename):\n");
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = 0;
                write(clientC_FD, buffer, sizeof(buffer));
                recv(clientC_FD, response, sizeof(response), 0);
                checkStatusCode(response, clientC_FD);
                getFile(clientC_FD, buffer);
                break;

            case 3:
                printf("Enter the command to send file (like > put filename):\n");
                fgets(buffer, sizeof(buffer), stdin);
                buffer[strcspn(buffer, "\n")] = 0;
                write(clientC_FD, buffer, sizeof(buffer));
                char filename[20];
                parseFileName(buffer, filename, 4);
                sendFile(clientC_FD, filename);
                break;

            case 4:
                flag = true;
                printf("Disconnecting...\n");
                break;

            default:
                printf("Invalid option\n");
        }

        if (flag) break;
    }

    close(clientC_FD);
}

void checkStatusCode(char *response, int clientC_FD) {
    if (strcmp(response, "200 OK") == 0) {
        printf("Operation successful\n");
    } else {
        printf("Error: %s\n", response);
    }
}

void getFile(int clientC_FD, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        perror("File open error");
        return;
    }
    
    char buffer[MAX];
    int bytes;
    while ((bytes = recv(clientC_FD, buffer, MAX, 0)) > 0) {
        fwrite(buffer, 1, bytes, file);
        if (bytes < MAX) break;
    }
    
    fclose(file);
    printf("File received: %s\n", filename);
}

void sendFile(int clientC_FD, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        perror("File open error");
        return;
    }
    
    char buffer[MAX];
    int bytes;
    while ((bytes = fread(buffer, 1, MAX, file)) > 0) {
        send(clientC_FD, buffer, bytes, 0);
    }
    
    fclose(file);
    printf("File sent: %s\n", filename);
}

void parseFileName(const char *command, char *filename, int offset) {
    strncpy(filename, command + offset, strlen(command) - offset);
    filename[strlen(command) - offset] = '\0';
}

