#include "../include/stdtype.h"
#include "../include/fs/ext4.h"
#include "../include/fs/vfs.h"
#include "../include/kernel/string.h"
#include "../include/kernel/memory/malloc.h"
#include "../include/kernel/kerio.h"

// 定义缺失的常量
#define PATH_MAX 256
#define S_IFDIR (1 << 14) // 目录类型位掩码
#define MAX_DIR_BUFFER_SIZE 512 // 最大目录缓冲区大小，避免分配过大内存

// Directory entry structure for EXT4
typedef struct {
    uint32_t inode;             // Inode number
    uint16_t rec_len;           // Entry length
    uint8_t name_len;           // Name length
    uint8_t file_type;          // File type
    char name[];                // File name (variable length)
} __attribute__((packed)) Ext4DirEntry;

// Directory iterator structure to keep track of current position
typedef struct {
    uint32_t current_block;     // Current block being read
    uint32_t current_offset;    // Current offset within block
    uint8_t* block_buffer;      // Buffer for current block
} Ext4DirIterator;

static FileSystem ext4_filesystem;
static Ext4FileSystemData* ext4_fs_data = NULL;

// 函数声明
uint32_t current_time();
uint32_t ext4_allocate_inode();
void ext4_free_inode(uint32_t inode_num);
uint32_t ext4_allocate_block();
void ext4_free_block(uint32_t block_num);
void ext4_write_inode(Ext4FileSystemData* fs_data, uint32_t inode_num, Ext4Inode* inode);
void ext4_write_super_block(Ext4FileSystemData* fs_data);
int ext4_add_dir_entry(Inode* dir_inode, const char* name, uint32_t inode_num, FileType type);

// 读取超级块
void ext4_read_super_block(Ext4FileSystemData* fs_data) {
    uint8_t buffer[512];
    
    // 读取包含超级块的扇区
    fs_data->device->read(EXT4_SUPER_BLOCK_OFFSET / BLOCK_SIZE, buffer);
    
    // 复制超级块数据
    memcpy(&fs_data->super_block, buffer, sizeof(Ext4SuperBlock));
    
    // 验证魔数
    if (fs_data->super_block.magic != EXT4_SUPER_MAGIC) {
        kernel_printf("Warning: Not an EXT4 file system (magic: 0x%x)\n", fs_data->super_block.magic);
    }
    
    // 计算块大小
    fs_data->block_size = EXT4_MIN_BLOCK_SIZE << fs_data->super_block.log_block_size;
    
    // 设置块组参数
    fs_data->blocks_per_group = fs_data->super_block.blocks_per_group;
    fs_data->inodes_per_group = fs_data->super_block.inodes_per_group;
    
    // 计算块组数量（使用32位除法）
    // 为了简化，只使用低32位的块数量
    fs_data->block_group_count = (fs_data->super_block.blocks_count_lo + fs_data->blocks_per_group - 1) / fs_data->blocks_per_group;
}

// 读取块
void ext4_read_block(Ext4FileSystemData* fs_data, uint32_t block_num, void* buffer) {
    // 计算块对应的扇区
    uint32_t sector = (block_num * fs_data->block_size) / BLOCK_SIZE;
    
    // 读取所有扇区
    for (uint32_t i = 0; i < fs_data->block_size / BLOCK_SIZE; i++) {
        fs_data->device->read(sector + i, (uint8_t*)buffer + (i * BLOCK_SIZE));
    }
}

// 写入块
void ext4_write_block(Ext4FileSystemData* fs_data, uint32_t block_num, const void* buffer) {
    // 计算块对应的扇区
    uint32_t sector = (block_num * fs_data->block_size) / BLOCK_SIZE;
    
    // 写入所有扇区
    for (uint32_t i = 0; i < fs_data->block_size / BLOCK_SIZE; i++) {
        fs_data->device->write(sector + i, (uint8_t*)buffer + (i * BLOCK_SIZE));
    }
}

// 读取索引节点
Ext4Inode* ext4_read_inode(Ext4FileSystemData* fs_data, uint32_t inode_num) {
    Ext4Inode* inode = (Ext4Inode*)malloc(sizeof(Ext4Inode));
    if (!inode) {
        return NULL;
    }
    
    // 计算索引节点所在的块组和块组内偏移
    uint32_t block_group = (inode_num - 1) / fs_data->inodes_per_group;
    uint32_t inode_index = (inode_num - 1) % fs_data->inodes_per_group;
    
    // 读取块组描述符
    uint32_t bgd_block = fs_data->super_block.first_data_block + 1 + block_group;
    Ext4BlockGroupDescriptor bgd;
    ext4_read_block(fs_data, bgd_block, &bgd);
    
    // 计算索引节点表的起始块
    uint32_t inode_table_block = bgd.bg_inode_table_lo;
    
    // 计算索引节点在表中的偏移
    uint32_t inode_offset = inode_index * fs_data->super_block.inode_size;
    uint32_t block_offset = inode_offset / fs_data->block_size;
    uint32_t in_block_offset = inode_offset % fs_data->block_size;
    
    // 读取包含索引节点的块
    // 使用固定大小的缓冲区，避免分配过大的内存块
    uint8_t* block_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
    if (!block_buffer) {
        free(inode);
        return NULL;
    }
    
    ext4_read_block(fs_data, inode_table_block + block_offset, block_buffer);
    
    // 复制索引节点数据
    memcpy(inode, block_buffer + in_block_offset, sizeof(Ext4Inode));
    
    free(block_buffer);
    return inode;
}

// 调试打印超级块信息
void ext4_debug_print_super_block(Ext4SuperBlock* sb) {
    kernel_printf("EXT4 Super Block Info:\n");
    kernel_printf("  Magic: 0x%x\n", sb->magic);
    kernel_printf("  Inodes count: %u\n", sb->inodes_count);
    kernel_printf("  Blocks count: %u\n", sb->blocks_count_lo);
    kernel_printf("  Free blocks count: %u\n", sb->free_blocks_count_lo);
    kernel_printf("  Free inodes count: %u\n", sb->free_inodes_count);
    kernel_printf("  First data block: %u\n", sb->first_data_block);
    kernel_printf("  Block size: %u\n", EXT4_MIN_BLOCK_SIZE << sb->log_block_size);
    kernel_printf("  Blocks per group: %u\n", sb->blocks_per_group);
    kernel_printf("  Inodes per group: %u\n", sb->inodes_per_group);
}

