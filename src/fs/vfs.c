#include <fs/vfs.h>
#include <kernel/memory/malloc.h>
#include <kernel/kerio.h>

#define MAX_FILE_DESCRIPTORS 1024
#define MAX_MOUNT_POINTS 32
#define MAX_FILESYSTEMS 16

static FileDescriptor file_descriptors[MAX_FILE_DESCRIPTORS];
static MountPoint mount_points[MAX_MOUNT_POINTS];
static FileSystem* registered_filesystems[MAX_FILESYSTEMS];
static uint32_t next_inode_num = 1;
static uint32_t num_mount_points = 0;
static uint32_t num_filesystems = 0;
static uint32_t next_fd = 0;

// 初始化VFS
extern int vfs_init() {
    // 初始化文件描述符表
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        file_descriptors[i].ref_count = 0;
        file_descriptors[i].inode = NULL;
        file_descriptors[i].ops = NULL;
        file_descriptors[i].offset = 0;
        file_descriptors[i].flags = 0;
    }
    
    // 初始化挂载点
    for (int i = 0; i < MAX_MOUNT_POINTS; i++) {
        mount_points[i].path = NULL;
        mount_points[i].fs = NULL;
        mount_points[i].next = NULL;
    }
    
    // 初始化文件系统注册表
    for (int i = 0; i < MAX_FILESYSTEMS; i++) {
        registered_filesystems[i] = NULL;
    }
    
    kernel_printf("VFS initialized successfully\n");
    return 0;
}

// 创建inode
extern Inode* vfs_create_inode(FileType type, uint32_t permissions, void* private_data) {
    Inode* inode = (Inode*)malloc(sizeof(Inode));
    if (!inode) {
        return NULL;
    }
    
    inode->inode_num = next_inode_num++;
    inode->type = type;
    inode->permissions = permissions;
    inode->size = 0;
    inode->blocks = 0;
    inode->ref_count = 1;
    inode->private_data = private_data;
    
    return inode;
}

// 销毁inode
extern void vfs_destroy_inode(Inode* inode) {
    if (inode && inode->ref_count > 0) {
        inode->ref_count--;
        if (inode->ref_count == 0) {
            free(inode);
        }
    }
}

// 注册文件系统
extern int vfs_register_filesystem(FileSystem* fs) {
    if (num_filesystems >= MAX_FILESYSTEMS) {
        kernel_printf("Maximum number of file systems reached\n");
        return -1;
    }
    
    for (int i = 0; i < num_filesystems; i++) {
        if (strcmp(registered_filesystems[i]->name, fs->name) == 0) {
            kernel_printf("File system already registered\n");
            return -1;
        }
    }
    
    registered_filesystems[num_filesystems++] = fs;
    kernel_printf("File system '%s' registered\n", fs->name);
    return 0;
}

// 查找文件系统
static FileSystem* find_filesystem(const char* fs_type) {
    for (int i = 0; i < num_filesystems; i++) {
        if (strcmp(registered_filesystems[i]->name, fs_type) == 0) {
            return registered_filesystems[i];
        }
    }
    return NULL;
}

// 挂载文件系统
extern int vfs_mount(const char* source, const char* target, const char* fs_type) {
    if (num_mount_points >= MAX_MOUNT_POINTS) {
        kernel_printf("Maximum number of mount points reached\n");
        return -1;
    }
    
    // 检查挂载点是否已存在
    for (int i = 0; i < num_mount_points; i++) {
        if (strcmp(mount_points[i].path, target) == 0) {
            kernel_printf("Mount point already exists\n");
            return -1;
        }
    }
    
    // 查找文件系统
    FileSystem* fs = find_filesystem(fs_type);
    if (!fs) {
        kernel_printf("File system '%s' not found\n", fs_type);
        return -1;
    }
    
    // 调用文件系统的挂载函数
    if (fs->mount(source, target) != 0) {
        kernel_printf("Failed to mount file system\n");
        return -1;
    }
    
    // 保存挂载点信息
    mount_points[num_mount_points].path = strdup(target);
    mount_points[num_mount_points].fs = fs;
    mount_points[num_mount_points].next = NULL;
    num_mount_points++;
    
    kernel_printf("Mounted %s on %s\n", source, target);
    return 0;
}

