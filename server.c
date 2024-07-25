#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <ctype.h>

#define PORT 3702  // Standard WSD port
#define MAX_PACKET_SIZE 1024
#define ALPHABET_SIZE 36
#define UUID_LENGTH 36

void error(const char *msg) {
    perror(msg);
    exit(1);
}

void generate_cipher_map(char *cipher_map) {
    const char *charset = "abcdefghijklmnopqrstuvwxyz0123456789";
    strcpy(cipher_map, charset);

    // Shuffle the characters
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        int j = rand() % ALPHABET_SIZE;
        char temp = cipher_map[i];
        cipher_map[i] = cipher_map[j];
        cipher_map[j] = temp;
    }
}

void encode_text(const char *input, const char *cipher_map, char *output) {
    const char *charset = "abcdefghijklmnopqrstuvwxyz0123456789";
    while (*input) {
        const char *ptr = strchr(charset, tolower(*input));
        if (ptr) {
            int index = ptr - charset;
            *output = cipher_map[index];
        } else {
            *output = *input; // Copy non-alphabet characters as-is
        }
        input++;
        output++;
    }
    *output = '\0';
}

void format_cipher_map_as_uuid(const char *cipher_map, char *uuid) {
    snprintf(uuid, UUID_LENGTH + 1, "%.8s-%.4s-%.4s-%.4s-%.12s", 
             cipher_map, cipher_map + 8, cipher_map + 12, cipher_map + 16, cipher_map + 20);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <IP_ADDRESS> <TEXT_TO_ENCODE>\n", argv[0]);
        exit(1);
    }

    const char *ip_address = argv[1];
    const char *text_to_encode = argv[2];

    srand(time(NULL));

    char cipher_map[ALPHABET_SIZE + 1];
    char formatted_cipher_map[UUID_LENGTH + 1];
    char encoded_text[UUID_LENGTH + 1];

    generate_cipher_map(cipher_map);
    format_cipher_map_as_uuid(cipher_map, formatted_cipher_map);
    encode_text(text_to_encode, cipher_map, encoded_text);

    // XML template with placeholders for custom data
    const char *xml_template = "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" xmlns:wsd=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\"><soap:Header><wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Resolve</wsa:Action><wsa:MessageID>urn:uuid:%s</wsa:MessageID></soap:Header><soap:Body><wsd:Resolve><wsa:EndpointReference><wsa:Address>urn:uuid:%s</wsa:Address></wsa:EndpointReference></wsd:Resolve></soap:Body></soap:Envelope>";

    // Create a buffer for the final packet data
    char wsd_packet[MAX_PACKET_SIZE];
    snprintf(wsd_packet, MAX_PACKET_SIZE, xml_template, formatted_cipher_map, encoded_text);

    int sockfd;
    struct sockaddr_in dest_addr;
    socklen_t addr_len = sizeof(dest_addr);

    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        error("Socket creation failed");
    }

    // Set destination address
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip_address, &dest_addr.sin_addr) <= 0) {
        error("Invalid address or address not supported");
    }

    // Send WSD packet
    if (sendto(sockfd, wsd_packet, strlen(wsd_packet), 0, (struct sockaddr *)&dest_addr, addr_len) < 0) {
        error("Sendto failed");
    }

    printf("WSD packet sent to %s:%d\n", ip_address, PORT);

    // Close socket
    close(sockfd);

    return 0;
}
