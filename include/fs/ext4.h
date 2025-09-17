#ifndef EXT4_H
#define EXT4_H

#include <fs/vfs.h>
#include <driver/block.h>

// EXT4相关常量定义
#define EXT4_SUPER_MAGIC 0xEF53
#define EXT4_MIN_BLOCK_SIZE 1024
#define EXT4_MAX_BLOCK_SIZE 65536
#define EXT4_SUPER_BLOCK_OFFSET 1024

// EXT4超级块结构
typedef struct {
    uint32_t inodes_count;         // 索引节点总数
    uint32_t blocks_count_lo;      // 块总数（低32位）
    uint32_t r_blocks_count_lo;    // 预留块总数（低32位）
    uint32_t free_blocks_count_lo; // 空闲块数（低32位）
    uint32_t free_inodes_count;    // 空闲索引节点数
    uint32_t first_data_block;     // 第一个数据块
    uint32_t log_block_size;       // 块大小的对数
    int32_t log_frag_size;         // 片大小的对数
    uint32_t blocks_per_group;     // 每组的块数
    uint32_t frags_per_group;      // 每组的片数
    uint32_t inodes_per_group;     // 每组的索引节点数
    uint32_t mtime;                // 挂载时间
    uint32_t wtime;                // 写入时间
    uint16_t mnt_count;            // 挂载计数
    int16_t max_mnt_count;         // 最大挂载计数
    uint16_t magic;                // 魔数
    uint16_t state;                // 文件系统状态
    uint16_t errors;               // 错误处理方式
    uint16_t minor_rev_level;      // 次版本号
    uint32_t last_check;           // 最后检查时间
    uint32_t check_interval;       // 检查间隔
    uint32_t creator_os;           // 创建操作系统
    uint32_t rev_level;            // 版本级别
    uint16_t def_resuid;           // 默认保留用户ID
    uint16_t def_resgid;           // 默认保留组ID
    uint32_t first_inode;          // 第一个索引节点
    uint16_t inode_size;           // 索引节点大小
    uint16_t block_group_nr;       // 块组号
    uint32_t feature_compat;       // 兼容特性
    uint32_t feature_incompat;     // 不兼容特性
    uint32_t feature_ro_compat;    // 只读兼容特性
    uint8_t uuid[16];              // 文件系统UUID
    char volume_name[16];          // 卷名
    char last_mounted[64];         // 最后挂载路径
    uint32_t algorithm_usage_bitmap; // 算法使用位图
    uint8_t prealloc_blocks;       // 预分配块数
    uint8_t prealloc_dir_blocks;   // 预分配目录块数
    uint16_t reserved_gdt_blocks;  // 保留GDT块数
    uint8_t journal_uuid[16];      // 日志UUID
    uint32_t journal_inum;         // 日志索引节点
    uint32_t journal_dev;          // 日志设备
    uint32_t last_orphan;          // 最后孤立索引节点
    uint32_t hash_seed[4];         // 哈希种子
    uint8_t def_hash_version;      // 默认哈希版本
    uint8_t reserved_char_pad;     // 保留字节填充
    uint16_t default_mount_opts;   // 默认挂载选项
    uint16_t first_meta_bg;        // 第一个元数据块组
    uint8_t mkfs_time[4];          // 创建时间
    uint8_t journal_blocks[4];     // 日志块
    uint32_t blocks_count_hi;      // 块总数（高32位）
    uint32_t r_blocks_count_hi;    // 预留块总数（高32位）
    uint32_t free_blocks_count_hi; // 空闲块数（高32位）
    uint16_t min_extra_isize;      // 最小额外索引节点大小
    uint16_t want_extra_isize;     // 期望额外索引节点大小
    uint32_t flags;                // 特征标志
    uint16_t raid_stride;          // RAID步长
    uint16_t mmp_interval;         // MMP检查间隔
    uint64_t mmp_block;            // MMP检查块
    uint32_t raid_stripe_width;    // RAID条带宽度
    uint8_t log_groups_per_flex;   // 每个flex的日志组
    uint8_t checksum_type;         // 校验和类型
    uint16_t reserved_pad;         // 保留填充
    uint64_t kbytes_written;       // 写入的KB数
    uint32_t s_flags;              // 软标志
    uint16_t metadata_csum_seed;   // 元数据校验和种子
    uint32_t huge_files_in_dirs;   // 目录中的大文件数
    uint8_t padding[1024 - 236];   // 填充到1024字节
} __attribute__((packed)) Ext4SuperBlock;

