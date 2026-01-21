#ifndef UTILS_H
#define UTILS_H

#include<stdio.h>
#include<stdlib.h>
#include<string.h>

void log_info(const char* message);
void log_error(const char* message);

void* safe_malloc(size_t size);
void str_copy(char* dest, const char* src, int max_len);

#endif
