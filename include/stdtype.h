#ifndef OS_STANDARD
#define OS_STANDARD

typedef char int8_t;
typedef unsigned char uint8_t;

typedef short int16_t;
typedef unsigned short uint16_t;

typedef int int32_t;
typedef unsigned int uint32_t;

typedef long long int int64_t;
typedef unsigned long long int uint64_t;

typedef const char *string;
typedef uint32_t size_t;

typedef char* va_list;
#define va_start(ap, last) (ap = (va_list)&last + sizeof(last))
#define va_arg(ap, type) (*(type*)((ap += sizeof(type)) - sizeof(type)))
#define va_end(ap) (ap = 0)

// 定义NULL宏
#ifndef NULL
#define NULL ((void*)0)
#endif

#endif