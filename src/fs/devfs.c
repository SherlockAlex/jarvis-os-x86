#include <stdtype.h>
#include <kernel/string.h>
#include <fs/vfs.h>
#include <fs/devfs.h>
#include <driver/keyboard.h>
#include <kernel/memory/malloc.h>
#include <kernel/kerio.h>

#define MAX_DEVICES 64

static DeviceNode devices[MAX_DEVICES];
static uint32_t num_devices = 0;
static FileSystem devfs_fs;
static MountPoint* devfs_mount_point = NULL;

// 查找设备节点
static DeviceNode* find_device_by_name(const char* name) {
    for (int i = 0; i < num_devices; i++) {
        if (strcmp(devices[i].name, name) == 0) {
            return &devices[i];
        }
    }
    return NULL;
}

// 块设备文件操作函数
static size_t block_dev_read(Inode* inode, void* buffer, size_t size, size_t offset) {
    typedef struct DeviceInfo {
        DeviceNode* device;
        FileOperations ops;
    } DeviceInfo;
    
    DeviceInfo* device_info = (DeviceInfo*)inode->private_data;
    DeviceNode* device = device_info->device;
    BlockDevice* block_dev = (BlockDevice*)device->device_data;
    
    if (!block_dev || !block_dev->read) {
        kernel_printf("Block device not initialized\n");
        return 0;
    }
    
    // 计算起始扇区和扇区数量
    uint32_t start_sector = offset / BLOCK_SIZE;
    uint32_t num_sectors = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t buffer_offset = offset % BLOCK_SIZE;
    
    if (start_sector + num_sectors > block_dev->block_count) {
        num_sectors = block_dev->block_count - start_sector;
        if (num_sectors == 0) {
            return 0;
        }
    }
    
    // 读取数据
    uint8_t sector_buffer[BLOCK_SIZE];
    size_t bytes_read = 0;
    
    for (uint32_t i = 0; i < num_sectors; i++) {
        block_dev->read(start_sector + i, sector_buffer);
        
        uint32_t copy_size = BLOCK_SIZE;
        if (i == 0) {
            copy_size -= buffer_offset;
        }
        if (i == num_sectors - 1) {
            copy_size = size - bytes_read;
            if (copy_size > BLOCK_SIZE) {
                copy_size = BLOCK_SIZE;
            }
        }
        
        memcpy(buffer + bytes_read, sector_buffer + (i == 0 ? buffer_offset : 0), copy_size);
        bytes_read += copy_size;
        
        if (bytes_read >= size) {
            break;
        }
    }
    
    return bytes_read;
}

static size_t block_dev_write(Inode* inode, const void* buffer, size_t size, size_t offset) {
    typedef struct DeviceInfo {
        DeviceNode* device;
        FileOperations ops;
    } DeviceInfo;
    
    DeviceInfo* device_info = (DeviceInfo*)inode->private_data;
    DeviceNode* device = device_info->device;
    BlockDevice* block_dev = (BlockDevice*)device->device_data;
    
    if (!block_dev || !block_dev->write) {
        kernel_printf("Block device not initialized\n");
        return 0;
    }
    
    // 计算起始扇区和扇区数量
    uint32_t start_sector = offset / BLOCK_SIZE;
    uint32_t num_sectors = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint32_t buffer_offset = offset % BLOCK_SIZE;
    
    if (start_sector + num_sectors > block_dev->block_count) {
        num_sectors = block_dev->block_count - start_sector;
        if (num_sectors == 0) {
            return 0;
        }
    }
    
    // 写入数据
    uint8_t sector_buffer[BLOCK_SIZE];
    size_t bytes_written = 0;
    
    for (uint32_t i = 0; i < num_sectors; i++) {
        if (i == 0 || i == num_sectors - 1) {
            // 部分扇区写入，需要先读取原有内容
            // 读取当前扇区内容
            block_dev->read(start_sector + i, sector_buffer);
        }
        
        uint32_t copy_size = BLOCK_SIZE;
        if (i == 0) {
            copy_size -= buffer_offset;
        }
        if (i == num_sectors - 1) {
            copy_size = size - bytes_written;
            if (copy_size > BLOCK_SIZE) {
                copy_size = BLOCK_SIZE;
            }
        }
        
        memcpy(sector_buffer + (i == 0 ? buffer_offset : 0), buffer + bytes_written, copy_size);
        
        block_dev->write(start_sector + i, sector_buffer);
        
        bytes_written += copy_size;
        
        if (bytes_written >= size) {
            break;
        }
    }
    
    return bytes_written;
}

