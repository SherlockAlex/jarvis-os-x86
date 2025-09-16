#include <stdtype.h>
#include <driver/keyboard.h>
#include <kernel/ioctl.h>
#include <kernel/kerio.h>
#include <kernel/interrupt/interrupt.h>
#include <kernel/memory/malloc.h>
#include <fs/devfs.h>

// 键盘扫描码到ASCII字符的映射表（未按下Shift）
static const char keyboard_map_normal[128] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// 键盘扫描码到ASCII字符的映射表（按下Shift）
static const char keyboard_map_shift[128] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// 全局键盘驱动实例
static KeyboardDriver* keyboard_driver = 0;

// 键盘设备节点
static DeviceNode keyboard_device_node;

// 键盘激活函数
static void keyboard_activate() {
    kernel_printf("Keyboard driver activated\n");
}

// 键盘重置函数
static int keyboard_reset() {
    keyboard_clear_buffer();
    keyboard_driver->keyboard_flags = 0;
    kernel_printf("Keyboard driver reset\n");
    return 0;
}

// 键盘禁用函数
static void keyboard_deactivate() {
    kernel_printf("Keyboard driver deactivated\n");
}

// 创建键盘驱动
Driver* create_keyboard_driver(InterruptManager* interrupt_manager) {
    if (keyboard_driver != 0) {
        return (Driver*)keyboard_driver;
    }
    
    keyboard_driver = (KeyboardDriver*)malloc(sizeof(KeyboardDriver));
    if (keyboard_driver == 0) {
        return 0;
    }
    
    keyboard_driver->base.activate = keyboard_activate;
    keyboard_driver->base.reset = keyboard_reset;
    keyboard_driver->base.deactivate = keyboard_deactivate;
    keyboard_driver->interrupt_manager = interrupt_manager;
    keyboard_driver->buffer_start = 0;
    keyboard_driver->buffer_end = 0;
    keyboard_driver->keyboard_flags = 0;
    keyboard_driver->last_scancode = 0;
    
    // 注册键盘中断处理程序
    InterruptHandler* handler = (InterruptHandler*)malloc(sizeof(InterruptHandler));
    if (handler == 0) {
        free(keyboard_driver);
        keyboard_driver = 0;
        return 0;
    }
    
    handler->handle_interrupt_function = keyboard_interrupt_handler;
    interrupt_manager->handlers[0x21] = handler;
    
    // 将键盘驱动注册到devfs
    strcpy(keyboard_device_node.name, "keyboard");
    keyboard_device_node.type = DEV_TYPE_CHAR;
    keyboard_device_node.major = 1;
    keyboard_device_node.minor = 0;
    keyboard_device_node.device_data = keyboard_driver;
    
    if (devfs_register_device(keyboard_device_node.name, keyboard_device_node.type, 
                              keyboard_device_node.major, keyboard_device_node.minor, 
                              keyboard_device_node.device_data) == 0) {
        kernel_printf("Keyboard device registered successfully as /dev/keyboard\n");
    } else {
        kernel_printf("Failed to register keyboard device\n");
    }
    
    return (Driver*)keyboard_driver;
}