// 卸载文件系统
extern int vfs_umount(const char* target) {
    for (int i = 0; i < num_mount_points; i++) {
        if (strcmp(mount_points[i].path, target) == 0) {
            // 调用文件系统的卸载函数
            if (mount_points[i].fs->umount(target) != 0) {
                kernel_printf("Failed to unmount file system\n");
                return -1;
            }
            
            // 释放挂载点路径
            free(mount_points[i].path);
            
            // 移动后续挂载点
            for (int j = i; j < num_mount_points - 1; j++) {
                mount_points[j] = mount_points[j + 1];
            }
            
            num_mount_points--;
            kernel_printf("Unmounted %s\n", target);
            return 0;
        }
    }
    
    kernel_printf("Mount point not found\n");
    return -1;
}

// 分配文件描述符
static int allocate_file_descriptor(Inode* inode, FileOperations* ops, int flags) {
    for (int i = 0; i < MAX_FILE_DESCRIPTORS; i++) {
        if (file_descriptors[i].ref_count == 0) {
            file_descriptors[i].inode = inode;
            file_descriptors[i].ops = ops;
            file_descriptors[i].offset = 0;
            file_descriptors[i].flags = flags;
            file_descriptors[i].ref_count = 1;
            return i;
        }
    }
    return -1; // 没有可用的文件描述符
}

// 打开文件
extern int vfs_open(const char* path, int flags) {
    // 简单实现：尝试在第一个挂载点查找文件
    if (num_mount_points > 0) {
        Inode* inode = mount_points[0].fs->get_inode(path);
        if (inode) {
            // 为简化，使用inode中的权限作为文件操作接口
            int fd = allocate_file_descriptor(inode, (FileOperations*)inode->private_data, flags);
            if (fd >= 0) {
                kernel_printf("File '%s' opened with fd %d\n", path, fd);
                return fd;
            }
        }
    }
    
    kernel_printf("Failed to open file '%s'\n", path);
    return -1;
}

// 关闭文件
extern int vfs_close(int fd) {
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTORS || file_descriptors[fd].ref_count == 0) {
        kernel_printf("Invalid file descriptor\n");
        return -1;
    }
    
    file_descriptors[fd].ref_count--;
    if (file_descriptors[fd].ref_count == 0) {
        if (file_descriptors[fd].inode && file_descriptors[fd].ops && file_descriptors[fd].ops->close) {
            file_descriptors[fd].ops->close(file_descriptors[fd].inode);
        }
        vfs_destroy_inode(file_descriptors[fd].inode);
        file_descriptors[fd].inode = NULL;
        file_descriptors[fd].ops = NULL;
    }
    
    kernel_printf("File descriptor %d closed\n", fd);
    return 0;
}

// 读取文件
extern size_t vfs_read(int fd, void* buffer, size_t size) {
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTORS || file_descriptors[fd].ref_count == 0 || !buffer) {
        kernel_printf("Invalid file descriptor or buffer\n");
        return 0;
    }
    
    FileDescriptor* desc = &file_descriptors[fd];
    if (!desc->ops || !desc->ops->read) {
        kernel_printf("Read operation not supported\n");
        return 0;
    }
    
    size_t bytes_read = desc->ops->read(desc->inode, buffer, size, desc->offset);
    if (bytes_read > 0) {
        desc->offset += bytes_read;
    }
    
    return bytes_read;
}

// 写入文件
extern size_t vfs_write(int fd, const void* buffer, size_t size) {
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTORS || file_descriptors[fd].ref_count == 0 || !buffer) {
        kernel_printf("Invalid file descriptor or buffer\n");
        return 0;
    }
    
    FileDescriptor* desc = &file_descriptors[fd];
    if (!desc->ops || !desc->ops->write) {
        kernel_printf("Write operation not supported\n");
        return 0;
    }
    
    // 检查是否以追加模式打开
    size_t write_offset = desc->offset;
    if (desc->flags & O_APPEND && desc->inode) {
        write_offset = desc->inode->size;
    }
    
    size_t bytes_written = desc->ops->write(desc->inode, buffer, size, write_offset);
    if (bytes_written > 0) {
        desc->offset = write_offset + bytes_written;
        if (desc->inode && desc->offset > desc->inode->size) {
            desc->inode->size = desc->offset;
        }
    }
    
    return bytes_written;
}

// IO控制
extern int vfs_ioctl(int fd, int request, void* argp) {
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTORS || file_descriptors[fd].ref_count == 0) {
        kernel_printf("Invalid file descriptor\n");
        return -1;
    }
    
    FileDescriptor* desc = &file_descriptors[fd];
    if (!desc->ops || !desc->ops->ioctl) {
        kernel_printf("IOCTL operation not supported\n");
        return -1;
    }
    
    return desc->ops->ioctl(desc->inode, request, argp);
}