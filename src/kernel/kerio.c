#include "stdtype.h"
#include "kernel/kerio.h"

extern void printk(const char *str);
extern void print_hex(uint32_t key);
extern void print_char(char c);

extern void print_int(int32_t num);
extern void print_float(double num, uint8_t precision);
extern int uint64_to_string(uint64_t num, char *str, int base);
extern int uint32_to_string(uint32_t num, char *str, int base);
extern int int64_to_string(int64_t num, char *str, int base);
extern int double_to_string(double num, char *str, int precision);
extern int int_to_string(int32_t num, char *str, int base);

typedef struct ScreenCursor
{
    uint8_t x,y;
} ScreenCursor;


ScreenCursor screen_cursor = {0, 0};

// 内联函数用于端口I/O
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t value;
    asm volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

// 辅助函数保持不变，但添加静态内联优化
static inline void reverse_string(char *str, int length) {
    int start = 0;
    int end = length - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }
}

// 内联函数提高访问效率
static inline uint16_t get_buffer_value(uint16_t *video_memory, uint8_t x, uint8_t y) {
    return video_memory[VIDEO_WIDTH * y + x];
}

static inline void set_buffer_value(uint16_t *video_memory, uint8_t x, uint8_t y, uint16_t value) {
    video_memory[VIDEO_WIDTH * y + x] = value;
}

char get_char(uint16_t *video_memory, uint8_t x, uint8_t y) {
    if (x >= VIDEO_WIDTH || y >= VIDEO_HEIGHT) return '\0';
    return (char)(get_buffer_value(video_memory, x, y) & 0x00FF);
}

void put_char(uint16_t *video_memory, uint8_t x, uint8_t y, char c) {
    uint16_t value = get_buffer_value(video_memory, x, y);
    set_buffer_value(video_memory, x, y, (value & 0xFF00) | c);
}

void full_remainder_char(uint16_t *video_memory, uint8_t start_x, uint8_t y) {
    // 使用内存设置优化而不是逐个字符写入
    for (uint8_t x = start_x; x < VIDEO_WIDTH; x++) {
        put_char(video_memory, x, y, ' ');
    }
}

void next_line(uint16_t *video_memory) {
    // 使用内存复制优化屏幕滚动
    // 复制整个屏幕向上移动一行
    for (uint8_t y = 1; y < VIDEO_HEIGHT; y++) {
        for (uint8_t x = 0; x < VIDEO_WIDTH; x++) {
            uint16_t value = get_buffer_value(video_memory, x, y);
            set_buffer_value(video_memory, x, y - 1, value);
        }
    }
    
    // 清除最后一行
    full_remainder_char(video_memory, 0, VIDEO_HEIGHT - 1);
    
    screen_cursor.x = 0;
    screen_cursor.y = VIDEO_HEIGHT - 1;
}

// 预计算常用值避免重复计算
static const uint32_t HEX_SHIFTS[8] = {28, 24, 20, 16, 12, 8, 4, 0};


// 非格式话打印字符串到屏幕
void printk(const char *str)
{
    static uint16_t *video_memory = (uint16_t *)VIDEO_MEMORY;

    for (int i = 0; str[i]; i++) {
        switch (str[i]) {
        case '\n':
            full_remainder_char(video_memory, screen_cursor.x, screen_cursor.y);
            screen_cursor.y++;
            screen_cursor.x = 0;
            break;
        case '\b':
            if (screen_cursor.x > 0) {
                screen_cursor.x--;
                put_char(video_memory, screen_cursor.x, screen_cursor.y, ' ');
            } else if (screen_cursor.y > 0) {
                screen_cursor.y--;
                screen_cursor.x = VIDEO_WIDTH - 1;
                put_char(video_memory, screen_cursor.x, screen_cursor.y, ' ');
            }
            break;
        case '\t':
            screen_cursor.x = (screen_cursor.x + 8) & ~7;
            break;
        default:
            put_char(video_memory, screen_cursor.x, screen_cursor.y, str[i]);
            screen_cursor.x++;
            break;
        }

        if (screen_cursor.x >= VIDEO_WIDTH) {
            screen_cursor.x = 0;
            screen_cursor.y++;
        }

        if (screen_cursor.y >= VIDEO_HEIGHT) {
            next_line(video_memory);
        }
    }
}