// 键盘中断处理函数
uint32_t keyboard_interrupt_handler(uint32_t esp) {
    uint8_t scancode = keyboard_read_scancode();
    char c = 0;
    
    // 处理按键释放（扫描码最高位为1）
    if (scancode & 0x80) {
        uint8_t released_key = scancode & 0x7F;
        
        switch (released_key) {
            case KEYBOARD_SCANCODE_LEFT_SHIFT:
            case KEYBOARD_SCANCODE_RIGHT_SHIFT:
                keyboard_driver->keyboard_flags &= ~KEYBOARD_FLAG_SHIFT;
                break;
            case KEYBOARD_SCANCODE_LEFT_CTRL:
                keyboard_driver->keyboard_flags &= ~KEYBOARD_FLAG_CTRL;
                break;
            case KEYBOARD_SCANCODE_LEFT_ALT:
                keyboard_driver->keyboard_flags &= ~KEYBOARD_FLAG_ALT;
                break;
        }
    } 
    // 处理按键按下
    else {
        switch (scancode) {
            case KEYBOARD_SCANCODE_LEFT_SHIFT:
            case KEYBOARD_SCANCODE_RIGHT_SHIFT:
                keyboard_driver->keyboard_flags |= KEYBOARD_FLAG_SHIFT;
                break;
            case KEYBOARD_SCANCODE_LEFT_CTRL:
                keyboard_driver->keyboard_flags |= KEYBOARD_FLAG_CTRL;
                break;
            case KEYBOARD_SCANCODE_LEFT_ALT:
                keyboard_driver->keyboard_flags |= KEYBOARD_FLAG_ALT;
                break;
            case KEYBOARD_SCANCODE_CAPS_LOCK:
                keyboard_driver->keyboard_flags ^= KEYBOARD_FLAG_CAPS_LOCK;
                break;
            case KEYBOARD_SCANCODE_NUM_LOCK:
                keyboard_driver->keyboard_flags ^= KEYBOARD_FLAG_NUM_LOCK;
                break;
            case KEYBOARD_SCANCODE_SCROLL_LOCK:
                keyboard_driver->keyboard_flags ^= KEYBOARD_FLAG_SCROLL_LOCK;
                break;
            default:
                // 将扫描码转换为ASCII字符并存入缓冲区
                c = keyboard_scancode_to_ascii(scancode);
                if (c != 0) {
                    // 在中断处理程序中，我们不需要禁用中断，因为中断已经禁用
                    if (!keyboard_is_buffer_full()) {
                        keyboard_driver->keyboard_buffer[keyboard_driver->buffer_end] = c;
                        keyboard_driver->buffer_end = (keyboard_driver->buffer_end + 1) % 256;
                    }
                }
                break;
        }
        
        keyboard_driver->last_scancode = scancode;
    }
    
    return esp;
}

// 从键盘数据端口读取扫描码
uint8_t keyboard_read_scancode() {
    return read_8bit(KEYBOARD_DATA_PORT);
}

// 将扫描码转换为ASCII字符
char keyboard_scancode_to_ascii(uint8_t scancode) {
    if (scancode > 127) {
        return 0;
    }
    
    // 检查是否按下Shift键或Caps Lock状态
    uint8_t shift = (keyboard_driver->keyboard_flags & KEYBOARD_FLAG_SHIFT) || 
                   (keyboard_driver->keyboard_flags & KEYBOARD_FLAG_CAPS_LOCK);
    
    // 根据Shift状态选择映射表
    char c = shift ? keyboard_map_shift[scancode] : keyboard_map_normal[scancode];
    
    // 如果按下Ctrl键，返回控制字符
    if (keyboard_driver->keyboard_flags & KEYBOARD_FLAG_CTRL) {
        if (c >= 'a' && c <= 'z') {
            return c - 'a' + 1; // Ctrl+A -> 0x01, Ctrl+B -> 0x02, etc.
        }
    }
    
    return c;
}

// 从键盘缓冲区读取一个字符
uint8_t keyboard_getchar() {
    uint8_t c = 0;
    // 禁用中断以确保进程安全
    asm volatile("cli");
    if (!keyboard_is_buffer_empty()) {
        c = keyboard_driver->keyboard_buffer[keyboard_driver->buffer_start];
        keyboard_driver->buffer_start = (keyboard_driver->buffer_start + 1) % 256;
    }
    // 启用中断
    asm volatile("sti");
    return c;
}

// 清空键盘缓冲区
void keyboard_clear_buffer() {
    // 禁用中断以确保进程安全
    asm volatile("cli");
    keyboard_driver->buffer_start = 0;
    keyboard_driver->buffer_end = 0;
    // 启用中断
    asm volatile("sti");
}

// 检查键盘缓冲区是否为空
uint8_t keyboard_is_buffer_empty() {
    // 这个函数只在中断处理程序或已禁用中断的上下文中调用，所以不需要额外禁用中断
    return keyboard_driver->buffer_start == keyboard_driver->buffer_end;
}

// 检查键盘缓冲区是否已满
uint8_t keyboard_is_buffer_full() {
    // 这个函数只在中断处理程序中调用，所以不需要禁用中断
    return (keyboard_driver->buffer_end + 1) % 256 == keyboard_driver->buffer_start;
}
