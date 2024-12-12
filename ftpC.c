

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
#include <sys/types.h>
#include <netinet/in.h>

#include <dirent.h>
#include <stdio.h>
#define CLIENT_DATA_PORT 3100
#define SERVER_CONTROL_PORT 3000
#define MAX 81

void signup(int clientC_FD);
void login(int clientC_FD);
void handleClientConnection(int clientC_FD);





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

    printf("\n\nEnter the login credentials\n\n");

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
    printf("\n-----------------------------------------------\n");
    printf("1. Sign Up\n");
    printf("2. Login\n");
    printf("3. Quit");
    printf("\n-----------------------------------------------\n");
}

void parseFileName(char *buffer, char *filename, int i)
{
    strcpy(filename,buffer+i);
}

int getClientDataSocket()
{
    int socketFD;
    struct sockaddr_in server_addr;

    // socket creation
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0)
    {
        printf("socket creation failed..\n");
        exit(0);
    }

    // allowing reuse of address and port
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    bzero(&server_addr, sizeof(server_addr));

    // assigning ip and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(CLIENT_DATA_PORT);

    // binding socket to ip
    if ((bind(socketFD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0))
    {
        printf("Bind failed");
        exit(0);
    }

    return socketFD;
}

void checkStatusCode(char *response, int clientC_FD)
{
    int code = atoi(response);

    switch (code)
    {

    case 200:
    {
        printf("\nsuccessful\n\n");
        break;
    }
    case 503:
    {
        printf("\ninvalid server request\n\n");
        close(clientC_FD);
        exit(0);
    }
    case 550:
    {
        printf("\ninvalid request\n\n");
        close(clientC_FD);
        exit(0);
    }
    case 201:
    {
        printf("\n file exists and ready for transfer\n\n");
        break;
    }
    case 501:
    {
        printf("\nerror changing directory\n");
        break;
    }
    }
    return;
}
void getFileWithMetadata(int clientC_FD, char *buffer) {
    char metadata[MAX];
    bzero(metadata, MAX);

    struct sockaddr_in server_data_addr;
    int serverD_FD, clientD_FD = getClientDataSocket();

    if (listen(clientD_FD, 10) < 0) {
        printf("Data port listening error\n");
        return;
    }

    printf("Data port started listening\n");

    int len = sizeof(server_data_addr);
    serverD_FD = accept(clientD_FD, (struct sockaddr *)&server_data_addr, &len);
    if (serverD_FD < 0) {
        printf("Error accepting server data port\n");
        close(clientD_FD);
        return;
    }

    // Receive metadata
    if (recv(serverD_FD, metadata, MAX, 0) <= 0) {
        printf("Failed to receive metadata\n");
        close(clientD_FD);
        close(serverD_FD);
        return;
    }

    // Parse metadata
    char filename[100];
    long filesize, timestamp;
    sscanf(metadata, "Name:%[^|]|Size:%ld|Time:%ld|", filename, &filesize, &timestamp);

    printf("Receiving file: %s (%ld bytes, timestamp: %ld)\n", filename, filesize, timestamp);

    // Receive file data
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("File open failed");
        close(clientD_FD);
        close(serverD_FD);
        return;
    }

    long received = 0;
    while (received < filesize) {
        int bytes_recv = recv(serverD_FD, buffer, MAX, 0);
        if (bytes_recv <= 0) {
            break;
        }

        fwrite(buffer, sizeof(char), bytes_recv, fp);
        received += bytes_recv;
    }

    fclose(fp);
    printf("File received successfully.\n");

    close(clientD_FD);
    close(serverD_FD);
}
void getFile(int clientC_FD, char *buffer) {
    getFileWithMetadata(clientC_FD, buffer);
    printf("Received metadata: %s\n", buffer);

}