static int block_dev_close(Inode* inode) {
    typedef struct DeviceInfo {
        DeviceNode* device;
        FileOperations ops;
    } DeviceInfo;
    
    if (inode) {
        // 释放DeviceInfo结构体
        DeviceInfo* device_info = (DeviceInfo*)inode->private_data;
        if (device_info) {
            free(device_info);
        }
        
        // 释放inode资源
        free(inode);
    }
    return 0;
}

// 字符设备文件操作函数
static size_t char_dev_read(Inode* inode, void* buffer, size_t size, size_t offset) {
    typedef struct DeviceInfo {
        DeviceNode* device;
        FileOperations ops;
    } DeviceInfo;
    
    DeviceInfo* device_info = (DeviceInfo*)inode->private_data;
    DeviceNode* device = device_info->device;
    
    // 对于键盘，我们从键盘缓冲区读取数据
    if (device->minor == 0) { // 假设键盘是字符设备的minor 0
        KeyboardDriver* keyboard_driver = (KeyboardDriver*)device->device_data;
        if (!keyboard_driver) {
            kernel_printf("Keyboard driver not initialized\n");
            return 0;
        }
        
        size_t bytes_read = 0;
        while (bytes_read < size) {
            char c = keyboard_getchar(keyboard_driver);
            if (c == 0) {
                // 缓冲区为空
                break;
            }
            
            *((char*)buffer + bytes_read) = c;
            bytes_read++;
        }
        
        return bytes_read;
    }
    
    return 0; // 暂不支持其他字符设备
}

static size_t char_dev_write(Inode* inode, const void* buffer, size_t size, size_t offset) {
    typedef struct DeviceInfo {
        DeviceNode* device;
        FileOperations ops;
    } DeviceInfo;
    
    DeviceInfo* device_info = (DeviceInfo*)inode->private_data;
    DeviceNode* device = device_info->device;
    
    // 字符设备通常是只读的（如键盘），所以暂不实现写入功能
    kernel_printf("Write operation not supported on character device %s\n", device->name);
    return 0;
}

static int char_dev_close(Inode* inode) {
    typedef struct DeviceInfo {
        DeviceNode* device;
        FileOperations ops;
    } DeviceInfo;
    
    if (inode) {
        // 释放DeviceInfo结构体
        DeviceInfo* device_info = (DeviceInfo*)inode->private_data;
        if (device_info) {
            free(device_info);
        }
        
        // 释放inode资源
        free(inode);
    }
    return 0;
}

// 设备文件系统操作函数
static Inode* devfs_get_inode(const char* path) {
    // 跳过路径前的斜杠
    if (path[0] == '/') {
        path++;
    }
    
    // 查找设备
    DeviceNode* device = find_device_by_name(path);
    if (!device) {
        kernel_printf("Device '%s' not found\n", path);
        return NULL;
    }
    
    // 创建inode
    Inode* inode = vfs_create_inode(
        device->type == DEV_TYPE_BLOCK ? FILE_TYPE_BLOCK_DEVICE : FILE_TYPE_CHAR_DEVICE,
        0666, // 读写权限
        device
    );
    
    if (!inode) {
        kernel_printf("Failed to create inode for device '%s'\n", path);
        return NULL;
    }
    
    // 设置inode大小（块设备的总字节数）
    if (device->type == DEV_TYPE_BLOCK) {
        BlockDevice* block_dev = (BlockDevice*)device->device_data;
        inode->size = block_dev->block_count * BLOCK_SIZE;
        inode->blocks = block_dev->block_count;
    }
    
    // 根据设备类型设置文件操作函数
    FileOperations* ops = (FileOperations*)malloc(sizeof(FileOperations));
    if (ops) {
        if (device->type == DEV_TYPE_BLOCK) {
            ops->read = block_dev_read;
            ops->write = block_dev_write;
            ops->close = block_dev_close;
            ops->ioctl = NULL; // 暂不支持ioctl
        } else if (device->type == DEV_TYPE_CHAR) {
            ops->read = char_dev_read;
            ops->write = char_dev_write;
            ops->close = char_dev_close;
            ops->ioctl = NULL; // 暂不支持ioctl
        } else {
            free(ops);
            ops = NULL;
        }
        
        // 创建一个设备信息结构体，包含设备指针和文件操作函数
        typedef struct DeviceInfo {
            DeviceNode* device;
            FileOperations ops;
        } DeviceInfo;
        
        DeviceInfo* device_info = (DeviceInfo*)malloc(sizeof(DeviceInfo));
        if (device_info) {
            device_info->device = device;
            device_info->ops = *ops;
            inode->private_data = device_info;
        }
        
        free(ops);
    } else {
        // 如果无法分配文件操作函数，至少保留设备指针
        inode->private_data = device;
    }
    
    return inode;
}

