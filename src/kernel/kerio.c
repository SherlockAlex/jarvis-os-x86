#include "stdtype.h"
#include "kernel/kerio.h"

extern void printk(const char *str);

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

int handler_printf(const char *format, va_list args)
{
    
    // 使用小型固定缓冲区替代动态分配的大缓冲区
    // 对于32位整数，最多需要11个字符（包括符号和终止符）
    char small_buffer[64]; // 足够处理各种格式转换
    
    // 用于跟踪可变参数的索引
    int arg_index = 0;
    
    // 解析格式字符串并直接输出
    while (*format != '\0') {
        if (*format != '%') {
            // 直接输出非格式字符
            print_char(*format++);
            continue;
        }
        
        format++; // 跳过 %
        
        // 解析标志
        int flags = 0;
        while (1) {
            if (*format == '-') flags |= 0x01; // 左对齐
            else if (*format == '+') flags |= 0x02; // 带符号
            else if (*format == ' ') flags |= 0x04; // 空格填充
            else if (*format == '#') flags |= 0x10; // 替代形式
            else if (*format == '0') flags |= 0x08; // 零填充
            else break;
            format++;
        }
        
        // 解析宽度
        int width = 0;
        if (*format >= '0' && *format <= '9') {
            width = 0;
            while (*format >= '0' && *format <= '9') {
                width = width * 10 + (*format - '0');
                format++;
            }
        }
        
        // 解析精度
        int precision = -1; // 默认精度
        if (*format == '.') {
            format++;
            precision = 0;
            while (*format >= '0' && *format <= '9') {
                precision = precision * 10 + (*format - '0');
                format++;
            }
        }
        
        // 解析转换说明符
        switch (*format) {
            case 'd':
            case 'i': {
                // 获取下一个参数作为有符号整数
                //int32_t num = (int32_t)args[arg_index++];
                int32_t num = va_arg(args, int32_t);
                int len = int_to_string(num, small_buffer, 10, 0, width, precision < 0 ? 1 : precision, flags);
                
                // 直接输出格式化后的数字
                for (int i = 0; i < len; i++) {
                    print_char(small_buffer[i]);
                }
                break;
            }
            case 'u': {
                // 获取下一个参数作为无符号整数
                uint32_t num = va_arg(args,uint32_t);
                int len = int_to_string((int32_t)num, small_buffer, 10, 0, width, precision < 0 ? 1 : precision, flags);
                
                // 直接输出格式化后的数字
                for (int i = 0; i < len; i++) {
                    print_char(small_buffer[i]);
                }
                break;
            }
            case 'o': {
                // 获取下一个参数作为八进制数
                uint32_t num = va_arg(args,uint32_t);
                int len = int_to_string((int32_t)num, small_buffer, 8, 0, width, precision < 0 ? 1 : precision, flags);
                
                // 直接输出格式化后的数字
                for (int i = 0; i < len; i++) {
                    print_char(small_buffer[i]);
                }
                break;
            }
            case 'x': {
                // 获取下一个参数作为十六进制数（小写）
                uint32_t num = va_arg(args,uint32_t);
                int len = int_to_string((int32_t)num, small_buffer, 16, 0, width, precision < 0 ? 1 : precision, flags);
                
                // 直接输出格式化后的数字
                for (int i = 0; i < len; i++) {
                    print_char(small_buffer[i]);
                }
                break;
            }
            case 'X': {
                // 获取下一个参数作为十六进制数（大写）
                uint32_t num = va_arg(args,uint32_t);
                int len = int_to_string((int32_t)num, small_buffer, 16, 1, width, precision < 0 ? 1 : precision, flags);
                
                // 直接输出格式化后的数字
                for (int i = 0; i < len; i++) {
                    print_char(small_buffer[i]);
                }
                break;
            }
            case 'c': {
                // 获取下一个参数作为字符
                char c = va_arg(args, char);
                
                // 处理宽度
                if (width > 1 && !(flags & 0x01)) { // 有宽度且不是左对齐
                    for (int i = 1; i < width; i++) {
                        print_char(flags & 0x08 ? '0' : ' ');
                    }
                }
                
                // 输出字符
                print_char(c);
                
                // 处理左对齐的宽度
                if (width > 1 && (flags & 0x01)) {
                    for (int i = 1; i < width; i++) {
                        print_char(' ');
                    }
                }
                break;
            }
            case 's': {
                // 获取下一个参数作为字符串指针
                const char* str = va_arg(args, const char*);
                
                size_t len = strlen(str);
                if (precision >= 0 && len > (size_t)precision) {
                    len = precision;
                }
                
                // 处理宽度和对齐
                if (width > len && !(flags & 0x01)) { // 不是左对齐
                    while (width-- > len) {
                        print_char(flags & 0x08 ? '0' : ' ');
                    }
                }
                
                // 直接输出字符串
                for (size_t i = 0; i < len; i++) {
                    print_char(str[i]);
                }
                
                // 如果是左对齐，在右侧填充
                if (flags & 0x01 && width > len) {
                    while (width-- > len) {
                        print_char(' ');
                    }
                }
                break;
            }
            case 'p': {
                // 获取下一个参数作为指针地址
                uint32_t addr = va_arg(args, uint32_t);
                
                // 输出前缀
                print_char('0');
                print_char('x');
                
                // 格式化地址
                int len = int_to_string((int32_t)addr, small_buffer, 16, 0, width - 2, 1, 0);
                
                // 输出格式化后的地址
                for (int i = 0; i < len; i++) {
                    print_char(small_buffer[i]);
                }
                break;
            }
            case '%': {
                // 直接输出百分号
                print_char('%');
                break;
            }
            default:
                // 直接输出未知格式说明符
                print_char(*format);
                break;
        }
        
        format++;
    }
    
    return 0; // 成功执行
}

// 更新kernel_printf函数以支持新的格式说明符
void kernel_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    handler_printf(format, args);
    
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