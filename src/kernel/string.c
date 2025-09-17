#include <kernel/string.h>

void *memcpy(void *str1, const void *str2, size_t n) {
    // 将 void 指针转换为 char 指针以便逐字节操作
    char *dest = (char *)str1;
    const char *src = (const char *)str2;
    
    // 逐字节复制数据
    for (size_t i = 0; i < n; i++) {
        dest[i] = src[i];
    }
    
    // 返回目标指针
    return str1;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

size_t strlen(const char* str)
{
    
    if(!str)
    {
        return 0;
    }

    size_t len = 0;
    char c = str[len];
    while (c != '\0')
    {
        len++;
        c = str[len];
    }

    return len;
    
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return (int)(p1[i] - p2[i]);
        }
    }
    
    return 0;
}

char* strcpy(char* dest, const char* src) {
    char* original_dest = dest;  // 保存目标字符串的起始地址
    while (*src != '\0') {       // 复制非终止符的字符
        *dest = *src;
        dest++;
        src++;
    }
    *dest = '\0';                // 添加终止符
    return original_dest;        // 返回目标字符串的起始地址
}

int strncmp(const char *s1, const char *s2, size_t n) {
    // 处理 n 为 0 的特殊情况
    if (n == 0) {
        return 0;
    }
    
    // 逐个字符比较，直到遇到不同字符、遇到字符串结束符或比较完 n 个字符
    while (n-- > 0 && *s1 && *s2) {
        if (*s1 != *s2) {
            // 返回第一个不同字符的差值
            return (unsigned char)*s1 - (unsigned char)*s2;
        }
        s1++;
        s2++;
    }
    
    // 如果 n 减到 0，说明前 n 个字符都相同
    if (n == (size_t)-1) { // 或者检查 n 是否已经减完
        return 0;
    }
    
    // 如果一个字符串提前结束，比较剩余部分
    return (unsigned char)*s1 - (unsigned char)*s2;
}

void* memset(void* s, int c, size_t n) {
    unsigned char* p = (unsigned char*) s;
    unsigned char uc = (unsigned char) c;
    for (size_t i = 0; i < n; i++) {
        p[i] = uc;
    }
    return s;
}

char *strchr(const char *s, int c) {
    // 将整数c转换为字符类型
    char ch = (char)c;
    // 遍历字符串，直到遇到结束符
    while (*s != '\0') {
        if (*s == ch) {
            // 找到匹配字符，返回当前指针（需要去掉const限定）
            return (char *)s;
        }
        s++;
    }
    // 检查是否查找结束符'\0'
    if (ch == '\0') {
        return (char *)s;
    }
    // 未找到字符，返回NULL
    return 0;
}

// strrchr函数实现：从字符串末尾开始查找指定字符
char *strrchr(const char *s, int c) {
    // 将整数c转换为字符类型
    char ch = (char)c;
    // 保存最后一次出现的位置
    char *last = NULL;
    
    // 遍历整个字符串
    while (*s != '\0') {
        if (*s == ch) {
            // 找到匹配字符，更新最后一次出现的位置
            last = (char *)s;
        }
        s++;
    }
    
    // 检查是否查找结束符'\0'
    if (ch == '\0') {
        return (char *)s;
    }
    
    // 返回最后一次出现的位置，如果没找到则返回NULL
    return last;
}

// 自定义strncpy函数实现
char *strncpy(char *dest, const char *src, size_t n) {
    // 保存目标字符串的起始地址用于返回
    char *original_dest = dest;
    
    // 复制字符，直到复制n个字符或遇到源字符串的结束符
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    
    // 如果源字符串长度小于n，用空字符填充剩余空间
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    
    return original_dest;
}

// 简单实现snprintf，支持%d格式
int snprintf(char *str, size_t size, const char *format, ...) {
    if (!str || size == 0) {
        return 0;
    }
    
    va_list args;
    va_start(args, format);
    
    char *p = str;
    const char *f = format;
    size_t written = 0;
    
    while (*f && written < size - 1) {
        if (*f == '%' && *(f+1) == 'd') {
            // 处理%d格式
            int num = va_arg(args, int);
            
            // 处理负数
            if (num < 0) {
                *p++ = '-';
                written++;
                num = -num;
            }
            
            // 计算数字的位数
            int temp = num;
            int digits = 0;
            do {
                digits++;
                temp /= 10;
            } while (temp > 0);
            
            // 确保有足够的空间
            if (written + digits >= size - 1) {
                break;
            }
            
            // 填充数字（从右到左）
            p += digits;
            written += digits;
            do {
                *(--p) = '0' + (num % 10);
                num /= 10;
            } while (num > 0);
            p += digits;
            
            f += 2; // 跳过"%d"
        } else {
            *p++ = *f++;
            written++;
        }
    }
    
    *p = '\0'; // 确保字符串以null结尾
    
    va_end(args);
    
    return written;
}