// 文件读取操作
size_t ext4_file_read(Inode* inode, void* buffer, size_t size, size_t offset) {
    if (!inode || !buffer) {
        return 0;
    }
    
    Ext4FileSystemData* fs_data = (Ext4FileSystemData*)inode->private_data;
    Ext4Inode* ext4_inode = (Ext4Inode*)inode->private_data;
    
    // 检查偏移是否超出文件大小（只使用32位部分）
    uint32_t file_size = ext4_inode->size_lo;
    if (offset >= file_size) {
        return 0;
    }
    
    // 调整读取大小，不超过文件剩余部分
    if (offset + size > file_size) {
        size = file_size - offset;
    }
    
    // 简单实现：假设使用直接块寻址
    uint32_t block_size = fs_data->block_size;
    uint32_t start_block = offset / block_size;
    uint32_t end_block = (offset + size - 1) / block_size;
    uint32_t start_offset = offset % block_size;
    
    size_t bytes_read = 0;
    // 使用固定大小的缓冲区，避免分配过大的内存块
    uint8_t* block_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
    
    if (!block_buffer) {
        return 0;
    }
    
    for (uint32_t i = start_block; i <= end_block && i < 12; i++) {
        // 检查块指针是否有效
        if (ext4_inode->block[i] == 0) {
            break;
        }
        
        // 读取块数据
        ext4_read_block(fs_data, ext4_inode->block[i], block_buffer);
        
        // 计算当前块要复制的字节数
        size_t copy_size = block_size;
        if (i == start_block) {
            copy_size -= start_offset;
        }
        if (i == end_block) {
            copy_size = size - bytes_read;
        }
        
        // 复制数据到用户缓冲区
        memcpy(buffer + bytes_read, block_buffer + (i == start_block ? start_offset : 0), copy_size);
        bytes_read += copy_size;
    }
    
    free(block_buffer);
    return bytes_read;
}

// 文件写入操作
size_t ext4_file_write(Inode* inode, const void* buffer, size_t size, size_t offset) {
    // 简化实现，仅返回0表示不支持写入
    kernel_printf("Write operation not fully implemented in EXT4\n");
    return 0;
}

// 文件关闭操作
int ext4_file_close(Inode* inode) {
    if (!inode) {
        return 0;
    }
    
    // 释放索引节点数据
    if (inode->private_data) {
        free(inode->private_data);
    }
    
    return 0;
}

// 声明获取索引节点函数，实际定义在文件末尾
Inode* ext4_get_inode(const char* path);

// 挂载文件系统
int ext4_mount(const char* source, const char* target) {
    if (!source || !target) {
        return -1;
    }
    
    // 检查是否已经挂载
    if (ext4_fs_data) {
        kernel_printf("EXT4 is already mounted\n");
        return -1;
    }
    
    // 分配文件系统数据结构
    ext4_fs_data = (Ext4FileSystemData*)malloc(sizeof(Ext4FileSystemData));
    if (!ext4_fs_data) {
        kernel_printf("Failed to allocate memory for EXT4 file system data\n");
        return -1;
    }
    
    // 初始化文件系统数据
    strncpy(ext4_fs_data->mount_point, target, sizeof(ext4_fs_data->mount_point) - 1);
    ext4_fs_data->mount_point[sizeof(ext4_fs_data->mount_point) - 1] = '\0';
    
    // 假设使用第一个块设备
    if (num_block_devices > 0) {
        ext4_fs_data->device = active_block_devices[0];
    } else {
        kernel_printf("No block devices available\n");
        free(ext4_fs_data);
        ext4_fs_data = NULL;
        return -1;
    }
    
    // 读取超级块
    ext4_read_super_block(ext4_fs_data);
    
    // 打印调试信息
    kernel_printf("Mounting EXT4 file system\n");
    ext4_debug_print_super_block(&ext4_fs_data->super_block);
    
    return 0;
}

// 卸载文件系统
int ext4_umount(const char* target) {
    if (!ext4_fs_data || !target || strcmp(ext4_fs_data->mount_point, target) != 0) {
        kernel_printf("EXT4 not mounted at %s\n", target);
        return -1;
    }
    
    // 释放资源
    free(ext4_fs_data);
    ext4_fs_data = NULL;
    
    kernel_printf("Unmounted EXT4 file system from %s\n", target);
    return 0;
}

// 获取文件系统对象
FileSystem* ext4_get_filesystem() {
    return &ext4_filesystem;
}

