#include <stdtype.h>

void write_8bit(uint16_t _port, uint8_t _data)
{
    __asm__ volatile("outb %0, %1" : : "a"(_data), "Nd"(_port));
}

void write_8bit_slow(uint16_t _port, uint8_t _data)
{
    __asm__ volatile("outb %0, %1\njmp 1f\n1: jmp 1f\n1:" : : "a"(_data), "Nd"(_port));
}

void write_16bit(uint16_t _port, uint16_t _data)
{
    __asm__ volatile("outw %0, %1" : : "a"(_data), "Nd"(_port));
}

void write_32bit(uint16_t _port, uint32_t _data)
{
    __asm__ volatile("outl %0, %1" : : "a"(_data), "Nd"(_port));
}

uint8_t read_8bit(uint16_t _port)
{
    uint8_t result;
    __asm__ volatile("inb %1, %0" : "=a"(result) : "Nd"(_port));
    return result;
}

uint16_t read_16bit(uint16_t _port)
{
    uint16_t result;
    __asm__ volatile("inw %1, %0" : "=a" (result) : "Nd" (_port));
    return result;
}

uint32_t read_32bit(uint16_t _port)
{
    uint32_t result;
    __asm__ volatile("inl %1, %0" : "=a" (result) : "Nd" (_port));
    return result;
}