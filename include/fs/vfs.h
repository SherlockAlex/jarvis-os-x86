#ifndef OS_FS_VFS
#define OS_FS_VFS

#include <stdtype.h>
#include <kernel/string.h>

// 文件类型枚举
typedef enum {
    FILE_TYPE_REGULAR,     // 普通文件
    FILE_TYPE_DIRECTORY,   // 目录
    FILE_TYPE_BLOCK_DEVICE,// 块设备
    FILE_TYPE_CHAR_DEVICE, // 字符设备
    FILE_TYPE_PIPE,        // 管道
    FILE_TYPE_SYMLINK      // 符号链接
} FileType;

// 文件访问模式
#define O_RDONLY    0x0001
#define O_WRONLY    0x0002
#define O_RDWR      0x0003
#define O_CREAT     0x0004
#define O_TRUNC     0x0008
#define O_APPEND    0x0010

// inode 结构
typedef struct {
    uint32_t inode_num;     // inode编号
    FileType type;          // 文件类型
    uint32_t permissions;   // 权限
    uint32_t size;          // 文件大小
    uint32_t blocks;        // 占用块数
    uint32_t ref_count;     // 引用计数
    void* private_data;     // 具体文件系统或设备的私有数据
} Inode;

// 文件操作接口
typedef struct {
    int (*open)(Inode* inode, int flags);
    int (*close)(Inode* inode);
    size_t (*read)(Inode* inode, void* buffer, size_t size, size_t offset);
    size_t (*write)(Inode* inode, const void* buffer, size_t size, size_t offset);
    int (*ioctl)(Inode* inode, int request, void* argp);
} FileOperations;

// 文件描述符结构
typedef struct {
    Inode* inode;
    FileOperations* ops;
    size_t offset;
    int flags;
    uint32_t ref_count;
} FileDescriptor;

// 文件系统抽象
typedef struct {
    char* name;             // 文件系统名称
    int (*mount)(const char* source, const char* target);
    int (*umount)(const char* target);
    Inode* (*get_inode)(const char* path);
} FileSystem;

// 挂载点结构
typedef struct MountPoint {
    char* path;
    FileSystem* fs;
    struct MountPoint* next;
} MountPoint;

// 全局VFS函数
extern int vfs_init();
extern int vfs_mount(const char* source, const char* target, const char* fs_type);
extern int vfs_umount(const char* target);
extern int vfs_open(const char* path, int flags);
extern int vfs_close(int fd);
extern size_t vfs_read(int fd, void* buffer, size_t size);
extern size_t vfs_write(int fd, const void* buffer, size_t size);
extern int vfs_ioctl(int fd, int request, void* argp);

extern Inode* vfs_create_inode(FileType type, uint32_t permissions, void* private_data);
extern void vfs_destroy_inode(Inode* inode);
extern int vfs_register_filesystem(FileSystem* fs);

#endif