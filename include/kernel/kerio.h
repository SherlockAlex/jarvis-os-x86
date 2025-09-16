#ifndef OS_KERNEL_KERIO
#define OS_KERNEL_KERIO

#define VIDEO_MEMORY 0xb8000
#define VIDEO_WIDTH 80
#define VIDEO_HEIGHT 25

#include "stdtype.h"

// VGA控制端口定义
#define VGA_COMMAND_PORT 0x3D4
#define VGA_DATA_PORT 0x3D5

// 光标控制命令
#define VGA_CURSOR_HIGH_BYTE 0x0E
#define VGA_CURSOR_LOW_BYTE 0x0F

// 光标状态
#define CURSOR_SHOW 0
#define CURSOR_HIDE 1

extern char get_char(uint16_t *video_memory, uint8_t x, uint8_t y);
extern void put_char(uint16_t*,uint8_t,uint8_t,char);

extern void clean();
extern void kernel_printf(const char *format, ...);

extern void set_cursor_position(uint8_t x, uint8_t y);
extern void get_cursor_position(uint8_t* x, uint8_t* y);
extern void show_cursor();
extern void hide_cursor();
extern void update_cursor();

extern uint8_t get_screen_x();
extern uint8_t get_screen_y();

#endif