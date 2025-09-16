#include <stdtype.h>
#include <driver/block.h>
#include <kernel/ioctl.h>
#include <kernel/kerio.h>
#include <kernel/memory/malloc.h>
#include <fs/devfs.h>

BlockDevice* active_block_devices[MAX_BLOCK_DEVICES];
uint32_t num_block_devices = 0;

// 锁操作函数
static void acquire_lock(uint32_t *lock) {
    while (__sync_lock_test_and_set(lock, 1)) {
        asm volatile ("pause");
    }
}

static void release_lock(uint32_t *lock) {
    __sync_lock_release(lock);
}

void ide_initialize(uint32_t base_port, uint32_t interrupt_line) {
    // 选择主驱动器
    write_8bit(base_port + 6, 0xA0);
    
    // 简短延迟
    for (int i = 0; i < 4; i++) read_8bit(base_port + 7);
    
    // 重置控制器
    write_8bit(base_port + 6, 0x04);
    for (int i = 0; i < 4; i++) read_8bit(base_port + 7);
    
    // 选择主驱动器
    write_8bit(base_port + 6, 0xA0);
    
    // 等待控制器就绪
    int timeout = 100000;
    while ((read_8bit(base_port + 7) & 0x80) && timeout--) {
        asm volatile ("pause");
    }
    
    if (timeout == 0) {
        kernel_printf("IDE controller not responding\n");
        return;
    }
    
    kernel_printf("IDE controller initialized\n");
}

void ide_read_sector(uint32_t sector, uint8_t* buffer) {
    acquire_lock(&active_block_devices[0]->lock);
    
    // 设置LBA地址
    write_8bit(0x1F6, 0xE0 | ((sector >> 24) & 0x0F));
    write_8bit(0x1F2, 1);
    write_8bit(0x1F3, sector & 0xFF);
    write_8bit(0x1F4, (sector >> 8) & 0xFF);
    write_8bit(0x1F5, (sector >> 16) & 0xFF);
    write_8bit(0x1F7, 0x20); // 读命令

    // 等待数据就绪，添加超时
    uint32_t timeout = 100000; // 超时计数器
    uint8_t status;
    
    while (((status = read_8bit(0x1F7)) & 0x80) && timeout--) {
        // 等待BSY位清零
        asm volatile ("pause");
    }
    
    if (timeout == 0) {
        kernel_printf("Timeout waiting for BSY to clear\n");
        release_lock(&active_block_devices[0]->lock);
        return;
    }
    
    // 等待DRQ数据就绪位
    timeout = 100000;
    while (!(status = read_8bit(0x1F7) & 0x08) && timeout--) {
        // 检查错误
        status = read_8bit(0x1F7);
        if (status & 0x01) {
            kernel_printf("IDE error occurred\n");
            release_lock(&active_block_devices[0]->lock);
            return;
        } else {
            kernel_printf("IDE waiting\n");
        }
        asm volatile ("pause");
    }
    
    if (timeout == 0) {
        kernel_printf("Timeout waiting for DRQ\n");
        release_lock(&active_block_devices[0]->lock);
        return;
    }

    // 读取数据
    for (int i = 0; i < 256; i++) {
        uint16_t data = read_16bit(0x1F0);
        buffer[i*2] = data & 0xFF;
        buffer[i*2+1] = (data >> 8) & 0xFF;
    }
    
    release_lock(&active_block_devices[0]->lock);
}

void ide_write_sector(uint32_t sector, uint8_t* buffer) {

    // 往块设备中写入数据
    
    acquire_lock(&active_block_devices[0]->lock);
    // 设置LBA地址
    // 向磁盘发送写命令
    write_8bit(0x1F6, 0xE0 | ((sector >> 24) & 0x0F));
    write_8bit(0x1F2, 1);
    write_8bit(0x1F3, sector & 0xFF);
    write_8bit(0x1F4, (sector >> 8) & 0xFF);
    write_8bit(0x1F5, (sector >> 16) & 0xFF);
    write_8bit(0x1F7, 0x30); // 写命令

    // 等待准备就绪
    while ((read_8bit(0x1F7) & 0x88) != 0x08) {
        // 忙等待
    }

    // 写入数据
    for (int i = 0; i < 256; i++) {
        uint16_t data = (buffer[i*2+1] << 8) | buffer[i*2];
        write_16bit(0x1F0, data);
    }
    release_lock(&active_block_devices[0]->lock);
}

int ide_check_drive_exists() {
    // 选择主驱动器
    write_8bit(0x1F6, 0xA0);
    
    // 短暂延迟
    for (int i = 0; i < 4; i++) read_8bit(0x1F7);
    
    // 检查状态
    uint8_t status = read_8bit(0x1F7);
    if (status == 0xFF) {
        // 没有设备
        return 0;
    }
    
    // 等待驱动器就绪
    int timeout = 100000;
    while ((status & 0x80) && timeout--) {
        status = read_8bit(0x1F7);
        asm volatile ("pause");
    }
    
    return timeout > 0;
}

// 块设备初始化
void block_device_initialize(BlockDevice* dev, uint32_t base_port, uint32_t interrupt_line) {
    dev->base_port = base_port;
    dev->interrupt_line = interrupt_line;
    dev->read = ide_read_sector;
    dev->write = ide_write_sector;
    dev->lock = 0;
    // 假设设备有1000个块，实际中需要检测设备大小
    dev->block_count = 1000;
    ide_initialize(base_port, interrupt_line);
    kernel_printf("Block device initialized at port %x\n", base_port);
    
    // 注册到devfs
    char device_name[16];
    snprintf(device_name, sizeof(device_name), "hda%d", num_block_devices);
    if (devfs_register_device(device_name, DEV_TYPE_BLOCK, 3, num_block_devices, dev) == 0) {
        kernel_printf("Block device registered as /dev/%s\n", device_name);
    } else {
        kernel_printf("Failed to register block device\n");
    }
}

// 创建块设备驱动
BlockDriver* create_block_driver(BlockDevice* dev) {
    BlockDriver* driver = (BlockDriver*)malloc(sizeof(BlockDriver));
    if (!driver) {
        kernel_printf("Failed to allocate block driver\n");
        return 0;
    }
    driver->base.activate = 0; // 暂无特殊激活函数
    driver->base.reset = 0;
    driver->base.deactivate = 0;
    driver->device = *dev;
    return driver;
}

// 块读取函数
void block_read(uint32_t block_num, uint8_t* buffer) {
    if (num_block_devices == 0) {
        kernel_printf("No block devices available\n");
        return;
    }

    // 使用第一个块设备
    //kernel_printf("block_read: reading block %d\n", block_num);
    active_block_devices[0]->read(block_num, buffer);
}

// 块写入函数
void block_write(uint32_t block_num, uint8_t* buffer) {
    if (num_block_devices == 0) {
        kernel_printf("No block devices available\n");
        return;
    }
    active_block_devices[0]->write(block_num, buffer);
}

// 块设备中断处理函数
uint32_t block_interrupt_handler(uint32_t esp) {
    // 处理块设备中断，例如DMA完成或数据传输完成
    // 这里简单确认中断并清除状态
    write_8bit(0x1F7, read_8bit(0x1F7) | 0x02); // 清除中断状态
    //kernel_printf("Block device interrupt handled\n");
    return esp;
}

