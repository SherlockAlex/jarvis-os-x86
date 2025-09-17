#include <fs/vfs.h>
#include <kernel/memory/malloc.h>
#include <kernel/kerio.h>
#include <kernel/string.h>

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

// 查找挂载点
MountPoint* vfs_find_mount_point(const char* path, char** path_in_fs) {
    if (!path || !path_in_fs) {
        return NULL;
    }
    
    // 规范化路径
    char* normalized_path = vfs_normalize_path(path, "/");
    if (!normalized_path) {
        return NULL;
    }
    
    MountPoint* best_match = NULL;
    size_t best_match_len = 0;
    
    // 查找最长匹配的挂载点
    for (int i = 0; i < num_mount_points; i++) {
        size_t mount_len = strlen(mount_points[i].path);
        
        // 确保挂载点路径以'/'结尾
        char mount_path[256];
        strncpy(mount_path, mount_points[i].path, sizeof(mount_path) - 1);
        if (mount_path[mount_len - 1] != '/') {
            mount_path[mount_len] = '/';
            mount_path[mount_len + 1] = '\0';
            mount_len++;
        }
        
        // 检查路径是否以挂载点路径开头
        if (strncmp(normalized_path, mount_path, mount_len) == 0) {
            if (mount_len > best_match_len) {
                best_match = &mount_points[i];
                best_match_len = mount_len;
            }
        }
    }
    
    // 如果没有找到挂载点，默认使用第一个挂载点（如果存在）
    if (!best_match && num_mount_points > 0) {
        best_match = &mount_points[0];
        best_match_len = strlen(best_match->path);
    }
    
    // 计算相对路径
    if (best_match) {
        size_t path_len = strlen(normalized_path);
        if (best_match_len < path_len) {
            *path_in_fs = strdup(normalized_path + best_match_len);
        } else {
            *path_in_fs = strdup("/");
        }
    } else {
        *path_in_fs = NULL;
    }
    
    free(normalized_path);
    return best_match;
}

