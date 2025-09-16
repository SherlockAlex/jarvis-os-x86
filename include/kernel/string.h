#ifndef OS_STRING
#define OS_STRING

#include <stdtype.h>

extern void* memcpy(void *str1, const void *str2, size_t n);

// 比较字符串
extern int strcmp(const char*, const char*);

extern size_t strlen(const char*);

extern char* strcpy(char* dest, const char* src);

extern int strncmp(const char *s1, const char *s2, size_t n);

extern int memcmp(const void *s1, const void *s2, size_t n);

extern void* memset(void* s, int c, size_t n);

extern char *strchr(const char *s, int c);

extern char *strncpy(char *dest, const char *src, size_t n);

extern int snprintf(char *str, size_t size, const char *format, ...);

extern char *strdup(const char *s);

#endif