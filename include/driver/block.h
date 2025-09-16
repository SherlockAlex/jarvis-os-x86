#ifndef OS_DRIVER_BLOCK_H
#define OS_DRIVER_BLOCK_H

#include <stdtype.h>
#include <driver/driver.h>
#include <kernel/interrupt/interrupt.h>

#define BLOCK_SIZE 512  // 标准块大小
#define MAX_BLOCK_DEVICES 256

// 块设备操作函数指针类型
typedef void (*block_read_func)(uint32_t block_num, uint8_t* buffer);
typedef void (*block_write_func)(uint32_t block_num, uint8_t* buffer);

// 块设备结构
typedef struct BlockDevice {
    uint32_t base_port;          // 基础端口号
    uint32_t interrupt_line;     // 中断线
    uint32_t block_count;        // 设备总块数
    block_read_func read;        // 读块函数
    block_write_func write;      // 写块函数
    uint32_t lock;               // 锁，用于并发访问
} __attribute__((packed)) BlockDevice;

// 块设备驱动结构
typedef struct BlockDriver {
    Driver base;                 // 基础驱动结构
    BlockDevice device;          // 块设备
} __attribute__((packed)) BlockDriver;

// 全局变量声明
extern BlockDevice* active_block_devices[MAX_BLOCK_DEVICES];
extern uint32_t num_block_devices;

// 函数声明
extern BlockDriver* create_block_driver(BlockDevice* dev);
extern void block_device_initialize(BlockDevice* dev, uint32_t base_port, uint32_t interrupt_line);
extern void block_read(uint32_t block_num, uint8_t* buffer);
extern void block_write(uint32_t block_num, uint8_t* buffer);
extern uint32_t block_interrupt_handler(uint32_t esp);

extern void ide_read_sector(uint32_t sector, uint8_t* buffer);
extern void ide_write_sector(uint32_t sector, uint8_t* buffer);
extern int ide_check_drive_exists();

#endif