/*
void getFile(int clientC_FD,char *buffer)
{
   
    char response[MAX];
    bzero(response, MAX);

    struct sockaddr_in server_data_addr;
    int serverD_FD, clientD_FD = getClientDataSocket();

    if (listen(clientD_FD, 10) < 0)
    {
        printf("Data port listening error\n");
        return;
    }
    else
    {
        printf("Data port started listening\n");
    }

    int len = sizeof(server_data_addr);
    serverD_FD = accept(clientD_FD, (struct sockaddr *)&server_data_addr, &len);
    if (serverD_FD < 0)
    {
        printf("error accepting server data port\n");
        close(clientD_FD);
        return;
    }

    char filename[100];

    //parseFileName(buffer,filename,4);
    FILE *fp = fopen("received.txt", "wb");  // Open file for writing in binary mode
    while (1) {
        int bytes_recv = recv(serverD_FD, buffer, MAX, 0);
        //printf("%d\n",bytes_recv);
        if (bytes_recv <= 0) {
            break;
        }

        char flag = buffer[0];
        short data_length = ntohs(*(short*)(buffer + 1));

        // write data into file
        fwrite(buffer + 3, sizeof(char), data_length, fp);  // Write received data to file
        if (flag == 'L') {
            // Last block received, break out of loop
            break;
        }
    }

    fclose(fp);
    close(clientD_FD);
    
}*/
void sendFileWithMetadata(char *filename) {
    char metadata[MAX];
    char buffer[MAX];
    bzero(metadata, MAX);
    bzero(buffer, MAX);

    struct sockaddr_in server_data_addr;
    int serverD_FD, clientD_FD = getClientDataSocket();

    if (listen(clientD_FD, 10) < 0) {
        printf("Data port listening error\n");
        return;
    }

    printf("Data port started listening\n");

    int len = sizeof(server_data_addr);
    serverD_FD = accept(clientD_FD, (struct sockaddr *)&server_data_addr, &len);
    if (serverD_FD < 0) {
        printf("Error accepting server data port\n");
        close(clientD_FD);
        return;
    }

    // Get file metadata
    struct stat file_stat;
    if (stat(filename, &file_stat) < 0) {
        perror("File stat failed");
        close(clientD_FD);
        close(serverD_FD);
        return;
    }

    // Prepare metadata: Name, Size, Timestamp
    snprintf(metadata, MAX, "Name:%s|Size:%ld|Time:%ld|Type:binary\n",
             filename,
             file_stat.st_size,
             file_stat.st_mtime);

    // Send metadata
    if (send(serverD_FD, metadata, strlen(metadata), 0) < 0) {
        perror("Failed to send metadata");
        close(clientD_FD);
        close(serverD_FD);
        return;
    }

    // Send file data
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("File open failed");
        close(clientD_FD);
        close(serverD_FD);
        return;
    }

    while (!feof(fp)) {
        int bytes_read = fread(buffer, sizeof(char), MAX, fp);
        if (bytes_read > 0) {
            if (send(serverD_FD, buffer, bytes_read, 0) < 0) {
                perror("File data send failed");
                fclose(fp);
                close(clientD_FD);
                close(serverD_FD);
                return;
            }
        }
    }

    fclose(fp);
    printf("File and metadata sent successfully.\n");
    close(clientD_FD);
    close(serverD_FD);
}
void sendFile(char *filename) {
    sendFileWithMetadata(filename);
   // printf("Sending metadata: %s\n", metadata);

}

/*void sendFile(char * filename)
{
    char buffer[MAX];
    bzero(buffer, MAX);

    struct sockaddr_in server_data_addr;
    int serverD_FD, clientD_FD = getClientDataSocket();

    if (listen(clientD_FD, 10) < 0)
    {
        printf("Data port listening error\n");
        return;
    }
    else
    {
        printf("Data port started listening\n");
    }

    int len = sizeof(server_data_addr);
    serverD_FD = accept(clientD_FD, (struct sockaddr *)&server_data_addr, &len);
    if (serverD_FD < 0)
    {
        printf("error accepting server data port\n");
        close(clientD_FD);
        return;
    }

    // send blocks of data 
    FILE *fp = fopen(filename,"rb");
    int bytes_read;
    while ((bytes_read = fread(buffer + 3, sizeof(char), MAX, fp)) > 0) {
        // Set flag character based on remaining data
        buffer[0] = (feof(fp)) ? 'L' : '*';  // 'L' for last block, '*' for others

        // Convert data length (excluding header) to network byte order
        // | L/* | length | length | data | data...
        short data_length = htons(bytes_read);
        memcpy(buffer + 1, &data_length, sizeof(short));

        // Send the entire block (header + data)
        int bytes_sent = write(serverD_FD, buffer, MAX);
        if (bytes_sent < 0) {
            perror("send failed");
            break;
        }
    }


}*/

