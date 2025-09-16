#ifndef DEVFS_H
#define DEVFS_H

#include <fs/vfs.h>
#include <driver/block.h>

// 设备类型枚举
typedef enum {
    DEV_TYPE_BLOCK,
    DEV_TYPE_CHAR,
    DEV_TYPE_NET
} DeviceType;

// 设备节点结构体
typedef struct {
    char name[32];
    DeviceType type;
    uint32_t major;
    uint32_t minor;
    void* device_data;
} DeviceNode;

// 设备文件系统操作接口
extern int devfs_init();
extern int devfs_mount(const char* mount_point);
extern int devfs_umount();
extern int devfs_register_device(const char* name, DeviceType type, uint32_t major, uint32_t minor, void* device_data);
extern int devfs_unregister_device(const char* name);
extern FileSystem* devfs_get_filesystem();

#endif // DEVFS_H