void print_char(char c) {
    // 使用静态缓冲区避免每次分配内存
    static char buffer[2] = {0, 0};
    buffer[0] = c;
    printk(buffer);
}

void print_hex(uint32_t key)
{
    // 使用静态缓冲区避免每次分配内存
    static char hex_str[9] = {0};
    static const char *hex = "0123456789ABCDEF";
    
    for (int i = 0; i < 8; i++) {
        hex_str[i] = hex[(key >> HEX_SHIFTS[i]) & 0x0F];
    }
    
    printk(hex_str);
}

void clean()
{
    screen_cursor.x = 0;
    screen_cursor.y = 0;
    uint16_t *video_memory = (uint16_t *)VIDEO_MEMORY;
    
    // 批量设置空格字符，减少函数调用
    for (uint16_t i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++) {
        video_memory[i] = (video_memory[i] & 0xFF00) | ' ';
    }
}

void update_video_buffer(uint16_t *buffer) {
    static uint16_t *video_memory = (uint16_t *)VIDEO_MEMORY;
    
    // 使用内存复制优化
    for (size_t i = 0; i < VIDEO_HEIGHT * VIDEO_WIDTH; i++) {
        video_memory[i] = buffer[i];
    }
}

int int_to_string(int32_t num, char *str, int base) {
    int i = 0;
    int is_negative = 0;
    
    // 处理0
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    // 处理负数（仅限十进制）
    if (num < 0 && base == 10) {
        is_negative = 1;
        num = -num;
    }
    
    // 转换数字
    while (num != 0) {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    
    // 添加负号
    if (is_negative) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    reverse_string(str, i);
    return i;
}

int double_to_string(double num, char *str, int precision) {
    int i = 0;
    
    // 处理负数
    if (num < 0) {
        str[i++] = '-';
        num = -num;
    }
    
    // 提取整数部分
    int int_part = (int)num;
    i += int_to_string(int_part, &str[i], 10);
    
    // 添加小数点
    str[i++] = '.';
    
    // 提取小数部分
    double fractional_part = num - (double)int_part;
    for (int j = 0; j < precision; j++) {
        fractional_part *= 10;
        int digit = (int)fractional_part;
        str[i++] = digit + '0';
        fractional_part -= digit;
    }
    
    str[i] = '\0';
    return i;
}

// 实现print_int函数
void print_int(int32_t num) {
    char buffer[32];
    int length = int_to_string(num, buffer, 10);
    
    for (int i = 0; i < length; i++) {
        print_char(buffer[i]);
    }
}

// 实现print_float函数
void print_float(double num, uint8_t precision) {
    char buffer[64];
    int length = double_to_string(num, buffer, precision);
    
    for (int i = 0; i < length; i++) {
        print_char(buffer[i]);
    }
}
// 打印long int
void print_long(int64_t num) {
    char buffer[32];
    int length = int64_to_string(num, buffer, 10);
    
    for (int i = 0; i < length; i++) {
        print_char(buffer[i]);
    }
}

// 打印long long int
void print_llong(int64_t num) {
    // 在大多数平台上，long long和long是相同的
    print_long(num);
}

// 打印unsigned int
void print_uint(uint32_t num) {
    char buffer[32];
    int length = uint32_to_string(num, buffer, 10);
    
    for (int i = 0; i < length; i++) {
        print_char(buffer[i]);
    }
}

// 打印unsigned long
void print_ulong(uint64_t num) {
    char buffer[32];
    int length = uint64_to_string(num, buffer, 10);
    
    for (int i = 0; i < length; i++) {
        print_char(buffer[i]);
    }
}

// 打印unsigned long long
void print_ullong(uint64_t num) {
    // 在大多数平台上，unsigned long long和unsigned long是相同的
    print_ulong(num);
}

// 十六进制打印unsigned long
void print_hex_long(uint64_t num) {
    // 使用静态缓冲区避免每次分配内存
    static char hex_str[17] = {0};
    static const char *hex = "0123456789ABCDEF";
    
    for (int i = 0; i < 16; i++) {
        hex_str[i] = hex[(num >> (60 - i * 4)) & 0x0F];
    }
    
    printk(hex_str);
}

// 十六进制打印unsigned long long
void print_hex_llong(uint64_t num) {
    print_hex_long(num);
}

// 实现int64到字符串的转换
int int64_to_string(int64_t num, char *str, int base) {
    int i = 0;
    int is_negative = 0;
    
    // 处理0
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    // 处理负数（仅限十进制）
    if (num < 0 && base == 10) {
        is_negative = 1;
        num = -num;
    }
    
    // 将64位数分解为两个32位数进行处理
    uint32_t high = (uint32_t)(num >> 32);
    uint32_t low = (uint32_t)num;
    
    // 处理高位部分
    if (high > 0) {
        // 这里需要特殊处理高位部分
        // 简化实现：直接使用十六进制表示大数
        if (base == 16) {
            // 十六进制可以直接处理
            while (high != 0) {
                int rem = high % base;
                str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
                high = high / base;
            }
        } else {
            // 对于十进制，我们简化处理，只显示低32位
            // 或者可以使用其他方法处理大数
            str[i++] = 'b'; // 标记这是一个大数
            str[i++] = 'i';
            str[i++] = 'g';
            str[i++] = ':';
            low = (uint32_t)num; // 只显示低32位
        }
    }
    
    // 处理低位部分
    while (low != 0) {
        int rem = low % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        low = low / base;
    }
    
    // 添加负号
    if (is_negative) {
        str[i++] = '-';
    }
    
    str[i] = '\0';
    reverse_string(str, i);
    return i;
}

// 实现uint32到字符串的转换
int uint32_to_string(uint32_t num, char *str, int base) {
    int i = 0;
    
    // 处理0
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    // 转换数字
    while (num != 0) {
        uint32_t rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }
    
    str[i] = '\0';
    reverse_string(str, i);
    return i;
}

// 实现uint64到字符串的转换
// 修改后的uint64_to_string函数，避免使用64位除法和取模
int uint64_to_string(uint64_t num, char *str, int base) {
    int i = 0;
    
    // 处理0
    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return i;
    }
    
    // 将64位数分解为两个32位数进行处理
    uint32_t high = (uint32_t)(num >> 32);
    uint32_t low = (uint32_t)num;
    
    // 处理高位部分
    if (high > 0) {
        // 这里需要特殊处理高位部分
        // 简化实现：直接使用十六进制表示大数
        if (base == 16) {
            // 十六进制可以直接处理
            while (high != 0) {
                int rem = high % base;
                str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
                high = high / base;
            }
        } else {
            // 对于十进制，我们简化处理，只显示低32位
            // 或者可以使用其他方法处理大数
            str[i++] = 'b'; // 标记这是一个大数
            str[i++] = 'i';
            str[i++] = 'g';
            str[i++] = ':';
            low = (uint32_t)num; // 只显示低32位
        }
    }
    
    // 处理低位部分
    while (low != 0) {
        int rem = low % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        low = low / base;
    }
    
    str[i] = '\0';
    reverse_string(str, i);
    return i;
}

