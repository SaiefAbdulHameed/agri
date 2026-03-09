#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../include/utils.h"

void log_info(const char* message) {
    printf("[INFO] %s\n", message);
}

void log_error(const char* message) {
    fprintf(stderr, "[ERROR] %s\n", message);
}
void trim(char *s) {
    char *end;

    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
        s++;

    end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\n' || *end == '\r' || *end == '\t'))
        *end-- = '\0';
}

void* safe_malloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) {
        log_error("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void str_copy(char* dest, const char* src, int max_len) {
    strncpy(dest, src, max_len - 1);
    dest[max_len - 1] = '\0';
}
