#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 3702  // Standard WSD port
#define MAX_PACKET_SIZE 1024
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

void encode_text(const char *input, const char *cipher_map, char *output) {
    while (*input) {
        if (*input >= 'a' && *input <= 'z') {
            *output = cipher_map[*input - 'a'];
        } else if (*input >= 'A' && *input <= 'Z') {
            *output = cipher_map[*input - 'A'] - ('a' - 'A');
        } else {
            *output = *input; // Copy non-alphabet characters as-is
        }
        input++;
        output++;
    }
    *output = '\0';
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <IP_ADDRESS> <UUID> <TEXT_TO_ENCODE>\n", argv[0]);
        exit(1);
    }

    const char *ip_address = argv[1];
    const char *uuid = argv[2];
    const char *text_to_encode = argv[3];

    // Fixed cipher map
    const char *cipher_map = "qwertyuiopasdfghjklzxcvbnm";
    char encoded_text[UUID_LENGTH + 1];

    encode_text(text_to_encode, cipher_map, encoded_text);

    // XML template with placeholders for custom data
    const char *xml_template = "<?xml version=\"1.0\" encoding=\"utf-8\"?><soap:Envelope xmlns:soap=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\" xmlns:wsd=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\"><soap:Header><wsa:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</wsa:To><wsa:Action>http://schemas.xmlsoap.org/ws/2005/04/discovery/Resolve</wsa:Action><wsa:MessageID>urn:uuid:%s</wsa:MessageID></soap:Header><soap:Body><wsd:Resolve><wsa:EndpointReference><wsa:Address>urn:uuid:%s</wsa:Address></wsa:EndpointReference></wsd:Resolve></soap:Body></soap:Envelope>";

    // Create a buffer for the final packet data
    char wsd_packet[MAX_PACKET_SIZE];
    snprintf(wsd_packet, MAX_PACKET_SIZE, xml_template, uuid, encoded_text);

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