// 解析路径
extern Inode* vfs_resolve_path(const char* path) {
    if (!path) {
        return NULL;
    }
    
    char* path_in_fs;
    MountPoint* mp = vfs_find_mount_point(path, &path_in_fs);
    
    if (!mp || !path_in_fs) {
        return NULL;
    }
    
    // 调用文件系统的get_inode函数
    Inode* inode = mp->fs->get_inode(path_in_fs);
    
    free(path_in_fs);
    return inode;
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

// 规范化路径
char* vfs_normalize_path(const char* path, const char* current_dir) {
    // 如果是绝对路径，直接使用
    if (path[0] == '/') {
        return strdup(path);
    }
    
    // 如果是相对路径，结合当前目录
    size_t curr_len = strlen(current_dir);
    size_t path_len = strlen(path);
    size_t new_len = curr_len + path_len + 2; // +2 for '/' and null terminator
    
    char* normalized_path = (char*)malloc(new_len);
    if (!normalized_path) {
        return NULL;
    }
    
    strcpy(normalized_path, current_dir);
    
    // 如果当前目录不以'/'结尾，添加'/'并拼接路径
    if (curr_len > 0 && current_dir[curr_len - 1] != '/') {
        snprintf(normalized_path + curr_len, new_len - curr_len, "/%s", path);
    } else {
        snprintf(normalized_path + curr_len, new_len - curr_len, "%s", path);
    }
    
    return normalized_path;
}

// 解析路径为组件
char** vfs_parse_path(const char* path, int* num_components) {
    if (!path || !num_components) {
        return NULL;
    }
    
    // 计算路径组件数量
    int count = 0;
    const char* p = path;
    
    // 跳过开头的'/'
    if (*p == '/') {
        p++;
    }
    
    // 计算组件数量
    while (*p) {
        count++;
        // 找到下一个'/'
        while (*p && *p != '/') {
            p++;
        }
        // 跳过连续的'/'
        while (*p && *p == '/') {
            p++;
        }
    }
    
    // 分配组件数组
    char** components = (char**)malloc(sizeof(char*) * (count + 1)); // +1 for NULL terminator
    if (!components) {
        *num_components = 0;
        return NULL;
    }
    
    // 分割路径
    int i = 0;
    p = path;
    
    // 跳过开头的'/'
    if (*p == '/') {
        p++;
    }
    
    while (*p) {
        const char* start = p;
        // 找到组件结尾
        while (*p && *p != '/') {
            p++;
        }
        
        // 复制组件
        size_t len = p - start;
        components[i] = (char*)malloc(len + 1);
        if (!components[i]) {
            // 释放已分配的内存
            vfs_free_path_components(components, i);
            *num_components = 0;
            return NULL;
        }
        
        strncpy(components[i], start, len);
        components[i][len] = '\0';
        i++;
        
        // 跳过连续的'/'
        while (*p && *p == '/') {
            p++;
        }
    }
    
    components[i] = NULL;
    *num_components = count;
    return components;
}

// 释放路径组件
void vfs_free_path_components(char** components, int num_components) {
    if (!components) {
        return;
    }
    
    for (int i = 0; i < num_components; i++) {
        if (components[i]) {
            free(components[i]);
        }
    }
    
    free(components);
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
    // 解析路径获取inode
    Inode* inode = vfs_resolve_path(path);
    if (!inode) {
        kernel_printf("Failed to open file '%s': No such file or directory\n", path);
        return -1;
    }
    
    // 获取文件操作接口
    FileOperations* ops = (FileOperations*)inode->private_data;
    if (!ops) {
        kernel_printf("Failed to open file '%s': No file operations available\n", path);
        vfs_destroy_inode(inode);
        return -1;
    }
    
    // 调用文件系统的open函数
    if (ops->open && ops->open(inode, flags) != 0) {
        kernel_printf("Failed to open file '%s': Operation failed\n", path);
        vfs_destroy_inode(inode);
        return -1;
    }
    
    // 分配文件描述符
    int fd = allocate_file_descriptor(inode, ops, flags);
    if (fd < 0) {
        kernel_printf("Failed to open file '%s': No file descriptors available\n", path);
        if (ops->close) {
            ops->close(inode);
        }
        vfs_destroy_inode(inode);
        return -1;
    }
    
    kernel_printf("File '%s' opened with fd %d\n", path, fd);
    return fd;
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

// 打开目录
extern int vfs_opendir(const char* path) {
    // 解析路径获取inode
    Inode* inode = vfs_resolve_path(path);
    if (!inode) {
        kernel_printf("Failed to open directory '%s': No such directory\n", path);
        return -1;
    }
    
    // 检查是否为目录
    if (inode->type != FILE_TYPE_DIRECTORY) {
        kernel_printf("Failed to open directory '%s': Not a directory\n", path);
        vfs_destroy_inode(inode);
        return -1;
    }
    
    // 获取文件操作接口
    FileOperations* ops = (FileOperations*)inode->private_data;
    if (!ops) {
        kernel_printf("Failed to open directory '%s': No directory operations available\n", path);
        vfs_destroy_inode(inode);
        return -1;
    }
    
    // 调用文件系统的opendir函数
    if (ops->opendir && ops->opendir(inode) != 0) {
        kernel_printf("Failed to open directory '%s': Operation failed\n", path);
        vfs_destroy_inode(inode);
        return -1;
    }
    
    // 分配文件描述符
    int fd = allocate_file_descriptor(inode, ops, O_RDONLY);
    if (fd < 0) {
        kernel_printf("Failed to open directory '%s': No file descriptors available\n", path);
        if (ops->closedir) {
            ops->closedir(inode);
        }
        vfs_destroy_inode(inode);
        return -1;
    }
    
    return fd;
}

// 关闭目录
extern int vfs_closedir(int fd) {
    return vfs_close(fd);
}

// 读取目录
extern int vfs_readdir(int fd, DirectoryEntry* entry) {
    if (fd < 0 || fd >= MAX_FILE_DESCRIPTORS || file_descriptors[fd].ref_count == 0 || !entry) {
        kernel_printf("Invalid file descriptor or entry\n");
        return -1;
    }
    
    FileDescriptor* desc = &file_descriptors[fd];
    if (!desc->ops || !desc->ops->readdir || desc->inode->type != FILE_TYPE_DIRECTORY) {
        kernel_printf("Not a directory or readdir not supported\n");
        return -1;
    }
    
    // 调用文件系统的readdir函数
    if (desc->ops->readdir(desc->inode, entry->name, sizeof(entry->name), &entry->type) != 0) {
        return -1;
    }
    
    return 0;
}

// 创建目录
extern int vfs_mkdir(const char* path, uint32_t permissions) {
    // 检查参数
    if (!path || strlen(path) == 0) {
        kernel_printf("Invalid path\n");
        return -1;
    }
    
    char* path_in_fs;
    MountPoint* mp = vfs_find_mount_point(path, &path_in_fs);
    
    if (!mp || !path_in_fs) {
        kernel_printf("No suitable file system found for path '%s'\n", path);
        return -1;
    }
    
    // 检查文件系统是否支持创建目录
    if (!mp->fs->mkdir) {
        kernel_printf("File system does not support directory creation\n");
        free(path_in_fs);
        return -1;
    }
    
    // 调用具体文件系统的mkdir实现
    int result = mp->fs->mkdir(path_in_fs, permissions);
    free(path_in_fs);
    
    if (result != 0) {
        kernel_printf("Failed to create directory '%s'\n", path);
        return result;
    }
    
    kernel_printf("Directory '%s' created successfully\n", path);
    return 0;
}

// 删除目录
extern int vfs_rmdir(const char* path) {
    // 检查参数
    if (!path || strlen(path) == 0) {
        kernel_printf("Invalid path\n");
        return -1;
    }
    
    char* path_in_fs;
    MountPoint* mp = vfs_find_mount_point(path, &path_in_fs);
    
    if (!mp || !path_in_fs) {
        kernel_printf("No suitable file system found for path '%s'\n", path);
        return -1;
    }
    
    // 检查文件系统是否支持删除目录
    if (!mp->fs->rmdir) {
        kernel_printf("File system does not support directory deletion\n");
        free(path_in_fs);
        return -1;
    }
    
    // 调用具体文件系统的rmdir实现
    int result = mp->fs->rmdir(path_in_fs);
    free(path_in_fs);
    
    if (result != 0) {
        kernel_printf("Failed to delete directory '%s'\n", path);
        return result;
    }
    
    kernel_printf("Directory '%s' deleted successfully\n", path);
    return 0;
}

// 删除文件
extern int vfs_remove(const char* path) {
    // 检查参数
    if (!path || strlen(path) == 0) {
        kernel_printf("Invalid path\n");
        return -1;
    }
    
    char* path_in_fs;
    MountPoint* mp = vfs_find_mount_point(path, &path_in_fs);
    
    if (!mp || !path_in_fs) {
        kernel_printf("No suitable file system found for path '%s'\n", path);
        return -1;
    }
    
    // 检查文件系统是否支持删除文件
    if (!mp->fs->remove) {
        kernel_printf("File system does not support file deletion\n");
        free(path_in_fs);
        return -1;
    }
    
    // 调用具体文件系统的remove实现
    int result = mp->fs->remove(path_in_fs);
    free(path_in_fs);
    
    if (result != 0) {
        kernel_printf("Failed to delete file '%s'\n", path);
        return result;
    }
    
    kernel_printf("File '%s' deleted successfully\n", path);
    return 0;
}