void handleClientConnection(int clientC_FD)
{
    char buffer[MAX];
    char response[MAX];

    bzero(buffer, MAX);
    sprintf(buffer, "%d", CLIENT_DATA_PORT);

    // send port Y to server
    write(clientC_FD, buffer, sizeof(buffer));
    bzero(response, MAX);

    recv(clientC_FD, response, sizeof(response), 0);
    checkStatusCode(response, clientC_FD);

    // options for the client
    while (1)
    {
        bool flag = false;
        int ch;
        printf("\n-----------------------------------------------\n");
        printf(" 1. Change server directory\n 2. Get file from server \n 3. Send file to server\n 4. quit\n");
        printf("\n-----------------------------------------------\n");
        scanf("%d", &ch);
        getchar();
        bzero(buffer, MAX);
        switch (ch)
        {
        case 1:
        {
            printf("\n-----------------------------------------------\n");
            printf("Enter the command to change directory (like > cd path)\n");
            fflush(stdin);
            fgets(buffer, sizeof(buffer), stdin);  
            
            // Remove trailing newline (optional)
            buffer[strcspn(buffer, "\n")] = '\0'; 

            // send the command to the server
            write(clientC_FD, buffer, sizeof(buffer));

            // receive status code
            recv(clientC_FD, response, sizeof(response), 0);
            checkStatusCode(response, clientC_FD);

            break;
        }
        case 2:
        {
            printf("\n-----------------------------------------------\n");
            printf("enter the command (like > get filename)\n");
            fflush(stdin);
            //scanf("%[^\n]s",buffer);
            //read(stdin,buffer,sizeof(buffer));
            fgets(buffer, sizeof(buffer), stdin);  
            
            // Remove trailing newline (optional)
            buffer[strcspn(buffer, "\n")] = '\0'; 

            // send the command to the server
            write(clientC_FD, buffer, sizeof(buffer));

            // receive status code
            recv(clientC_FD, response, sizeof(response), 0);
            checkStatusCode(response, clientC_FD);

            //receive the file from the server
            getFile(clientC_FD,buffer);
            break;
        }
        case 3:
        {
            printf("\n-----------------------------------------------\n");
            printf("enter the command (like > put filename)\n");
            fflush(stdin);
            //scanf("%[^\n]s",buffer);
            //read(stdin,buffer,sizeof(buffer));
            fgets(buffer, sizeof(buffer), stdin);  
            
            // Remove trailing newline (optional)
            buffer[strcspn(buffer, "\n")] = '\0'; 

            // send the command to the server
            write(clientC_FD, buffer, sizeof(buffer));

            // parse filename
            char filename[20];
            parseFileName(buffer,filename,4);

            // send file function
            sendFile(filename);
            break;
        }
        case 4:
        {
            flag = true;
            break;
        }
        }
        if (flag)
            break;
    }
    close(clientC_FD);
}

int main()
{
    int clientC_FD;
    struct sockaddr_in server_addr;

    clientC_FD = socket(AF_INET, SOCK_STREAM, 0);

    if (clientC_FD < 0)
    {
        printf("failed to create client socket\n");
        exit(0);
    }

    bzero(&server_addr, sizeof(server_addr));

    // assign server ip and port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(SERVER_CONTROL_PORT);

    // connect client with server
    if (connect(clientC_FD, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        printf("error connecting with server...\n");
        exit(0);
    }
    else
        printf("--Connected to Server successfully--\n");

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
    
    return 0;
}