// 更新kernel_printf函数以支持新的格式说明符
void kernel_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            // 检查长度修饰符
            if (format[i] == 'l') {
                i++;
                if (format[i] == 'l') {
                    i++;
                    // 处理long long类型
                    switch (format[i]) {
                        case 'd': 
                        case 'i': {
                            int64_t num = va_arg(args, int64_t);
                            print_llong(num);
                            break;
                        }
                        case 'u': {
                            uint64_t num = va_arg(args, uint64_t);
                            print_ullong(num);
                            break;
                        }
                        case 'x': 
                        case 'X': {
                            uint64_t num = va_arg(args, uint64_t);
                            print_hex_llong(num);
                            break;
                        }
                        default: {
                            print_char('%');
                            print_char('l');
                            print_char('l');
                            print_char(format[i]);
                            break;
                        }
                    }
                } else {
                    // 处理long类型
                    switch (format[i]) {
                        case 'd': 
                        case 'i': {
                            int64_t num = va_arg(args, int64_t);
                            print_long(num);
                            break;
                        }
                        case 'u': {
                            uint64_t num = va_arg(args, uint64_t);
                            print_ulong(num);
                            break;
                        }
                        case 'x': 
                        case 'X': {
                            uint64_t num = va_arg(args, uint64_t);
                            print_hex_long(num);
                            break;
                        }
                        default: {
                            print_char('%');
                            print_char('l');
                            print_char(format[i]);
                            break;
                        }
                    }
                }
            } else {
                // 处理普通类型
                switch (format[i]) {
                    case 'd': 
                    case 'i': {
                        int32_t num = va_arg(args, int32_t);
                        print_int(num);
                        break;
                    }
                    case 'u': {
                        uint32_t num = va_arg(args, uint32_t);
                        print_uint(num);
                        break;
                    }
                    case 'f': {
                        double num = va_arg(args, double);
                        print_float(num, 6); // 默认6位精度
                        break;
                    }
                    case 'x': 
                    case 'X': {
                        uint32_t num = va_arg(args, uint32_t);
                        print_hex(num);
                        break;
                    }
                    case 'c': {
                        char c = va_arg(args, int);
                        print_char(c);
                        break;
                    }
                    case 's': {
                        const char *str = va_arg(args, const char*);
                        printk(str);
                        break;
                    }
                    case '%': {
                        print_char('%');
                        break;
                    }
                    default: {
                        print_char('%');
                        print_char(format[i]);
                        break;
                    }
                }
            }
        } else {
            print_char(format[i]);
        }
    }
    
    va_end(args);
}// 设置光标位置
extern void set_cursor_position(uint8_t x, uint8_t y) {
    // 确保坐标在有效范围内
    if (x >= VIDEO_WIDTH) x = VIDEO_WIDTH - 1;
    if (y >= VIDEO_HEIGHT) y = VIDEO_HEIGHT - 1;
    
    // 更新内部光标位置
    screen_cursor.x = x;
    screen_cursor.y = y;
    
    // 计算光标在VGA缓冲区中的位置
    uint16_t cursor_position = y * VIDEO_WIDTH + x;
    
    // 发送命令到VGA控制端口
    outb(VGA_COMMAND_PORT, VGA_CURSOR_LOW_BYTE);
    outb(VGA_DATA_PORT, (uint8_t)(cursor_position & 0xFF));
    outb(VGA_COMMAND_PORT, VGA_CURSOR_HIGH_BYTE);
    outb(VGA_DATA_PORT, (uint8_t)((cursor_position >> 8) & 0xFF));
}

// 获取当前光标位置
extern void get_cursor_position(uint8_t* x, uint8_t* y) {
    if (x) *x = screen_cursor.x;
    if (y) *y = screen_cursor.y;
}

// 显示光标
extern void show_cursor() {
    // 读取当前光标状态
    outb(VGA_COMMAND_PORT, 0x0A);
    uint8_t cursor_start = inb(VGA_DATA_PORT);
    
    // 清除光标隐藏位
    outb(VGA_COMMAND_PORT, 0x0A);
    outb(VGA_DATA_PORT, cursor_start & 0x1F);
    
    // 更新光标位置
    update_cursor();
}

// 隐藏光标
extern void hide_cursor() {
    // 设置光标起始扫描线大于结束扫描线，这样光标就会隐藏
    outb(VGA_COMMAND_PORT, 0x0A);
    outb(VGA_DATA_PORT, 0x20);
}

// 更新光标位置，使其与内部光标状态一致
extern void update_cursor() {
    set_cursor_position(screen_cursor.x, screen_cursor.y);
}

// 获取当前屏幕X坐标
extern uint8_t get_screen_x() {
    return screen_cursor.x;
}

// 获取当前屏幕Y坐标
extern uint8_t get_screen_y() {
    return screen_cursor.y;
}