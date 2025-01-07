#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#define FORMAT_LENGTH 36
#define NUMBER_STR_SIZE 13
#define SEGMENT_FORMAT "%.8s-%.4s-%.4s-%.4s-%.12s"
#define LINE_COUNT(input_len) (((input_len) + FORMAT_LENGTH - 1) / FORMAT_LENGTH)

char randomPaddingCharacter() {
    return "0123456789abcdef"[rand() % 16];
}

char terminatingCharacter() {
    return "abcdef"[rand() % 6];
}

void encoding_uuid(char* uuid, int number) {
    const int segments[] = { 8, 4, 4, 4, 12 };
    int bufferIndex = 0;

    for (int i = 0; i < 5; i++) {
        int segmentSize = segments[i];

        if (i == 4) {
            int appendSize = snprintf(NULL, 0, "%da", number);
            segmentSize -= appendSize;
            bufferIndex += snprintf(uuid + bufferIndex, FORMAT_LENGTH + 1 - bufferIndex, "%d%c", number, terminatingCharacter());
        }

        for (int j = 0; j < segmentSize; j++) {
            uuid[bufferIndex++] = randomPaddingCharacter();
        }

        if (i < 4) {
            uuid[bufferIndex++] = '-';
        }
    }
    uuid[bufferIndex] = '\0';
}

int decoding_uuid(const char* uuid) {
    const char* last_segment = strrchr(uuid, '-') + 1;
    char number_str[NUMBER_STR_SIZE] = { 0 };
    int i = 0;

    while (isdigit(last_segment[i]) && i < NUMBER_STR_SIZE - 1) {
        number_str[i++] = last_segment[i];
    }
    number_str[i] = '\0';

    return atoi(number_str);
}

void xorAndConvertToHex(const char* input, char* output, const char* key) {
    for (size_t i = 0; input[i] != '\0'; i++) {
        unsigned char xor_char = input[i] ^ key[i % strlen(key)];
        snprintf(output + (i * 2), 3, "%02x", xor_char);
    }
}

void hexAndXorDecode(const char* input, char* output, const char* key) {
    for (size_t i = 0; i < strlen(input) / 2; i++) {
        unsigned char hex_byte;
        // Use scanf_s instead of sscanf
        sscanf_s(input + (i * 2), "%02hhx", &hex_byte);
        output[i] = hex_byte ^ key[i % strlen(key)];
    }
    output[strlen(input) / 2] = '\0';
}


char** split_format_and_pad(const char* input, int* line_count) {
    size_t input_len = strlen(input);
    int padding_count = (input_len % FORMAT_LENGTH) ? FORMAT_LENGTH - (input_len % FORMAT_LENGTH) : 0;
    size_t padded_len = input_len + padding_count;

    char* padded_input = (char*)malloc(padded_len + 1);
    if (!padded_input) return NULL;

    memcpy(padded_input, input, input_len);
    for (size_t i = input_len; i < padded_len; i++) {
        padded_input[i] = randomPaddingCharacter();
    }
    padded_input[padded_len] = '\0';

    *line_count = LINE_COUNT(padded_len);
    char** lines = (char**)malloc(*line_count * sizeof(char*));
    if (!lines) {
        free(padded_input);
        return NULL;
    }

    for (size_t i = 0; i < *line_count; i++) {
        lines[i] = (char*)malloc(37);
        if (!lines[i]) {
            for (size_t j = 0; j < i; j++) free(lines[j]);
            free(lines);
            free(padded_input);
            return NULL;
        }

        snprintf(lines[i], 37, SEGMENT_FORMAT,
            padded_input + i * FORMAT_LENGTH,
            padded_input + i * FORMAT_LENGTH + 8,
            padded_input + i * FORMAT_LENGTH + 12,
            padded_input + i * FORMAT_LENGTH + 16,
            padded_input + i * FORMAT_LENGTH + 20);
    }
    free(padded_input);
    return lines;
}

int main() {
    srand((unsigned int)time(NULL));

    const char* input = "skibiditoilet1234567890osdfh;oashdfohospdhfopasdfoaspdiofjopijp9rewjp9qfhj9erf";
    size_t hexOutputSize = strlen(input) * 2 + 1;
    char* hexOutput = (char*)malloc(hexOutputSize);
    char* uuid = (char*)malloc(FORMAT_LENGTH + 1);

    if (!hexOutput || !uuid) {
        printf("Memory allocation failed\n");
        free(hexOutput);
        free(uuid);
        return 1;
    }

    encoding_uuid(uuid, strlen(input));
    printf("Generated UUID: %s\n", uuid);
    xorAndConvertToHex(input, hexOutput, uuid);
    printf("Encoded Output (Hex): %s\n", hexOutput);

    int line_count;
    char** lines = split_format_and_pad(hexOutput, &line_count);
    if (!lines) {
        free(hexOutput);
        free(uuid);
        return 1;
    }

    for (int i = 0; i < line_count; i++) {
        printf("Line %d: %s\n", i + 1, lines[i]);
        free(lines[i]);
    }
    free(lines);

    int extracted_number = decoding_uuid(uuid);
    printf("Extracted number: %d\n", extracted_number);

    char* decodedOutput = (char*)malloc(hexOutputSize / 2 + 1);
    if (!decodedOutput) {
        printf("Memory allocation for decodedOutput failed\n");
        free(hexOutput);
        free(uuid);
        return 1;
    }

    hexAndXorDecode(hexOutput, decodedOutput, uuid);
    printf("Decoded Original Output: %s\n", decodedOutput);

    free(hexOutput);
    free(uuid);
    free(decodedOutput);

    return 0;
}