static int devfs_mount_impl(const char* source, const char* target) {
    kernel_printf("devfs mounted at %s\n", target);
    return 0;
}

static int devfs_umount_impl(const char* target) {
    kernel_printf("devfs unmounted from %s\n", target);
    return 0;
}

// 初始化设备文件系统
extern int devfs_init() {
    // 初始化设备列表
    for (int i = 0; i < MAX_DEVICES; i++) {
        devices[i].name[0] = '\0';
        devices[i].type = DEV_TYPE_BLOCK;
        devices[i].major = 0;
        devices[i].minor = 0;
        devices[i].device_data = NULL;
    }
    
    num_devices = 0;
    
    // 初始化文件系统接口
    strcpy(devfs_fs.name, "devfs");
    devfs_fs.get_inode = devfs_get_inode;
    devfs_fs.mount = devfs_mount_impl;
    devfs_fs.umount = devfs_umount_impl;
    
    kernel_printf("Device filesystem (devfs) initialized\n");
    return 0;
}

// 挂载设备文件系统
extern int devfs_mount(const char* mount_point) {
    // 注册文件系统
    if (vfs_register_filesystem(&devfs_fs) != 0) {
        kernel_printf("Failed to register devfs\n");
        return -1;
    }
    
    // 挂载文件系统
    if (vfs_mount("none", mount_point, "devfs") != 0) {
        kernel_printf("Failed to mount devfs at %s\n", mount_point);
        return -1;
    }
    
    devfs_mount_point = (MountPoint*)malloc(sizeof(MountPoint));
    if (devfs_mount_point) {
        devfs_mount_point->path = strdup(mount_point);
        devfs_mount_point->fs = &devfs_fs;
        devfs_mount_point->next = NULL;
    }
    
    kernel_printf("devfs successfully mounted at %s\n", mount_point);
    return 0;
}

// 卸载设备文件系统
extern int devfs_umount() {
    if (!devfs_mount_point) {
        kernel_printf("devfs not mounted\n");
        return -1;
    }
    
    int result = vfs_umount(devfs_mount_point->path);
    if (result == 0) {
        free(devfs_mount_point->path);
        free(devfs_mount_point);
        devfs_mount_point = NULL;
        kernel_printf("devfs successfully unmounted\n");
    }
    
    return result;
}

// 注册设备
extern int devfs_register_device(const char* name, DeviceType type, uint32_t major, uint32_t minor, void* device_data) {
    if (num_devices >= MAX_DEVICES) {
        kernel_printf("Maximum number of devices reached\n");
        return -1;
    }
    
    if (find_device_by_name(name)) {
        kernel_printf("Device '%s' already exists\n", name);
        return -1;
    }
    
    strncpy(devices[num_devices].name, name, sizeof(devices[num_devices].name) - 1);
    devices[num_devices].type = type;
    devices[num_devices].major = major;
    devices[num_devices].minor = minor;
    devices[num_devices].device_data = device_data;
    
    num_devices++;
    kernel_printf("Device '%s' registered as type %d, major=%d, minor=%d\n",
                 name, type, major, minor);
    return 0;
}

// 注销设备
extern int devfs_unregister_device(const char* name) {
    DeviceNode* device = find_device_by_name(name);
    if (!device) {
        kernel_printf("Device '%s' not found\n", name);
        return -1;
    }
    
    // 计算设备在数组中的索引
    int index = device - devices;
    
    // 移动后续设备
    for (int i = index; i < num_devices - 1; i++) {
        devices[i] = devices[i + 1];
    }
    
    // 清空最后一个设备的信息
    devices[num_devices - 1].name[0] = '\0';
    devices[num_devices - 1].type = DEV_TYPE_BLOCK;
    devices[num_devices - 1].major = 0;
    devices[num_devices - 1].minor = 0;
    devices[num_devices - 1].device_data = NULL;
    
    num_devices--;
    kernel_printf("Device '%s' unregistered\n", name);
    return 0;
}

// 获取devfs文件系统接口
extern FileSystem* devfs_get_filesystem() {
    return &devfs_fs;
}