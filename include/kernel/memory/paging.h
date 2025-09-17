#ifndef OS_KERNEL_MEMORY_PAGING_H
#define OS_KERNEL_MEMORY_PAGING_H

#include <stdtype.h>

// 内核相关常量定义
#define KERNEL_START_ADDRESS 0x0100000      // 内核起始地址（1MB）

// 页表相关常量定义
#define PAGE_SIZE 4096                      // 页大小为4KB
#define PAGE_DIR_ENTRIES 1024               // 页目录项数量
#define PAGE_TABLE_ENTRIES 1024             // 页表项数量
#define PAGE_MASK 0xFFFFF000                // 页掩码
#define OFFSET_MASK 0x00000FFF              // 偏移量掩码

// 页表项标志位定义
#define PTE_PRESENT 0x001                   // 页面存在
#define PTE_WRITABLE 0x002                  // 页面可写
#define PTE_USER 0x004                      // 用户模式可访问
#define PTE_WRITE_THROUGH 0x008             // 写通缓存
#define PTE_CACHE_DISABLED 0x010            // 禁用缓存
#define PTE_ACCESSED 0x020                  // 已访问
#define PTE_DIRTY 0x040                     // 已修改
#define PTE_PAT 0x080                       // 页属性表
#define PTE_GLOBAL 0x100                    // 全局页
#define PTE_PSE 0x080                       // 页大小扩展

// 页表项结构
typedef struct PageTableEntry {
    uint32_t present : 1;                   // 页面是否在物理内存中
    uint32_t read_write : 1;                // 0 = 只读, 1 = 可读可写
    uint32_t user_supervisor : 1;           // 0 = 内核模式, 1 = 用户模式
    uint32_t write_through : 1;             // 写通/回写
    uint32_t cache_disabled : 1;            // 是否禁用缓存
    uint32_t accessed : 1;                  // 是否被访问过
    uint32_t dirty : 1;                     // 是否被修改过
    uint32_t pat : 1;                       // 页属性表
    uint32_t global : 1;                    // 全局页
    uint32_t available : 3;                 // 可用位
    uint32_t page_base_address : 20;        // 页基地址
} PageTableEntry;

// 页目录项结构
typedef struct PageDirectoryEntry {
    uint32_t present : 1;                   // 目录项是否存在
    uint32_t read_write : 1;                // 0 = 只读, 1 = 可读可写
    uint32_t user_supervisor : 1;           // 0 = 内核模式, 1 = 用户模式
    uint32_t write_through : 1;             // 写通/回写
    uint32_t cache_disabled : 1;            // 是否禁用缓存
    uint32_t accessed : 1;                  // 是否被访问过
    uint32_t ignored : 1;                   // 忽略位
    uint32_t page_size : 1;                 // 页大小(0=4KB,1=4MB)
    uint32_t global : 1;                    // 全局页
    uint32_t available : 3;                 // 可用位
    uint32_t page_table_base_address : 20;  // 页表基地址
} PageDirectoryEntry;

// 页目录结构
typedef struct PageDirectory {
    PageDirectoryEntry entries[PAGE_DIR_ENTRIES]; // 页目录项
} PageDirectory;

// 页表结构
typedef struct PageTable {
    PageTableEntry entries[PAGE_TABLE_ENTRIES];   // 页表项
} PageTable;

// 内存区域类型
typedef enum {
    MEMORY_KERNEL = 0,                      // 内核内存
    MEMORY_CODE = 1,                        // 代码段
    MEMORY_DATA = 2,                        // 数据段
    MEMORY_HEAP = 3,                        // 堆内存
    MEMORY_STACK = 4,                       // 栈内存
    MEMORY_MAPPED_FILE = 5                  // 内存映射文件
} MemoryRegionType;

// 内存区域结构
typedef struct MemoryRegion {
    uint32_t virtual_address;               // 虚拟地址起始
    uint32_t physical_address;              // 物理地址起始
    uint32_t size;                          // 区域大小
    uint32_t flags;                         // 区域标志
    MemoryRegionType type;                  // 区域类型
    struct MemoryRegion* next;              // 指向下一个区域
} MemoryRegion;

// 页面框结构
typedef struct PageFrame {
    uint32_t physical_address;              // 物理地址
    uint32_t reference_count;               // 引用计数
    uint32_t flags;                         // 页面框标志
} PageFrame;

// 页面框管理器结构
typedef struct PageFrameManager {
    uint32_t total_frames;                  // 总页面框数
    uint32_t free_frames;                   // 空闲页面框数
    uint32_t* frame_bitmap;                 // 页面框位图
    PageFrame* frames;                      // 页面框数组
} PageFrameManager;

// 虚拟内存管理器结构
typedef struct VirtualMemoryManager {
    PageFrameManager* frame_manager;        // 页面框管理器
    PageDirectory* kernel_directory;        // 内核页目录
    uint32_t kernel_start;                  // 内核起始地址
    uint32_t kernel_end;                    // 内核结束地址
} VirtualMemoryManager;

// 函数声明

// 页面框管理函数
extern void pfm_init(PageFrameManager* manager, uint32_t start_address, uint32_t size);
extern uint32_t pfm_allocate_frame();
extern void pfm_free_frame(uint32_t frame_address);
extern uint32_t pfm_get_free_frames_count();

// 页表管理函数
extern PageDirectory* pd_create();
extern void pd_destroy(PageDirectory* directory);
extern uint32_t pd_get_physical_address(PageDirectory* directory, uint32_t virtual_address);
extern void pd_switch(PageDirectory* directory);
extern int pd_map_page(PageDirectory* directory, uint32_t virtual_address, uint32_t physical_address, uint32_t flags);
extern int pd_unmap_page(PageDirectory* directory, uint32_t virtual_address);

// 虚拟内存管理函数
extern void on_init_virtual_memory_manager(VirtualMemoryManager* manager, uint32_t kernel_start, uint32_t kernel_end);
extern int vmm_allocate_pages(PageDirectory* directory, uint32_t virtual_address, uint32_t size, uint32_t flags);
extern int vmm_free_pages(PageDirectory* directory, uint32_t virtual_address, uint32_t size);
extern int vmm_map_pages(PageDirectory* directory, uint32_t virtual_address, uint32_t physical_address, uint32_t size, uint32_t flags);
extern int vmm_unmap_pages(PageDirectory* directory, uint32_t virtual_address, uint32_t size);

// 页面故障处理函数
extern void page_fault_handler(uint32_t error_code);

// 内存映射文件支持函数
extern int vmm_map_file(PageDirectory* directory, const char* filename, uint32_t virtual_address, uint32_t flags);
extern int vmm_unmap_file(PageDirectory* directory, uint32_t virtual_address);

// 内存区域管理函数
extern MemoryRegion* vmm_create_memory_region(uint32_t virtual_address, uint32_t size, uint32_t flags, MemoryRegionType type);
extern void vmm_destroy_memory_region(MemoryRegion* region);

extern void enable_paging();
extern void disable_paging();
extern uint32_t get_current_page_directory();

#endif // OS_KERNEL_MEMORY_PAGING_H