// EXT4块组描述符结构
typedef struct {
    uint32_t bg_block_bitmap_lo;   // 块位图块号（低32位）
    uint32_t bg_inode_bitmap_lo;   // 索引节点位图块号（低32位）
    uint32_t bg_inode_table_lo;    // 索引节点表块号（低32位）
    uint16_t bg_free_blocks_count_lo; // 空闲块数（低16位）
    uint16_t bg_free_inodes_count_lo; // 空闲索引节点数（低16位）
    uint16_t bg_used_dirs_count_lo; // 已使用目录数（低16位）
    uint16_t bg_flags;             // 块组标志
    uint32_t bg_exclude_bitmap_lo; // 排除位图块号（低32位）
    uint16_t bg_block_bitmap_csum_lo; // 块位图校验和（低16位）
    uint16_t bg_inode_bitmap_csum_lo; // 索引节点位图校验和（低16位）
    uint16_t bg_itable_unused_lo;  // 未使用的索引节点表项（低16位）
    uint16_t bg_checksum;          // 块组描述符校验和
    uint32_t bg_block_bitmap_hi;   // 块位图块号（高32位）
    uint32_t bg_inode_bitmap_hi;   // 索引节点位图块号（高32位）
    uint32_t bg_inode_table_hi;    // 索引节点表块号（高32位）
    uint16_t bg_free_blocks_count_hi; // 空闲块数（高16位）
    uint16_t bg_free_inodes_count_hi; // 空闲索引节点数（高16位）
    uint16_t bg_used_dirs_count_hi; // 已使用目录数（高16位）
    uint16_t bg_itable_unused_hi;  // 未使用的索引节点表项（高16位）
    uint32_t bg_exclude_bitmap_hi; // 排除位图块号（高32位）
    uint8_t bg_reserved[12];       // 保留
} __attribute__((packed)) Ext4BlockGroupDescriptor;

// EXT4索引节点结构
typedef struct {
    uint16_t mode;                 // 文件模式
    uint16_t uid;                  // 用户ID
    uint32_t size_lo;              // 文件大小（低32位）
    uint32_t atime;                // 访问时间
    uint32_t ctime;                // 状态更改时间
    uint32_t mtime;                // 修改时间
    uint32_t dtime;                // 删除时间
    uint16_t gid;                  // 组ID
    uint16_t links_count;          // 链接计数
    uint32_t blocks_lo;            // 块计数（低32位）
    uint32_t flags;                // 文件标志
    union {
        uint32_t osd1;             // 特定于操作系统的值
    } osd1;                        // 特定于操作系统的字段
    uint32_t block[15];            // 块指针
    uint32_t generation;           // 文件版本（用于NFS）
    uint32_t file_acl_lo;          // 文件访问控制列表（低32位）
    uint32_t size_high;            // 文件大小（高32位）
    uint32_t obso_faddr;           // 过时的碎片地址
    union {
        struct {
            uint16_t l_i_blocks_high; // 块计数（高16位）
            uint16_t l_i_file_acl_high; // 文件访问控制列表（高16位）
            uint16_t l_i_uid_high;   // 高16位用户ID
            uint16_t l_i_gid_high;   // 高16位组ID
            uint16_t l_i_checksum_lo; // 索引节点校验和（低16位）
            uint16_t l_i_reserved;   // 保留
        } linux1;
    } osd2;                        // 特定于操作系统的字段
    uint16_t checksum_hi;          // 索引节点校验和（高16位）
    uint32_t i_version;            // 文件版本（64位）
    uint64_t i_size;               // 文件大小（64位）
    uint64_t blocks;               // 块计数（64位）
    uint64_t delay_acct_blks;      // 延迟分配块数
    uint64_t i_dtime;              // 删除时间（64位）
} __attribute__((packed)) Ext4Inode;

// EXT4文件系统私有数据结构
typedef struct {
    BlockDevice* device;           // 块设备指针
    Ext4SuperBlock super_block;    // 超级块
    Ext4BlockGroupDescriptor* bg_descriptors; // 块组描述符
    uint32_t block_size;           // 块大小
    uint32_t blocks_per_group;     // 每组块数
    uint32_t inodes_per_group;     // 每组索引节点数
    uint32_t block_group_count;    // 块组数量
    char mount_point[64];          // 挂载点
} Ext4FileSystemData;

// EXT4文件系统操作函数声明
extern int ext4_init();
extern int ext4_mount(const char* source, const char* target);
extern int ext4_umount(const char* target);
extern Inode* ext4_get_inode(const char* path);
extern FileSystem* ext4_get_filesystem();

// EXT4文件操作函数声明
extern size_t ext4_file_read(Inode* inode, void* buffer, size_t size, size_t offset);
extern size_t ext4_file_write(Inode* inode, const void* buffer, size_t size, size_t offset);
extern int ext4_file_close(Inode* inode);
extern int ext4_file_open(Inode* inode, int flags);

extern void ext4_read_super_block(Ext4FileSystemData* fs_data);
extern Ext4Inode* ext4_read_inode(Ext4FileSystemData* fs_data, uint32_t inode_num);
extern void ext4_read_block(Ext4FileSystemData* fs_data, uint32_t block_num, void* buffer);
extern void ext4_write_block(Ext4FileSystemData* fs_data, uint32_t block_num, const void* buffer);

extern void ext4_debug_print_super_block(Ext4SuperBlock* sb);

#endif // EXT4_H