// 删除目录
extern int ext4_rmdir(const char* path) {
    if (!ext4_fs_data || !path) {
        kernel_printf("EXT4: Invalid parameters\n");
        return -1;
    }
    
    kernel_printf("EXT4: Removing directory '%s'\n", path);
    
    // 1. 解析路径，找到父目录和要删除的目录名
    int num_components;
    char** components = vfs_parse_path(path, &num_components);
    if (!components || num_components <= 0) {
        return -1;
    }
    
    // 不能删除根目录
    if (strcmp(path, "/") == 0) {
        kernel_printf("EXT4: Cannot remove root directory\n");
        vfs_free_path_components(components, num_components);
        return -1;
    }
    
    // 分离父目录路径和目录名
    char* parent_path = NULL;
    char* dir_name = NULL;
    
    if (num_components == 1) {
        // 在根目录下删除
        parent_path = strdup("/");
        dir_name = strdup(components[0]);
    } else {
        // 构建父目录路径
        parent_path = (char*)malloc(PATH_MAX);
        if (!parent_path) {
            vfs_free_path_components(components, num_components);
            return -1;
        }
        parent_path[0] = '\0';
        
        for (int i = 0; i < num_components - 1; i++) {
            size_t len = strlen(parent_path);
            strcpy(parent_path + len, "/");
            len += 1;
            strcpy(parent_path + len, components[i]);
        }
        
        if (strlen(parent_path) == 0) {
            strcpy(parent_path, "/");
        }
        
        dir_name = strdup(components[num_components - 1]);
    }
    
    vfs_free_path_components(components, num_components);
    
    // 2. 查找父目录的inode
    Inode* parent_inode = ext4_get_inode(parent_path);
    if (!parent_inode) {
        kernel_printf("EXT4: Parent directory '%s' not found\n", parent_path);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    // 3. 查找要删除的目录inode
    Inode* target_inode = ext4_get_inode(path);
    if (!target_inode) {
        kernel_printf("EXT4: Directory '%s' not found\n", path);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    // 检查是否为目录
    if (target_inode->type != FILE_TYPE_DIRECTORY) {
        kernel_printf("EXT4: '%s' is not a directory\n", path);
        vfs_destroy_inode(target_inode);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    // 4. 检查目录是否为空
    Ext4Inode* ext4_target_inode = ext4_read_inode(ext4_fs_data, target_inode->inode_num);
    if (!ext4_target_inode) {
        vfs_destroy_inode(target_inode);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    uint32_t block_size = ext4_fs_data->block_size;
    // 使用固定大小的缓冲区，避免分配过大的内存块
    uint8_t* block_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
    if (!block_buffer) {
        free(ext4_target_inode);
        vfs_destroy_inode(target_inode);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    // 读取目录的第一个数据块
    if (ext4_target_inode->block[0] != 0) {
        ext4_read_block(ext4_fs_data, ext4_target_inode->block[0], block_buffer);
    } else {
        memset(block_buffer, 0, block_size);
    }
    
    // 检查目录是否只包含 . 和 .. 两个条目
    int entry_count = 0;
    uint32_t offset = 0;
    Ext4DirEntry* entry = NULL;
    
    while (offset < block_size) {
        entry = (Ext4DirEntry*)(block_buffer + offset);
        
        // 检查是否到达块末尾
        if (entry->rec_len == 0) {
            break;
        }
        
        // 计数有效的目录项
        if (entry->inode != 0 && entry->name_len > 0) {
            entry_count++;
        }
        
        offset += entry->rec_len;
    }
    
    // 目录必须只包含 . 和 .. 两个条目才认为是空的
    if (entry_count != 2) {
        kernel_printf("EXT4: Directory '%s' is not empty\n", path);
        free(block_buffer);
        free(ext4_target_inode);
        vfs_destroy_inode(target_inode);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    // 5. 在父目录中删除目录项
    // 读取父目录的第一个数据块
    Ext4Inode* parent_ext4_inode = ext4_read_inode(ext4_fs_data, parent_inode->inode_num);
    if (!parent_ext4_inode) {
        free(block_buffer);
        free(ext4_target_inode);
        vfs_destroy_inode(target_inode);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    if (parent_ext4_inode->block[0] != 0) {
        ext4_read_block(ext4_fs_data, parent_ext4_inode->block[0], block_buffer);
    }
    
    // 查找并删除目录项
    offset = 0;
    int found = 0;
    
    while (offset < block_size && !found) {
        entry = (Ext4DirEntry*)(block_buffer + offset);
        
        // 检查是否到达块末尾
        if (entry->rec_len == 0) {
            break;
        }
        
        // 检查是否是要删除的目录
        if (entry->inode != 0 && entry->name_len > 0 && 
            strncmp(entry->name, dir_name, entry->name_len) == 0) {
            // 标记目录项为已删除（将inode设置为0）
            entry->inode = 0;
            found = 1;
        }
        
        offset += entry->rec_len;
    }
    
    if (!found) {
        kernel_printf("EXT4: Directory entry not found in parent directory\n");
        free(block_buffer);
        free(ext4_target_inode);
        free(parent_ext4_inode);
        vfs_destroy_inode(target_inode);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    // 写回修改后的父目录块
    ext4_write_block(ext4_fs_data, parent_ext4_inode->block[0], block_buffer);
    
    // 6. 释放目录的数据块
    if (ext4_target_inode->block[0] != 0) {
        ext4_free_block(ext4_target_inode->block[0]);
    }
    
    // 7. 释放目录的inode
    ext4_free_inode(target_inode->inode_num);
    
    // 8. 更新父目录的链接计数和修改时间
    parent_ext4_inode->links_count--;
    parent_ext4_inode->mtime = parent_ext4_inode->ctime = current_time();
    ext4_write_inode(ext4_fs_data, parent_inode->inode_num, parent_ext4_inode);
    
    // 9. 更新文件系统元数据
    ext4_write_super_block(ext4_fs_data);
    
    // 清理资源
    free(block_buffer);
    free(ext4_target_inode);
    free(parent_ext4_inode);
    vfs_destroy_inode(target_inode);
    vfs_destroy_inode(parent_inode);
    free(parent_path);
    free(dir_name);
    
    return 0;
}

// 删除文件
extern int ext4_remove(const char* path) {
    if (!ext4_fs_data || !path) {
        kernel_printf("EXT4: Invalid parameters\n");
        return -1;
    }
    
    kernel_printf("EXT4: Removing file '%s'\n", path);
    
    // 1. 解析路径，找到父目录和要删除的文件名
    int num_components;
    char** components = vfs_parse_path(path, &num_components);
    if (!components || num_components <= 0) {
        return -1;
    }
    
    // 分离父目录路径和文件名
    char* parent_path = NULL;
    char* file_name = NULL;
    
    if (num_components == 1) {
        // 在根目录下删除
        parent_path = strdup("/");
        file_name = strdup(components[0]);
    } else {
        // 构建父目录路径
        parent_path = (char*)malloc(PATH_MAX);
        if (!parent_path) {
            vfs_free_path_components(components, num_components);
            return -1;
        }
        parent_path[0] = '\0';
        
        for (int i = 0; i < num_components - 1; i++) {
            size_t len = strlen(parent_path);
            strcpy(parent_path + len, "/");
            len += 1;
            strcpy(parent_path + len, components[i]);
        }
        
        if (strlen(parent_path) == 0) {
            strcpy(parent_path, "/");
        }
        
        file_name = strdup(components[num_components - 1]);
    }
    
    vfs_free_path_components(components, num_components);
    
    // 2. 查找父目录的inode
    Inode* parent_inode = ext4_get_inode(parent_path);
    if (!parent_inode) {
        kernel_printf("EXT4: Parent directory '%s' not found\n", parent_path);
        free(parent_path);
        free(file_name);
        return -1;
    }
    
    // 3. 在父目录中查找要删除的文件
    // 读取父目录的第一个数据块
    uint32_t block_size = ext4_fs_data->block_size;
    // 使用固定大小的缓冲区，避免分配过大的内存块
    uint8_t* block_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
    if (!block_buffer) {
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(file_name);
        return -1;
    }
    
    Ext4Inode* parent_ext4_inode = ext4_read_inode(ext4_fs_data, parent_inode->inode_num);
    if (!parent_ext4_inode) {
        free(block_buffer);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(file_name);
        return -1;
    }
    
    if (parent_ext4_inode->block[0] != 0) {
        ext4_read_block(ext4_fs_data, parent_ext4_inode->block[0], block_buffer);
    }
    
    // 4. 在目录项中查找文件并删除
    uint32_t offset = 0;
    Ext4DirEntry* entry = NULL;
    uint32_t target_inode_num = 0;
    
    while (offset < block_size) {
        entry = (Ext4DirEntry*)(block_buffer + offset);
        
        // 检查是否到达块末尾
        if (entry->rec_len == 0) {
            break;
        }
        
        // 检查是否是要删除的文件
        if (entry->inode != 0 && entry->name_len > 0 && 
            strncmp(entry->name, file_name, entry->name_len) == 0) {
            // 保存要删除的inode编号
            target_inode_num = entry->inode;
            
            // 标记目录项为已删除（将inode设置为0）
            entry->inode = 0;
            break;
        }
        
        offset += entry->rec_len;
    }
    
    // 如果找到了文件，写回修改后的目录块
    if (target_inode_num != 0) {
        ext4_write_block(ext4_fs_data, parent_ext4_inode->block[0], block_buffer);
        
        // 5. 读取并释放文件的inode和数据块
        Ext4Inode* target_inode = ext4_read_inode(ext4_fs_data, target_inode_num);
        if (target_inode) {
            // 释放文件的数据块
            for (int i = 0; i < 12; i++) {  // 只处理直接块
                if (target_inode->block[i] != 0) {
                    ext4_free_block(target_inode->block[i]);
                }
            }
            
            // 释放inode
            ext4_free_inode(target_inode_num);
            free(target_inode);
        }
        
        // 更新父目录的修改时间
        parent_ext4_inode->mtime = parent_ext4_inode->ctime = current_time();
        ext4_write_inode(ext4_fs_data, parent_inode->inode_num, parent_ext4_inode);
        
        // 6. 更新文件系统元数据
        ext4_write_super_block(ext4_fs_data);
    } else {
        kernel_printf("EXT4: File '%s' not found\n", path);
    }
    
    // 清理资源
    free(block_buffer);
    free(parent_ext4_inode);
    vfs_destroy_inode(parent_inode);
    free(parent_path);
    free(file_name);
    
    return target_inode_num != 0 ? 0 : -1;
}

// 打开目录
extern int ext4_dir_opendir(Inode* inode) {
    if (!inode || !ext4_fs_data) {
        return -1;
    }
    
    // 检查inode是否为目录类型
    if (inode->type != FILE_TYPE_DIRECTORY) {
        return -1;
    }
    
    // 读取目录的EXT4 inode数据
    Ext4Inode* ext4_inode = ext4_read_inode(ext4_fs_data, inode->inode_num);
    if (!ext4_inode) {
        return -1;
    }
    
    // 初始化目录迭代器
    Ext4DirIterator* iterator = (Ext4DirIterator*)malloc(sizeof(Ext4DirIterator));
    if (!iterator) {
        free(ext4_inode);
        return -1;
    }
    
    iterator->current_block = 0;
    iterator->current_offset = 0;
    iterator->block_buffer = NULL;
    
    // 保存原始inode数据和迭代器到inode的私有数据
    // 首先释放旧的私有数据
    if (inode->private_data) {
        free(inode->private_data);
    }
    
    // 这里我们将ext4_inode和iterator都存储在private_data中
    // 但在实际系统中，可能需要一个更复杂的结构来管理这些数据
    inode->private_data = ext4_inode;
    
    // 存储迭代器在一个全局位置或专用结构中
    // 注意：这个实现在多任务环境下可能存在问题，因为它使用了一个全局变量
    // 在实际系统中，应该使用更安全的方式来管理目录迭代器
    
    // 更新目录的访问时间
    ext4_inode->atime = current_time();
    ext4_write_inode(ext4_fs_data, inode->inode_num, ext4_inode);
    
    return 0;
}

// 关闭目录
extern int ext4_dir_closedir(Inode* inode) {
    if (!inode || !ext4_fs_data) {
        return -1;
    }
    
    // Clean up directory iterator if it exists
    Ext4DirIterator* iterator = (Ext4DirIterator*)inode->private_data;
    if (iterator) {
        // Free block buffer if allocated
        if (iterator->block_buffer) {
            free(iterator->block_buffer);
        }
        
        // Free the iterator itself
        free(iterator);
        
        // Clear private_data to avoid dangling pointer
        inode->private_data = NULL;
    }
    
    return 0;
}

// 读取目录
extern int ext4_dir_readdir(Inode* inode, char* name, size_t name_len, FileType* type) {
    if (!inode || !name || !type || !ext4_fs_data) {
        return -1;
    }
    
    // Check if inode is a directory
    if (inode->type != FILE_TYPE_DIRECTORY) {
        return -1;
    }
    
    Ext4Inode* ext4_inode = (Ext4Inode*)inode->private_data;
    uint32_t block_size = ext4_fs_data->block_size;
    
    // Initialize directory iterator if it doesn't exist
    Ext4DirIterator* iterator = (Ext4DirIterator*)inode->private_data;
    if (!iterator) {
        iterator = (Ext4DirIterator*)malloc(sizeof(Ext4DirIterator));
        if (!iterator) {
            return -1;
        }
        
        iterator->current_block = 0;
        iterator->current_offset = 0;
        iterator->block_buffer = NULL;
        
        // Store iterator in inode's private_data
        inode->private_data = iterator;
    }
    
    // Allocate block buffer if not already allocated
    if (!iterator->block_buffer) {
        // 使用固定大小的缓冲区，避免分配过大的内存块
        iterator->block_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
        if (!iterator->block_buffer) {
            free(iterator);
            inode->private_data = NULL;
            return -1;
        }
    }
    
    // Read directory entries until we find a valid one or reach the end
    while (iterator->current_block < 12) {  // Assume using only direct blocks for simplicity
        uint32_t block_num = ext4_inode->block[iterator->current_block];
        
        // If block pointer is zero, we've reached the end of directory data
        if (block_num == 0) {
            break;
        }
        
        // If we're at the beginning of a block, read it
        if (iterator->current_offset == 0) {
            ext4_read_block(ext4_fs_data, block_num, iterator->block_buffer);
        }
        
        // Process entries in the current block
        while (iterator->current_offset < block_size) {
            Ext4DirEntry* entry = (Ext4DirEntry*)(iterator->block_buffer + iterator->current_offset);
            
            // Check if we've reached the end of valid entries in this block
            if (entry->rec_len == 0) {
                break;
            }
            
            // Skip over deleted entries (inode == 0)
            if (entry->inode != 0 && entry->name_len > 0) {
                // Copy the name (truncating if necessary)
                strncpy(name, entry->name, name_len - 1);
                name[name_len - 1] = '\0';
                
                // Map EXT4 file type to VFS FileType
                switch (entry->file_type) {
                    case 2:  // Regular file
                        *type = FILE_TYPE_REGULAR;
                        break;
                    case 1:  // Directory
                        *type = FILE_TYPE_DIRECTORY;
                        break;
                    case 6:  // Block device
                        *type = FILE_TYPE_BLOCK_DEVICE;
                        break;
                    case 3:  // Character device
                        *type = FILE_TYPE_CHAR_DEVICE;
                        break;
                    case 12: // Symbolic link
                        *type = FILE_TYPE_SYMLINK;
                        break;
                    case 13: // Socket
                    case 14: // Named pipe
                        *type = FILE_TYPE_PIPE;
                        break;
                    default: // Unknown type, default to regular file
                        *type = FILE_TYPE_REGULAR;
                        break;
                }
                
                // Update iterator position for next call
                iterator->current_offset += entry->rec_len;
                
                return 0; // Success
            }
            
            // Move to next entry
            iterator->current_offset += entry->rec_len;
        }
        
        // Move to next block
        iterator->current_block++;
        iterator->current_offset = 0;
    }
    
    // No more entries found, reset iterator for next readdir sequence
    free(iterator->block_buffer);
    free(iterator);
    inode->private_data = NULL;
    
    return -1; // No more entries
}

// 打开文件
extern int ext4_file_open(Inode* inode, int flags) {
    if (!inode || !ext4_fs_data) {
        return -1;
    }
    
    // 获取文件系统的inode数据
    Ext4Inode* ext4_inode = ext4_read_inode(ext4_fs_data, inode->inode_num);
    if (!ext4_inode) {
        return -1;
    }
    
    // 处理文件打开标志
    if (flags & O_TRUNC) {
        // 截断文件到0长度
        // 在实际实现中，这里应该释放文件的数据块并更新inode
        ext4_inode->size_lo = 0;
        ext4_inode->size_high = 0;
        ext4_inode->blocks_lo = 0;
        ext4_inode->blocks = 0;
        
        // 更新修改时间
        ext4_inode->mtime = current_time();
        ext4_write_inode(ext4_fs_data, inode->inode_num, ext4_inode);
    }
    
    // 更新访问时间
    ext4_inode->atime = current_time();
    ext4_write_inode(ext4_fs_data, inode->inode_num, ext4_inode);
    
    // 释放临时inode结构
    free(ext4_inode);
    
    return 0;
}

// 创建目录
extern int ext4_mkdir(const char* path, uint32_t permissions) {
    if (!ext4_fs_data || !path) {
        kernel_printf("EXT4: Invalid parameters\n");
        return -1;
    }
    
    kernel_printf("EXT4: Creating directory '%s' with permissions 0%o\n", path, permissions);
    
    // 1. 解析路径，找到父目录和新目录名称
    int num_components;
    char** components = vfs_parse_path(path, &num_components);
    if (!components || num_components <= 0) {
        return -1;
    }
    
    // 处理根目录情况
    if (strcmp(path, "/") == 0) {
        kernel_printf("EXT4: Root directory already exists\n");
        vfs_free_path_components(components, num_components);
        return -1;
    }
    
    // 分离父目录路径和新目录名称
    char* parent_path = NULL;
    char* dir_name = NULL;
    
    if (num_components == 1) {
        // 在根目录下创建
        parent_path = strdup("/");
        dir_name = strdup(components[0]);
    } else {
        // 构建父目录路径
        parent_path = (char*)malloc(PATH_MAX);
        if (!parent_path) {
            vfs_free_path_components(components, num_components);
            return -1;
        }
        parent_path[0] = '\0';
        
        for (int i = 0; i < num_components - 1; i++) {
            size_t len = strlen(parent_path);
            strcpy(parent_path + len, "/");
            len += 1;
            strcpy(parent_path + len, components[i]);
        }
        
        if (strlen(parent_path) == 0) {
            strcpy(parent_path, "/");
        }
        
        dir_name = strdup(components[num_components - 1]);
    }
    
    vfs_free_path_components(components, num_components);
    
    // 2. 查找父目录的inode
    Inode* parent_inode = ext4_get_inode(parent_path);
    if (!parent_inode) {
        kernel_printf("EXT4: Parent directory '%s' not found\n", parent_path);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    // 3. 分配新的目录inode
    uint32_t new_inode_num = ext4_allocate_inode();
    if (new_inode_num == 0) {
        kernel_printf("EXT4: Failed to allocate inode\n");
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    // 4. 分配数据块用于新目录
    uint32_t data_block = ext4_allocate_block();
    if (data_block == 0) {
        kernel_printf("EXT4: Failed to allocate data block\n");
        ext4_free_inode(new_inode_num);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    // 5. 创建新的目录inode并初始化
    Ext4Inode* ext4_new_inode = (Ext4Inode*)malloc(sizeof(Ext4Inode));
    if (!ext4_new_inode) {
        kernel_printf("EXT4: Memory allocation failed\n");
        ext4_free_block(data_block);
        ext4_free_inode(new_inode_num);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    memset(ext4_new_inode, 0, sizeof(Ext4Inode));
    ext4_new_inode->mode = permissions | S_IFDIR;  // 设置为目录类型
    ext4_new_inode->uid = 0;                       // 默认为root用户
    ext4_new_inode->gid = 0;                       // 默认为root组
    ext4_new_inode->size_lo = ext4_fs_data->block_size; // 目录大小为一个块
    ext4_new_inode->atime = ext4_new_inode->ctime = ext4_new_inode->mtime = current_time();
    ext4_new_inode->links_count = 2;               // 目录至少有 . 和 .. 两个链接
    ext4_new_inode->blocks_lo = ext4_fs_data->block_size / 512; // 按扇区计算块数
    ext4_new_inode->block[0] = data_block;         // 第一个数据块
    
    // 写入新inode到磁盘
    ext4_write_inode(ext4_fs_data, new_inode_num, ext4_new_inode);
    
    // 6. 初始化目录数据块，添加 . 和 .. 条目
    // 使用固定大小的缓冲区，避免分配过大的内存块
    uint8_t* block_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
    if (!block_buffer) {
        free(ext4_new_inode);
        ext4_free_block(data_block);
        ext4_free_inode(new_inode_num);
        vfs_destroy_inode(parent_inode);
        free(parent_path);
        free(dir_name);
        return -1;
    }
    
    memset(block_buffer, 0, ext4_fs_data->block_size);
    
    // 添加 . 条目（指向自身）
    Ext4DirEntry* self_entry = (Ext4DirEntry*)block_buffer;
    self_entry->inode = new_inode_num;
    self_entry->rec_len = 12;  // 基础长度 + 2字节名称
    self_entry->name_len = 1;
    self_entry->file_type = 2; // 目录类型
    strcpy(self_entry->name, ".");
    
    // 添加 .. 条目（指向上级目录）
    Ext4DirEntry* parent_entry = (Ext4DirEntry*)(block_buffer + self_entry->rec_len);
    parent_entry->inode = parent_inode->inode_num;   // 使用实际的父目录inode编号
    parent_entry->rec_len = ext4_fs_data->block_size - self_entry->rec_len;
    parent_entry->name_len = 2;
    parent_entry->file_type = 2; // 目录类型
    strcpy(parent_entry->name, "..");
    
    // 写入目录数据块到磁盘
    ext4_write_block(ext4_fs_data, data_block, block_buffer);
    
    // 7. 在父目录中添加新目录条目
    int result = ext4_add_dir_entry(parent_inode, dir_name, new_inode_num, FILE_TYPE_DIRECTORY);
    
    // 8. 更新父目录的链接计数和修改时间
    Ext4Inode* parent_ext4_inode = ext4_read_inode(ext4_fs_data, parent_inode->inode_num);
    if (parent_ext4_inode) {
        parent_ext4_inode->links_count++;
        parent_ext4_inode->mtime = parent_ext4_inode->ctime = current_time();
        ext4_write_inode(ext4_fs_data, parent_inode->inode_num, parent_ext4_inode);
        free(parent_ext4_inode);
    }
    
    // 9. 更新文件系统元数据
    ext4_fs_data->super_block.free_inodes_count--;
    ext4_fs_data->super_block.free_blocks_count_lo--;
    ext4_fs_data->super_block.wtime = current_time();
    ext4_write_super_block(ext4_fs_data);
    
    // 清理资源
    free(block_buffer);
    free(ext4_new_inode);
    vfs_destroy_inode(parent_inode);
    free(parent_path);
    free(dir_name);
    
    if (result < 0) {
        return result;
    }
    
    return 0;
}

// 函数声明
static Inode* ext4_find_inode(const char* path);

// 查找文件或目录的inode
static Inode* ext4_find_inode(const char* path) {
    if (!ext4_fs_data || !path) {
        return NULL;
    }
    
    // 解析路径组件
    int num_components;
    char** components = vfs_parse_path(path, &num_components);
    if (!components) {
        return NULL;
    }
    
    // 当前目录inode
    Inode* current_inode = NULL;
    
    // 处理根目录
    if (strcmp(path, "/") == 0) {
        // 创建根目录inode
        current_inode = vfs_create_inode(FILE_TYPE_DIRECTORY, 0755, NULL);
        if (!current_inode) {
            vfs_free_path_components(components, num_components);
            return NULL;
        }
        
        // 设置文件操作函数
        FileOperations* dir_ops = (FileOperations*)malloc(sizeof(FileOperations));
        if (!dir_ops) {
            vfs_destroy_inode(current_inode);
            vfs_free_path_components(components, num_components);
            return NULL;
        }
        
        // 设置目录操作函数
        memset(dir_ops, 0, sizeof(FileOperations));
        dir_ops->opendir = ext4_dir_opendir;
        dir_ops->closedir = ext4_dir_closedir;
        dir_ops->readdir = ext4_dir_readdir;
        
        current_inode->private_data = dir_ops;
    } else {
        // 查找普通文件
        // 在实际实现中，这里应该遍历路径组件，从根目录开始查找
        
        // 简化实现：只返回一个模拟的文件inode
        current_inode = vfs_create_inode(FILE_TYPE_REGULAR, 0644, NULL);
        if (!current_inode) {
            vfs_free_path_components(components, num_components);
            return NULL;
        }
        
        // 设置文件操作函数
        FileOperations* file_ops = (FileOperations*)malloc(sizeof(FileOperations));
        if (!file_ops) {
            vfs_destroy_inode(current_inode);
            vfs_free_path_components(components, num_components);
            return NULL;
        }
        
        // 设置文件操作函数
        memset(file_ops, 0, sizeof(FileOperations));
        file_ops->read = ext4_file_read;
        file_ops->write = ext4_file_write;
        file_ops->close = ext4_file_close;
        file_ops->open = ext4_file_open;
        
        current_inode->private_data = file_ops;
    }
    
    vfs_free_path_components(components, num_components);
    return current_inode;
}

// 获取inode
Inode* ext4_get_inode(const char* path) {
    if (!ext4_fs_data || !path) {
        return NULL;
    }
    
    // 规范化路径
    char* normalized_path = vfs_normalize_path(path, "/");
    if (!normalized_path) {
        return NULL;
    }
    
    // 查找inode
    Inode* inode = ext4_find_inode(normalized_path);
    
    free(normalized_path);
    return inode;
}

// 初始化EXT4文件系统
int ext4_init() {
    // 设置文件系统操作函数
    ext4_filesystem.name = "ext4";
    ext4_filesystem.mount = ext4_mount;
    ext4_filesystem.umount = ext4_umount;
    ext4_filesystem.get_inode = ext4_get_inode;
    ext4_filesystem.mkdir = ext4_mkdir;
    ext4_filesystem.rmdir = ext4_rmdir;
    ext4_filesystem.remove = ext4_remove;
    
    // 注册文件系统到VFS
    if (vfs_register_filesystem(&ext4_filesystem) != 0) {
        kernel_printf("Failed to register EXT4 file system\n");
        return -1;
    }
    
    kernel_printf("EXT4 file system initialized\n");
    return 0;
}

uint32_t current_time() {
    // 简化实现，返回固定时间戳
    // 在实际系统中，应该从RTC或其他时钟源获取当前时间
    return 1600000000; // 示例时间戳 (2020-09-13)
}

uint32_t ext4_allocate_inode() {
    if (!ext4_fs_data) {
        return 0;
    }
    
    // 检查是否有空闲inode
    if (ext4_fs_data->super_block.free_inodes_count <= 0) {
        return 0;
    }
    
    // 遍历所有块组查找空闲inode
    for (uint32_t bg = 0; bg < ext4_fs_data->block_group_count; bg++) {
        // 读取块组描述符
        uint32_t bgd_block = ext4_fs_data->super_block.first_data_block + 1 + bg;
        Ext4BlockGroupDescriptor bgd;
        ext4_read_block(ext4_fs_data, bgd_block, &bgd);
        
        // 如果块组没有空闲inode，跳过
        if (bgd.bg_free_inodes_count_lo == 0 && bgd.bg_free_inodes_count_hi == 0) {
            continue;
        }
        
        // 读取inode位图
        // 使用固定大小的缓冲区，避免分配过大的内存块
        uint8_t* bitmap_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
        if (!bitmap_buffer) {
            return 0;
        }
        
        ext4_read_block(ext4_fs_data, bgd.bg_inode_bitmap_lo, bitmap_buffer);
        
        // 遍历位图查找空闲inode
        for (uint32_t i = 0; i < ext4_fs_data->inodes_per_group; i++) {
            uint32_t bitmap_index = i / 8;
            uint8_t bitmap_bit = i % 8;
            
            // 跳过前两个inode（通常是保留的）
            if (bg == 0 && i < 2) {
                continue;
            }
            
            // 检查位是否为0（空闲）
            if (!(bitmap_buffer[bitmap_index] & (1 << bitmap_bit))) {
                // 计算inode编号
                uint32_t inode_num = bg * ext4_fs_data->inodes_per_group + i + 1;
                
                // 标记inode为已使用
                bitmap_buffer[bitmap_index] |= (1 << bitmap_bit);
                ext4_write_block(ext4_fs_data, bgd.bg_inode_bitmap_lo, bitmap_buffer);
                
                // 更新块组描述符中的空闲inode计数
                bgd.bg_free_inodes_count_lo--;
                if (bgd.bg_free_inodes_count_lo == 0xFFFF && bgd.bg_free_inodes_count_hi > 0) {
                    bgd.bg_free_inodes_count_hi--;
                }
                
                // 写回块组描述符
                ext4_write_block(ext4_fs_data, bgd_block, &bgd);
                
                // 更新超级块中的空闲inode计数
                ext4_fs_data->super_block.free_inodes_count--;
                ext4_fs_data->super_block.wtime = current_time();
                ext4_write_super_block(ext4_fs_data);
                
                free(bitmap_buffer);
        return inode_num;
    }
    }
    
    free(bitmap_buffer);
}
    
    // 没有找到空闲inode
    return 0;
}

void ext4_free_inode(uint32_t inode_num) {
    if (!ext4_fs_data || inode_num == 0) {
        return;
    }
    
    // 计算inode所在的块组和块组内偏移
    uint32_t block_group = (inode_num - 1) / ext4_fs_data->inodes_per_group;
    uint32_t inode_index = (inode_num - 1) % ext4_fs_data->inodes_per_group;
    
    // 检查块组是否有效
    if (block_group >= ext4_fs_data->block_group_count) {
        return;
    }
    
    // 读取块组描述符
    uint32_t bgd_block = ext4_fs_data->super_block.first_data_block + 1 + block_group;
    Ext4BlockGroupDescriptor bgd;
    ext4_read_block(ext4_fs_data, bgd_block, &bgd);
    
    // 读取inode位图
    // 使用固定大小的缓冲区，避免分配过大的内存块
    uint8_t* bitmap_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
    if (!bitmap_buffer) {
        return;
    }
    
    ext4_read_block(ext4_fs_data, bgd.bg_inode_bitmap_lo, bitmap_buffer);
    
    // 计算位图中的索引和位
    uint32_t bitmap_index = inode_index / 8;
    uint8_t bitmap_bit = inode_index % 8;
    
    // 检查inode是否已经被释放（位为0）
    if (!(bitmap_buffer[bitmap_index] & (1 << bitmap_bit))) {
        free(bitmap_buffer);
        return;
    }
    
    // 标记inode为空闲
    bitmap_buffer[bitmap_index] &= ~(1 << bitmap_bit);
    ext4_write_block(ext4_fs_data, bgd.bg_inode_bitmap_lo, bitmap_buffer);
    
    // 更新块组描述符中的空闲inode计数
    bgd.bg_free_inodes_count_lo++;
    if (bgd.bg_free_inodes_count_lo == 0 && bgd.bg_free_inodes_count_hi > 0) {
        bgd.bg_free_inodes_count_hi--;
        bgd.bg_free_inodes_count_lo = 0xFFFF;
    }
    
    // 写回块组描述符
    ext4_write_block(ext4_fs_data, bgd_block, &bgd);
    
    // 更新超级块中的空闲inode计数
    ext4_fs_data->super_block.free_inodes_count++;
    ext4_fs_data->super_block.wtime = current_time();
    ext4_write_super_block(ext4_fs_data);
    
    free(bitmap_buffer);
}

uint32_t ext4_allocate_block() {
    if (!ext4_fs_data) {
        return 0;
    }
    
    // 检查是否有空闲块
    if (ext4_fs_data->super_block.free_blocks_count_lo <= 0 && 
        ext4_fs_data->super_block.free_blocks_count_hi <= 0) {
        return 0;
    }
    
    // 遍历所有块组查找空闲块
    for (uint32_t bg = 0; bg < ext4_fs_data->block_group_count; bg++) {
        // 读取块组描述符
        uint32_t bgd_block = ext4_fs_data->super_block.first_data_block + 1 + bg;
        Ext4BlockGroupDescriptor bgd;
        ext4_read_block(ext4_fs_data, bgd_block, &bgd);
        
        // 如果块组没有空闲块，跳过
        if (bgd.bg_free_blocks_count_lo == 0 && bgd.bg_free_blocks_count_hi == 0) {
            continue;
        }
        
        // 读取块位图
        // 使用固定大小的缓冲区，避免分配过大的内存块
        uint8_t* bitmap_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
        if (!bitmap_buffer) {
            return 0;
        }
        
        ext4_read_block(ext4_fs_data, bgd.bg_block_bitmap_lo, bitmap_buffer);
        
        // 遍历位图查找空闲块
        for (uint32_t i = 0; i < ext4_fs_data->blocks_per_group; i++) {
            uint32_t bitmap_index = i / 8;
            uint8_t bitmap_bit = i % 8;
            
            // 跳过第一个数据块之前的块（通常是系统保留的）
            uint32_t block_num = bg * ext4_fs_data->blocks_per_group + i + 1;
            if (block_num <= ext4_fs_data->super_block.first_data_block) {
                continue;
            }
            
            // 检查位是否为0（空闲）
            if (!(bitmap_buffer[bitmap_index] & (1 << bitmap_bit))) {
                // 标记块为已使用
                bitmap_buffer[bitmap_index] |= (1 << bitmap_bit);
                ext4_write_block(ext4_fs_data, bgd.bg_block_bitmap_lo, bitmap_buffer);
                
                // 更新块组描述符中的空闲块计数
                bgd.bg_free_blocks_count_lo--;
                if (bgd.bg_free_blocks_count_lo == 0xFFFF && bgd.bg_free_blocks_count_hi > 0) {
                    bgd.bg_free_blocks_count_hi--;
                }
                
                // 写回块组描述符
                ext4_write_block(ext4_fs_data, bgd_block, &bgd);
                
                // 更新超级块中的空闲块计数
                ext4_fs_data->super_block.free_blocks_count_lo--;
                if (ext4_fs_data->super_block.free_blocks_count_lo == 0xFFFF && 
                    ext4_fs_data->super_block.free_blocks_count_hi > 0) {
                    ext4_fs_data->super_block.free_blocks_count_hi--;
                }
                
                ext4_fs_data->super_block.wtime = current_time();
                ext4_write_super_block(ext4_fs_data);
                
                free(bitmap_buffer);
                return block_num;
            }
        }
        
        free(bitmap_buffer);
    }
    
    // 没有找到空闲块
    return 0;
}

void ext4_free_block(uint32_t block_num) {
    if (!ext4_fs_data || block_num == 0) {
        return;
    }
    
    // 计算块所在的块组和块组内偏移
    uint32_t block_group = (block_num - 1) / ext4_fs_data->blocks_per_group;
    uint32_t block_index = (block_num - 1) % ext4_fs_data->blocks_per_group;
    
    // 检查块组是否有效
    if (block_group >= ext4_fs_data->block_group_count) {
        return;
    }
    
    // 检查块是否是系统保留块
    if (block_num <= ext4_fs_data->super_block.first_data_block) {
        return;
    }
    
    // 读取块组描述符
    uint32_t bgd_block = ext4_fs_data->super_block.first_data_block + 1 + block_group;
    Ext4BlockGroupDescriptor bgd;
    ext4_read_block(ext4_fs_data, bgd_block, &bgd);
    
    // 读取块位图
    // 使用固定大小的缓冲区，避免分配过大的内存块
    uint8_t* bitmap_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
    if (!bitmap_buffer) {
        return;
    }
    
    ext4_read_block(ext4_fs_data, bgd.bg_block_bitmap_lo, bitmap_buffer);
    
    // 计算位图中的索引和位
    uint32_t bitmap_index = block_index / 8;
    uint8_t bitmap_bit = block_index % 8;
    
    // 检查块是否已经被释放（位为0）
    if (!(bitmap_buffer[bitmap_index] & (1 << bitmap_bit))) {
        free(bitmap_buffer);
        return;
    }
    
    // 标记块为空闲
    bitmap_buffer[bitmap_index] &= ~(1 << bitmap_bit);
    ext4_write_block(ext4_fs_data, bgd.bg_block_bitmap_lo, bitmap_buffer);
    
    // 更新块组描述符中的空闲块计数
    bgd.bg_free_blocks_count_lo++;
    if (bgd.bg_free_blocks_count_lo == 0 && bgd.bg_free_blocks_count_hi > 0) {
        bgd.bg_free_blocks_count_hi--;
        bgd.bg_free_blocks_count_lo = 0xFFFF;
    }
    
    // 写回块组描述符
    ext4_write_block(ext4_fs_data, bgd_block, &bgd);
    
    // 更新超级块中的空闲块计数
    ext4_fs_data->super_block.free_blocks_count_lo++;
    if (ext4_fs_data->super_block.free_blocks_count_lo == 0 && 
        ext4_fs_data->super_block.free_blocks_count_hi > 0) {
        ext4_fs_data->super_block.free_blocks_count_hi--;
        ext4_fs_data->super_block.free_blocks_count_lo = 0xFFFF;
    }
    
    ext4_fs_data->super_block.wtime = current_time();
    ext4_write_super_block(ext4_fs_data);
    
    free(bitmap_buffer);
}

void ext4_write_inode(Ext4FileSystemData* fs_data, uint32_t inode_num, Ext4Inode* inode) {
    if (!fs_data || !inode) {
        return;
    }
    
    // 计算索引节点所在的块组和块组内偏移
    uint32_t block_group = (inode_num - 1) / fs_data->inodes_per_group;
    uint32_t inode_index = (inode_num - 1) % fs_data->inodes_per_group;
    
    // 读取块组描述符
    uint32_t bgd_block = fs_data->super_block.first_data_block + 1 + block_group;
    Ext4BlockGroupDescriptor bgd;
    ext4_read_block(fs_data, bgd_block, &bgd);
    
    // 计算索引节点表的起始块
    uint32_t inode_table_block = bgd.bg_inode_table_lo;
    
    // 计算索引节点在表中的偏移
    uint32_t inode_offset = inode_index * fs_data->super_block.inode_size;
    uint32_t block_offset = inode_offset / fs_data->block_size;
    uint32_t in_block_offset = inode_offset % fs_data->block_size;
    
    // 读取包含索引节点的块
    // 使用固定大小的缓冲区，避免分配过大的内存块
    uint8_t* block_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
    if (!block_buffer) {
        return;
    }
    
    // 先读取现有的块内容
    ext4_read_block(fs_data, inode_table_block + block_offset, block_buffer);
    
    // 更新块中的索引节点数据
    memcpy(block_buffer + in_block_offset, inode, sizeof(Ext4Inode));
    
    // 写回块
    ext4_write_block(fs_data, inode_table_block + block_offset, block_buffer);
    
    free(block_buffer);
}

void ext4_write_super_block(Ext4FileSystemData* fs_data) {
    if (!fs_data) {
        return;
    }
    
    uint8_t buffer[512];
    memset(buffer, 0, sizeof(buffer));
    
    // 复制超级块数据到缓冲区
    memcpy(buffer, &fs_data->super_block, sizeof(Ext4SuperBlock));
    
    // 写入包含超级块的扇区
    fs_data->device->write(EXT4_SUPER_BLOCK_OFFSET / BLOCK_SIZE, buffer);
}

int ext4_add_dir_entry(Inode* dir_inode, const char* name, uint32_t inode_num, FileType type) {
    if (!dir_inode || !name || !ext4_fs_data) {
        return -1;
    }
    
    // 检查是否为目录
    if (dir_inode->type != FILE_TYPE_DIRECTORY) {
        return -1;
    }
    
    // 简化实现：假设目录足够大，并且直接在第一个块中添加条目
    Ext4Inode* ext4_dir_inode = (Ext4Inode*)dir_inode->private_data;
    uint32_t block_size = ext4_fs_data->block_size;
    
    // 读取目录的第一个数据块
    // 使用固定大小的缓冲区，避免分配过大的内存块
    uint8_t* block_buffer = (uint8_t*)malloc(MAX_DIR_BUFFER_SIZE);
    if (!block_buffer) {
        return -1;
    }
    
    if (ext4_dir_inode->block[0] != 0) {
        ext4_read_block(ext4_fs_data, ext4_dir_inode->block[0], block_buffer);
    } else {
        // 如果没有数据块，分配一个
        uint32_t new_block = ext4_allocate_block();
        if (new_block == 0) {
            free(block_buffer);
            return -1;
        }
        
        ext4_dir_inode->block[0] = new_block;
        memset(block_buffer, 0, block_size);
        
        // 更新索引节点
        ext4_write_inode(ext4_fs_data, dir_inode->inode_num, ext4_dir_inode);
    }
    
    // 查找空闲的目录条目位置
    uint32_t offset = 0;
    Ext4DirEntry* entry = NULL;
    
    while (offset < block_size) {
        entry = (Ext4DirEntry*)(block_buffer + offset);
        
        // 如果找到一个空条目（inode为0）或者到达块末尾
        if (entry->inode == 0 || offset + entry->rec_len >= block_size) {
            break;
        }
        
        offset += entry->rec_len;
    }
    
    // 计算新条目的大小
    uint8_t name_len = strlen(name);
    uint16_t rec_len = (8 + name_len + 3) & ~3; // 确保4字节对齐
    
    // 确保有足够的空间
    if (offset + rec_len > block_size) {
        free(block_buffer);
        return -1; // 空间不足
    }
    
    // 创建新目录条目
    entry = (Ext4DirEntry*)(block_buffer + offset);
    entry->inode = inode_num;
    entry->rec_len = rec_len;
    entry->name_len = name_len;
    
    // 映射VFS文件类型到EXT4文件类型
    switch (type) {
        case FILE_TYPE_REGULAR:
            entry->file_type = 2;
            break;
        case FILE_TYPE_DIRECTORY:
            entry->file_type = 1;
            break;
        case FILE_TYPE_BLOCK_DEVICE:
            entry->file_type = 6;
            break;
        case FILE_TYPE_CHAR_DEVICE:
            entry->file_type = 3;
            break;
        case FILE_TYPE_SYMLINK:
            entry->file_type = 12;
            break;
        case FILE_TYPE_PIPE:
            entry->file_type = 13;
            break;
        default:
            entry->file_type = 2; // 默认是普通文件
            break;
    }
    
    // 复制名称
    strncpy(entry->name, name, name_len);
    
    // 写回块
    ext4_write_block(ext4_fs_data, ext4_dir_inode->block[0], block_buffer);
    
    free(block_buffer);
    return 0;
}