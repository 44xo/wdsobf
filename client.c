#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 3702  // Standard WSD port
#define BUFFER_SIZE 2048
#define ALPHABET_SIZE 26
#define UUID_LENGTH 36

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void load_cipher_map(const char *cipher_map, char *reverse_map) {
    // Create the reverse substitution map
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        reverse_map[cipher_map[i] - 'a'] = 'a' + i;
    }
}

void decode_text(const char *input, const char *reverse_map, char *output) {
    while (*input) {
        if (*input >= 'a' && *input <= 'z') {
            *output = reverse_map[*input - 'a'];
        } else if (*input >= 'A' && *input <= 'Z') {
            *output = reverse_map[*input - 'A'] - ('a' - 'A');
        } else {
            *output = *input; // Copy non-alphabet characters as-is
        }
        input++;
        output++;
    }
    *output = '\0';
}

void extract_uuid(const char *xml_data, char *uuid, const char *start_tag, const char *end_tag) {
    char *start_ptr = strstr(xml_data, start_tag);
    if (start_ptr) {
        start_ptr += strlen(start_tag);
        char *end_ptr = strstr(start_ptr, end_tag);
        if (end_ptr) {
            size_t uuid_length = end_ptr - start_ptr;
            strncpy(uuid, start_ptr, uuid_length);
            uuid[uuid_length] = '\0';
        } else {
            fprintf(stderr, "End tag not found\n");
        }
    } else {
        fprintf(stderr, "Start tag not found\n");
    }
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    char buffer[BUFFER_SIZE];
    socklen_t len = sizeof(cliaddr);

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        error("Socket creation failed");
    }

    // Set server address
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        error("Bind failed");
    }

    printf("Listening for WSD packets on port %d...\n", PORT);

    while (1) {
        int n = recvfrom(sockfd, (char *)buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) {
            error("Receive failed");
        }
        buffer[n] = '\0';

        printf("Received packet from %s:%d\n", inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

        char message_id[UUID_LENGTH + 1];
        char encoded_uuid[UUID_LENGTH + 1];

        extract_uuid(buffer, message_id, "<wsa:MessageID>urn:uuid:", "</wsa:MessageID>");
        extract_uuid(buffer, encoded_uuid, "<wsa:Address>urn:uuid:", "</wsa:Address>");

        printf("Message ID (Cipher Map): %s\n", message_id);
        printf("Encoded UUID: %s\n", encoded_uuid);

        // Fixed cipher map
        const char *cipher_map = "qwertyuiopasdfghjklzxcvbnm";
        char reverse_map[ALPHABET_SIZE];
        load_cipher_map(cipher_map, reverse_map);

        char decoded_text[UUID_LENGTH + 1];
        decode_text(encoded_uuid, reverse_map, decoded_text);

        printf("Decoded Text: %s\n", decoded_text);
    }

    close(sockfd);
    return 0;
}
