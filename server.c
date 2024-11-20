#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ctype.h>
#include <time.h>

#pragma comment(lib, "Ws2_32.lib")

#define PORT 3702  // Same port as the server
#define SERVER_IP "10.242.2.121"  // Replace with actual server IP if needed
#define BUFFER_SIZE 2048
#define UUID_LENGTH 36
#define ALPHABET_SIZE 36

void error(const char* msg) {
    fprintf(stderr, "%s: %d\n", msg, WSAGetLastError());
    exit(1);
}

void encoding_uuid(char* uuid) {
    // Random UUID generator based on format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    int segments[] = { 8, 4, 4, 4, 12 };

    srand((unsigned int)time(NULL));  // Seed random number generator

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < segments[i]; j++) {
            int r = rand() % 36;
            if (r < 26) {
                uuid[j] = 'a' + r;  // Generate a random lowercase letter
            }
            else {
                uuid[j] = '0' + (r - 26);  // Generate a random digit
            }
        }
        uuid += segments[i];
        if (i < 4) {
            *uuid++ = '-';  // Add hyphens between segments
        }
    }
    *uuid = '\0';  // Null-terminate the string
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

void encode_text(const char* input, const char* cipher_map, char* output) {
    const char* charset = "abcdefghijklmnopqrstuvwxyz0123456789";

    while (*input) {
        const char* ptr = strchr(charset, tolower(*input));
        if (ptr) {
            int index = ptr - charset;
            *output = cipher_map[index];
        }
        else {
            *output = *input;  // Copy non-alphabet characters as-is
        }
        output++;
        input++;
    }
    *output = '\0';
}

void send_message(SOCKET sockfd, const struct sockaddr_in* servaddr, socklen_t len, const char* message, const char* message_id) {
    char cipher_map[ALPHABET_SIZE + 1];
    load_cipher_map_from_uuid(message_id, cipher_map);

    char encoded_message[BUFFER_SIZE];
    encode_text(message, cipher_map, encoded_message);

    // Construct the full XML message with the encoded text
    char full_message[BUFFER_SIZE];
    snprintf(full_message, sizeof(full_message),
        "<?xml version=\"1.0\"?>"
        "<soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\">"
        "<soap:Header>"
        "<wsa:MessageID>urn:uuid:%s</wsa:MessageID>"
        "<wsa:Address>urn:uuid:%s</wsa:Address>"
        "</soap:Header>"
        "</soap:Envelope>", message_id, encoded_message);

    // Send the message to the server
    int n = sendto(sockfd, full_message, (int)strlen(full_message), 0, (struct sockaddr*)servaddr, len);
    if (n == SOCKET_ERROR) {
        error("Error sending message");
    }
}

int main() {
    WSADATA wsaData;
    SOCKET sockfd;
    struct sockaddr_in servaddr;

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
    servaddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &servaddr.sin_addr) <= 0) {
        error("Invalid server address");
    }

    socklen_t len = sizeof(servaddr);

    // Generate a random UUID for the message ID
    char message_id[UUID_LENGTH + 1];
    generate_random_uuid(message_id);

    // Main loop to send messages to the server
    while (1) {
        char input[BUFFER_SIZE];
        printf("Enter message (or type '!file <filepath>' to request a file): ");
        fgets(input, BUFFER_SIZE, stdin);
        input[strcspn(input, "\n")] = '\0';  // Remove newline character

        if (strlen(input) == 0) {
            continue;  // Skip empty input
        }

        // Send the message or file request to the server
        send_message(sockfd, &servaddr, len, input, message_id);

        if (strncmp(input, "!file", 5) == 0) {
            printf("File request sent for: %s\n", input + 6);  // Display the file path
        }
    }

    // Close socket
    closesocket(sockfd);

    // Cleanup Winsock
    WSACleanup();

    return 0;
}
