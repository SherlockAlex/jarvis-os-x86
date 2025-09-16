#ifndef OS_KERNEL_IOCTL
#define OS_KERNEL_IOCTL

#include <stdtype.h>

// 为驱动端口，编写驱动程序时后用到

extern void write_8bit(uint16_t,uint8_t);
extern void write_8bit_slow(uint16_t,uint8_t);
extern void write_16bit(uint16_t,uint16_t);
extern void write_32bit(uint16_t,uint32_t);

extern uint8_t read_8bit(uint16_t);
extern uint16_t read_16bit(uint16_t);
extern uint16_t read_32bit(uint16_t);

#endif