// strdup函数实现
extern char *strdup(const char *s) {
    if (!s) {
        return NULL;
    }
    
    size_t len = strlen(s) + 1; // +1 for null terminator
    char *dup = (char*)malloc(len);
    
    if (dup) {
        strcpy(dup, s);
    }
    
    return dup;
}

// 以下是va_list相关的简单定义
// 注意：这只是为了让代码编译通过的简化实现
#ifndef va_list
#define va_list char*
#define va_start(ap, v) ap = (va_list)&v + sizeof(v)
#define va_arg(ap, t) (*(t*)((ap += sizeof(t)) - sizeof(t)))
#define va_end(ap) ap = 0
#endif

size_t int_to_string(int32_t num, char* buffer, int base, int uppercase, int width, int precision, int flags)
{
    char digits[] = "0123456789abcdef0123456789ABCDEF";
    char* start = buffer;
    char* ptr = buffer;
    int negative = 0;
    int prefix_len = 0;
    int i;
    
    // 处理零值的特殊情况
    if (num == 0 && precision == 0) {
        return 0;
    }
    
    // 处理负数
    if (num < 0 && base == 10) {
        negative = 1;
        num = -num;
    }
    
    // 处理前缀 (# 标志)
    if (flags & 0x10 && num != 0) { // # 标志
        if (base == 8) {
            *ptr++ = '0';
            prefix_len = 1;
        } else if (base == 16) {
            *ptr++ = '0';
            *ptr++ = uppercase ? 'X' : 'x';
            prefix_len = 2;
        }
    }
    
    // 转换数字 - 使用32位运算避免调用未定义的64位函数
    do {
        int digit = (int)(num % (int32_t)base);
        *ptr++ = digits[uppercase ? digit + 16 : digit];
        num = (int32_t)(num / (int32_t)base);
    } while (num > 0 || ptr - buffer - prefix_len < precision);
    
    // 处理宽度和填充
    int num_digits = ptr - buffer;
    int pad = width > num_digits ? width - num_digits : 0;
    
    if (!(flags & 0x01)) { // 不是左对齐
        if (flags & 0x08) { // 0 标志
            // 左填充0
            for (i = 0; i < pad; i++) {
                *--start = '0';
            }
        } else {
            // 左填充空格
            for (i = 0; i < pad; i++) {
                *--start = ' ';
            }
        }
    }
    
    // 添加符号
    if (negative) {
        *--start = '-';
    } else if (flags & 0x02) { // + 标志
        *--start = '+';
    } else if (flags & 0x04) { // 空格标志
        *--start = ' ';
    }
    
    // 如果是左对齐，在右侧填充空格
    if (flags & 0x01 && pad > 0) {
        for (i = 0; i < pad; i++) {
            *ptr++ = ' ';
        }
    }
    
    // 反转数字部分
    char* left = start + (negative || flags & 0x02 || flags & 0x04 ? 1 : 0) + prefix_len;
    char* right = ptr - (flags & 0x01 && pad > 0 ? pad : 0) - 1;
    while (left < right) {
        char temp = *left;
        *left++ = *right;
        *right-- = temp;
    }
    
    // 添加结束符
    *ptr = '\0';
    
    return ptr - start;
}

// 自定义实现需要的字符串函数
char* strtok(char* str, const char* delim) {
    static char* last = NULL;
    char* token;
    
    if (str == NULL) {
        str = last;
    }
    
    if (str == NULL) {
        return NULL;
    }
    
    // 跳过前导分隔符
    while (*str && strchr(delim, *str)) {
        str++;
    }
    
    if (*str == '\0') {
        last = NULL;
        return NULL;
    }
    
    token = str;
    
    // 找到下一个分隔符
    while (*str && !strchr(delim, *str)) {
        str++;
    }
    
    if (*str) {
        *str = '\0';
        last = str + 1;
    } else {
        last = NULL;
    }
    
    return token;
}