#ifndef OS_DRIVER_KEYBOARD
#define OS_DRIVER_KEYBOARD

#include <stdtype.h>
#include <driver/driver.h>
#include <kernel/interrupt/interrupt.h>

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_COMMAND_PORT 0x64

// 键盘状态标志
#define KEYBOARD_FLAG_SHIFT 0x01
#define KEYBOARD_FLAG_CTRL 0x02
#define KEYBOARD_FLAG_ALT 0x04
#define KEYBOARD_FLAG_CAPS_LOCK 0x08
#define KEYBOARD_FLAG_NUM_LOCK 0x10
#define KEYBOARD_FLAG_SCROLL_LOCK 0x20

// 特殊键扫描码
#define KEYBOARD_SCANCODE_ESC 0x01
#define KEYBOARD_SCANCODE_BACKSPACE 0x0E
#define KEYBOARD_SCANCODE_TAB 0x0F
#define KEYBOARD_SCANCODE_ENTER 0x1C
#define KEYBOARD_SCANCODE_LEFT_CTRL 0x1D
#define KEYBOARD_SCANCODE_LEFT_SHIFT 0x2A
#define KEYBOARD_SCANCODE_RIGHT_SHIFT 0x36
#define KEYBOARD_SCANCODE_LEFT_ALT 0x38
#define KEYBOARD_SCANCODE_CAPS_LOCK 0x3A
#define KEYBOARD_SCANCODE_F1 0x3B
#define KEYBOARD_SCANCODE_F2 0x3C
#define KEYBOARD_SCANCODE_F3 0x3D
#define KEYBOARD_SCANCODE_F4 0x3E
#define KEYBOARD_SCANCODE_F5 0x3F
#define KEYBOARD_SCANCODE_F6 0x40
#define KEYBOARD_SCANCODE_F7 0x41
#define KEYBOARD_SCANCODE_F8 0x42
#define KEYBOARD_SCANCODE_F9 0x43
#define KEYBOARD_SCANCODE_F10 0x44
#define KEYBOARD_SCANCODE_F11 0x57
#define KEYBOARD_SCANCODE_F12 0x58
#define KEYBOARD_SCANCODE_NUM_LOCK 0x45
#define KEYBOARD_SCANCODE_SCROLL_LOCK 0x46
#define KEYBOARD_SCANCODE_UP 0x48
#define KEYBOARD_SCANCODE_LEFT 0x4B
#define KEYBOARD_SCANCODE_RIGHT 0x4D
#define KEYBOARD_SCANCODE_DOWN 0x50
#define KEYBOARD_SCANCODE_INSERT 0x52
#define KEYBOARD_SCANCODE_DELETE 0x53

typedef struct KeyboardDriver {
    Driver base;
    InterruptManager* interrupt_manager;
    uint8_t keyboard_buffer[256];
    uint16_t buffer_start;
    uint16_t buffer_end;
    uint8_t keyboard_flags;
    uint8_t last_scancode;
} KeyboardDriver;

// 键盘驱动函数
extern Driver* create_keyboard_driver(InterruptManager* interrupt_manager);
extern uint32_t keyboard_interrupt_handler(uint32_t esp);
extern uint8_t keyboard_read_scancode();
extern char keyboard_scancode_to_ascii(uint8_t scancode);
extern uint8_t keyboard_getchar();
extern void keyboard_clear_buffer();
extern uint8_t keyboard_is_buffer_empty();
extern uint8_t keyboard_is_buffer_full();

#endif