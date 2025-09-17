#include "kernel/memory/paging.h"
#include "stdtype.h"
#include "kernel/kerio.h"
#include "kernel/memory/malloc.h"
#include "kernel/string.h"
#include "kernel/interrupt/interrupt.h"
#include "kernel/gdt.h"
#include "kernel/multitask/process.h"
#include "fs/vfs.h"

extern ProcessManager* process_manager;

// 全局虚拟内存管理器
VirtualMemoryManager* vmm = NULL;

// 内联汇编函数，用于操作CR0和CR3寄存器
static inline void set_cr3(uint32_t page_directory_physical_address) {
    asm volatile ("mov %0, %%cr3" : : "r"(page_directory_physical_address));
}

static inline uint32_t get_cr3() {
    uint32_t cr3;
    asm volatile ("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

static inline void set_cr0(uint32_t cr0) {
    asm volatile ("mov %0, %%cr0" : : "r"(cr0));
}

static inline uint32_t get_cr0() {
    uint32_t cr0;
    asm volatile ("mov %%cr0, %0" : "=r"(cr0));
    return cr0;
}

// 初始化页面框管理器
void pfm_init(PageFrameManager* manager, uint32_t start_address, uint32_t size) {
    // 确保start_address是页对齐的
    start_address = (start_address + PAGE_SIZE - 1) & PAGE_MASK;
    
    // 计算页面框数量
    manager->total_frames = size / PAGE_SIZE;
    manager->free_frames = manager->total_frames;
    
    // 为页面框位图分配内存
    uint32_t bitmap_size = (manager->total_frames + 31) / 32; // 向上取整到32的倍数
    manager->frame_bitmap = (uint32_t*)malloc(bitmap_size * sizeof(uint32_t));
    memset(manager->frame_bitmap, 0, bitmap_size * sizeof(uint32_t));
    
    // 分配页面框数组
    manager->frames = (PageFrame*)malloc(manager->total_frames * sizeof(PageFrame));
    memset(manager->frames, 0, manager->total_frames * sizeof(PageFrame));
    
    // 初始化页面框数组
    for (uint32_t i = 0; i < manager->total_frames; i++) {
        manager->frames[i].physical_address = start_address + i * PAGE_SIZE;
        manager->frames[i].reference_count = 0;
        manager->frames[i].flags = 0;
    }
    
    kernel_printf("Page Frame Manager initialized: %d frames available\n", manager->free_frames);
}

// 分配一个页面框
uint32_t pfm_allocate_frame() {
    if (!vmm || !vmm->frame_manager) {
        kernel_printf("Page Frame Manager not initialized\n");
        return 0;
    }
    
    PageFrameManager* manager = vmm->frame_manager;
    
    // 遍历位图寻找空闲页面框
    for (uint32_t i = 0; i < manager->total_frames; i++) {
        uint32_t bitmap_index = i / 32;
        uint32_t bit_index = i % 32;
        
        if (!(manager->frame_bitmap[bitmap_index] & (1 << bit_index))) {
            // 标记为已分配
            manager->frame_bitmap[bitmap_index] |= (1 << bit_index);
            manager->free_frames--;
            manager->frames[i].reference_count++;
            
            return manager->frames[i].physical_address;
        }
    }
    
    kernel_printf("No free page frames available\n");
    return 0;
}

// 释放一个页面框
void pfm_free_frame(uint32_t frame_address) {
    if (!vmm || !vmm->frame_manager) {
        kernel_printf("Page Frame Manager not initialized\n");
        return;
    }
    
    PageFrameManager* manager = vmm->frame_manager;
    
    // 计算页面框索引
    uint32_t frame_index = (frame_address - manager->frames[0].physical_address) / PAGE_SIZE;
    
    if (frame_index >= manager->total_frames) {
        kernel_printf("Invalid frame address: %x\n", frame_address);
        return;
    }
    
    // 检查是否已分配
    uint32_t bitmap_index = frame_index / 32;
    uint32_t bit_index = frame_index % 32;
    
    if (!(manager->frame_bitmap[bitmap_index] & (1 << bit_index))) {
        kernel_printf("Frame %x is not allocated\n", frame_address);
        return;
    }
    
    // 减少引用计数
    manager->frames[frame_index].reference_count--;
    
    // 如果引用计数为0，释放页面框
    if (manager->frames[frame_index].reference_count == 0) {
        manager->frame_bitmap[bitmap_index] &= ~(1 << bit_index);
        manager->free_frames++;
        manager->frames[frame_index].flags = 0;
    }
}

// 获取空闲页面框数量
uint32_t pfm_get_free_frames_count() {
    if (!vmm || !vmm->frame_manager) {
        return 0;
    }
    
    return vmm->frame_manager->free_frames;
}

// 创建页目录
PageDirectory* pd_create() {
    // 分配页目录内存
    PageDirectory* directory = (PageDirectory*)malloc(sizeof(PageDirectory));
    if (!directory) {
        kernel_printf("Failed to allocate memory for page directory\n");
        return NULL;
    }
    
    memset(directory, 0, sizeof(PageDirectory));
    
    // 如果是内核页目录，需要复制内核空间的映射
    if (vmm && vmm->kernel_directory && directory != vmm->kernel_directory) {
        // 复制内核空间的页目录项（假设内核空间从第768个页目录项开始）
        for (uint32_t i = 768; i < PAGE_DIR_ENTRIES; i++) {
            directory->entries[i] = vmm->kernel_directory->entries[i];
        }
    }
    
    return directory;
}

// 销毁页目录
void pd_destroy(PageDirectory* directory) {
    if (!directory || directory == vmm->kernel_directory) {
        kernel_printf("Cannot destroy NULL or kernel page directory\n");
        return;
    }
    
    // 释放所有用户空间页表
    for (uint32_t i = 0; i < 768; i++) {
        if (directory->entries[i].present) {
            PageTable* table = (PageTable*)(directory->entries[i].page_table_base_address << 12);
            free(table);
        }
    }
    
    free(directory);
}

// 获取虚拟地址对应的物理地址
uint32_t pd_get_physical_address(PageDirectory* directory, uint32_t virtual_address) {
    if (!directory) {
        return 0;
    }
    
    // 计算页目录索引和页表索引
    uint32_t dir_index = (virtual_address >> 22) & 0x3FF;
    uint32_t table_index = (virtual_address >> 12) & 0x3FF;
    
    // 检查页目录项是否存在
    if (!directory->entries[dir_index].present) {
        return 0;
    }
    
    // 获取页表
    PageTable* table = (PageTable*)(directory->entries[dir_index].page_table_base_address << 12);
    
    // 检查页表项是否存在
    if (!table->entries[table_index].present) {
        return 0;
    }
    
    // 计算物理地址
    uint32_t physical_address = (table->entries[table_index].page_base_address << 12) | 
                                (virtual_address & OFFSET_MASK);
    
    return physical_address;
}

// 切换页目录
void pd_switch(PageDirectory* directory) {
    if (!directory) {
        kernel_printf("Cannot switch to NULL page directory\n");
        return;
    }
    
    // 切换CR3寄存器
    set_cr3((uint32_t)directory);
}

// 映射虚拟页到物理页
int pd_map_page(PageDirectory* directory, uint32_t virtual_address, uint32_t physical_address, uint32_t flags) {
    if (!directory || !physical_address) {
        return -1;
    }
    
    // 计算页目录索引和页表索引
    uint32_t dir_index = (virtual_address >> 22) & 0x3FF;
    uint32_t table_index = (virtual_address >> 12) & 0x3FF;
    
    // 如果页目录项不存在，创建页表
    if (!directory->entries[dir_index].present) {
        PageTable* table = (PageTable*)malloc(sizeof(PageTable));
        if (!table) {
            return -1;
        }
        memset(table, 0, sizeof(PageTable));
        
        // 更新页目录项
        directory->entries[dir_index].present = 1;
        directory->entries[dir_index].read_write = (flags & PTE_WRITABLE) ? 1 : 0;
        directory->entries[dir_index].user_supervisor = (flags & PTE_USER) ? 1 : 0;
        directory->entries[dir_index].write_through = (flags & PTE_WRITE_THROUGH) ? 1 : 0;
        directory->entries[dir_index].cache_disabled = (flags & PTE_CACHE_DISABLED) ? 1 : 0;
        directory->entries[dir_index].page_table_base_address = (uint32_t)table >> 12;
    }
    
    // 获取页表
    PageTable* table = (PageTable*)(directory->entries[dir_index].page_table_base_address << 12);
    
    // 更新页表项
    table->entries[table_index].present = 1;
    table->entries[table_index].read_write = (flags & PTE_WRITABLE) ? 1 : 0;
    table->entries[table_index].user_supervisor = (flags & PTE_USER) ? 1 : 0;
    table->entries[table_index].write_through = (flags & PTE_WRITE_THROUGH) ? 1 : 0;
    table->entries[table_index].cache_disabled = (flags & PTE_CACHE_DISABLED) ? 1 : 0;
    table->entries[table_index].page_base_address = physical_address >> 12;
    
    // 刷新TLB
    asm volatile ("invlpg (%0)" : : "r"(virtual_address));
    
    return 0;
}

// 解除页面映射
int pd_unmap_page(PageDirectory* directory, uint32_t virtual_address) {
    if (!directory) {
        return -1;
    }
    
    // 计算页目录索引和页表索引
    uint32_t dir_index = (virtual_address >> 22) & 0x3FF;
    uint32_t table_index = (virtual_address >> 12) & 0x3FF;
    
    // 检查页目录项是否存在
    if (!directory->entries[dir_index].present) {
        return 0;
    }
    
    // 获取页表
    PageTable* table = (PageTable*)(directory->entries[dir_index].page_table_base_address << 12);
    
    // 检查页表项是否存在
    if (!table->entries[table_index].present) {
        return 0;
    }
    
    // 保存物理地址以便释放页面框
    uint32_t physical_address = table->entries[table_index].page_base_address << 12;
    
    // 清除页表项
    memset(&table->entries[table_index], 0, sizeof(PageTableEntry));
    
    // 刷新TLB
    asm volatile ("invlpg (%0)" : : "r"(virtual_address));
    
    // 释放页面框
    pfm_free_frame(physical_address);
    
    return 0;
}

// 启用分页
void enable_paging() {
    uint32_t cr0 = get_cr0();
    set_cr0(cr0 | 0x80000000); // 设置CR0的PG位
}

// 禁用分页
void disable_paging() {
    uint32_t cr0 = get_cr0();
    set_cr0(cr0 & ~0x80000000); // 清除CR0的PG位
}

// 获取当前页目录
uint32_t get_current_page_directory() {
    return get_cr3();
}

// 页面故障处理函数
void page_fault_handler(uint32_t error_code) {
    // 获取导致页面故障的虚拟地址
    uint32_t fault_address;
    asm volatile ("mov %%cr2, %0" : "=r"(fault_address));
    
    // 检查是否是用户模式下的页面错误
    if (error_code & 0x4) {
        if (!process_manager || !process_manager->current_process) {
            kernel_printf("User page fault but no current process\n");
            for (;;);
        }
        
        Process* current = process_manager->current_process;
        
        // 检查是否是写保护错误（可能是写时复制）
        if (!(error_code & 0x1) && (error_code & 0x2)) {
            // 这是一个保护违例，可能是写时复制页面
            uint32_t dir_index = (fault_address >> 22) & 0x3FF;
            uint32_t table_index = (fault_address >> 12) & 0x3FF;
            
            if (current->page_directory->entries[dir_index].present) {
                PageTable* table = (PageTable*)(current->page_directory->entries[dir_index].page_table_base_address << 12);
                if (table->entries[table_index].present) {
                    // 检查是否是写时复制页面（只读标记，但尝试写入）
                    if (!table->entries[table_index].read_write) {
                        // 分配新的物理页面
                        uint32_t new_physical = pfm_allocate_frame();
                        if (new_physical) {
                            // 复制旧页面内容到新页面
                            uint32_t old_physical = table->entries[table_index].page_base_address << 12;
                            memcpy((void*)new_physical, (void*)old_physical, PAGE_SIZE);
                            
                            // 更新页表项，映射到新页面并设置可写
                            table->entries[table_index].page_base_address = new_physical >> 12;
                            table->entries[table_index].read_write = 1;
                            
                            // 刷新TLB
                            asm volatile ("invlpg (%0)" : : "r"(fault_address));
                            return; // 成功处理，返回继续执行
                        }
                    }
                }
            }
        }
        
        // 检查是否是页面不存在错误（可能是按需分页）
        if (error_code & 0x1) {
            // 检查内存区域链表，看是否有这个虚拟地址范围的映射
            MemoryRegion* region = current->memory_regions;
            while (region) {
                if (fault_address >= region->virtual_address && 
                    fault_address < region->virtual_address + region->size) {
                    
                    // 分配物理页面
                    uint32_t physical_address = pfm_allocate_frame();
                    if (physical_address) {
                        // 清0页面内容
                        memset((void*)physical_address, 0, PAGE_SIZE);
                        
                        // 页对齐虚拟地址
                        uint32_t aligned_address = fault_address & PAGE_MASK;
                        
                        // 映射页面
                        if (pd_map_page(current->page_directory, aligned_address, physical_address, 
                                       region->flags) == 0) {
                            // 如果是内存映射文件，需要加载文件内容
                            if (region->type == MEMORY_MAPPED_FILE) {
                                // 实际系统中需要加载文件内容
                                kernel_printf("Loading file content for memory mapped file\n");
                            }
                            
                            // 刷新TLB
                            asm volatile ("invlpg (%0)" : : "r"(fault_address));
                            return; // 成功处理，返回继续执行
                        }
                    }
                    break;
                }
                region = region->next;
            }
        }
    }
    
    // 如果无法处理，打印错误信息并终止进程
    kernel_printf("Unhandled page fault at address 0x%x\n", fault_address);
    
    // 检查错误类型
    if (error_code & 0x1) {
        kernel_printf("Page not present\n");
    } else {
        kernel_printf("Protection violation\n");
    }
    
    if (error_code & 0x2) {
        kernel_printf("Write operation\n");
    } else {
        kernel_printf("Read operation\n");
    }
    
    if (error_code & 0x4) {
        kernel_printf("User mode\n");
        // 终止当前进程
        if (process_manager && process_manager->current_process) {
            terminate_process(process_manager->current_process->pid, -1);
            // 强制调度
            asm volatile ("int $0x20");
        }
    } else {
        kernel_printf("Kernel mode\n");
        // 内核错误，死循环
        for (;;);
    }
}

// 声明全局物理内存大小变量，由kernel.c初始化
extern uint32_t memupper_global;

// 初始化虚拟内存管理器
void on_init_virtual_memory_manager(VirtualMemoryManager* manager, uint32_t kernel_start, uint32_t kernel_end) {
    // 保存全局虚拟内存管理器指针
    vmm = manager;
    
    // 初始化页面框管理器
    PageFrameManager* frame_manager = (PageFrameManager*)malloc(sizeof(PageFrameManager));
    if (!frame_manager) {
        kernel_printf("Failed to allocate memory for page frame manager\n");
        return;
    }

    
    
    // 计算可用物理内存大小
    uint32_t total_memory = memupper_global * 1024;
    
    // 初始化页面框管理器，从16MB开始（避开内核占用的内存）
    uint32_t frame_manager_start = 16 * 1024 * 1024;
    uint32_t frame_manager_size = total_memory - frame_manager_start;

    
    pfm_init(frame_manager, frame_manager_start, frame_manager_size);
    
    manager->frame_manager = frame_manager;
    manager->kernel_start = kernel_start;
    manager->kernel_end = kernel_end;
    
    // 创建内核页目录
    manager->kernel_directory = pd_create();

    
    if (!manager->kernel_directory) {
        kernel_printf("Failed to create kernel page directory\n");
        return;
    }
    
    // Print virtual memory debug information
    kernel_printf("=== Virtual Memory Debug Information ===\n");
    kernel_printf("Kernel start address: 0x%x\n", kernel_start);
    kernel_printf("Kernel end address: 0x%x\n", kernel_end);
    kernel_printf("Kernel size: %d KB\n", (kernel_end - kernel_start) / 1024);
    kernel_printf("Page directory address: 0x%x\n", manager->kernel_directory);
    kernel_printf("Total physical memory: %d KB\n", memupper_global);
    kernel_printf("Page frame manager start address: 0x%x\n", frame_manager_start);
    kernel_printf("Page frame manager size: %d KB\n", frame_manager_size / 1024);
    kernel_printf("Available page frames: %d\n", frame_manager->free_frames);
    
    
    // Map kernel space
    kernel_printf("Starting to map kernel space...\n");
    uint32_t mapped_pages = 0;
    for (uint32_t virt = kernel_start; virt < kernel_end; virt += PAGE_SIZE) {
        // Identity mapping for kernel space
        uint32_t phys = virt;
        if (pd_map_page(manager->kernel_directory, virt, phys, PTE_PRESENT | PTE_WRITABLE) == 0) {
            mapped_pages++;
        }
    }
    kernel_printf("Kernel space mapping completed, mapped pages: %d\n", mapped_pages);
    
    
    // Switch to kernel page directory
    kernel_printf("Switching to kernel page directory...\n");
    pd_switch(manager->kernel_directory);
    kernel_printf("Current page directory address: 0x%x\n", get_cr3());
    
    while(1);

    // Enable paging
    kernel_printf("Enabling paging mechanism...\n");
    enable_paging();
    kernel_printf("Paging status: Enabled (CR0: 0x%x)\n", get_cr0());
    
    kernel_printf("Virtual Memory Manager initialized successfully\n");
}

// 分配虚拟内存页面
int vmm_allocate_pages(PageDirectory* directory, uint32_t virtual_address, uint32_t size, uint32_t flags) {
    if (!directory) {
        return -1;
    }
    
    // 确保虚拟地址和大小是页对齐的
    virtual_address = virtual_address & PAGE_MASK;
    size = (size + PAGE_SIZE - 1) & PAGE_MASK;
    
    // 分配页面
    for (uint32_t i = 0; i < size; i += PAGE_SIZE) {
        uint32_t phys = pfm_allocate_frame();
        if (!phys) {
            // 分配失败，释放已分配的页面
            for (uint32_t j = 0; j < i; j += PAGE_SIZE) {
                pd_unmap_page(directory, virtual_address + j);
            }
            return -1;
        }
        
        if (pd_map_page(directory, virtual_address + i, phys, flags) != 0) {
            // 映射失败，释放已分配的页面
            pfm_free_frame(phys);
            for (uint32_t j = 0; j < i; j += PAGE_SIZE) {
                pd_unmap_page(directory, virtual_address + j);
            }
            return -1;
        }
    }
    
    return 0;
}

// 释放虚拟内存页面
int vmm_free_pages(PageDirectory* directory, uint32_t virtual_address, uint32_t size) {
    if (!directory) {
        return -1;
    }
    
    // 确保虚拟地址和大小是页对齐的
    virtual_address = virtual_address & PAGE_MASK;
    size = (size + PAGE_SIZE - 1) & PAGE_MASK;
    
    // 释放页面
    for (uint32_t i = 0; i < size; i += PAGE_SIZE) {
        pd_unmap_page(directory, virtual_address + i);
    }
    
    return 0;
}

// 映射物理内存到虚拟地址
int vmm_map_pages(PageDirectory* directory, uint32_t virtual_address, uint32_t physical_address, uint32_t size, uint32_t flags) {
    if (!directory || !physical_address) {
        return -1;
    }
    
    // 确保地址和大小是页对齐的
    virtual_address = virtual_address & PAGE_MASK;
    physical_address = physical_address & PAGE_MASK;
    size = (size + PAGE_SIZE - 1) & PAGE_MASK;
    
    // 映射页面
    for (uint32_t i = 0; i < size; i += PAGE_SIZE) {
        if (pd_map_page(directory, virtual_address + i, physical_address + i, flags) != 0) {
            // 映射失败，释放已映射的页面
            for (uint32_t j = 0; j < i; j += PAGE_SIZE) {
                pd_unmap_page(directory, virtual_address + j);
            }
            return -1;
        }
    }
    
    return 0;
}

// 解除物理内存映射
int vmm_unmap_pages(PageDirectory* directory, uint32_t virtual_address, uint32_t size) {
    // 直接调用vmm_free_pages
    return vmm_free_pages(directory, virtual_address, size);
}

// 创建内存区域
MemoryRegion* vmm_create_memory_region(uint32_t virtual_address, uint32_t size, uint32_t flags, MemoryRegionType type) {
    MemoryRegion* region = (MemoryRegion*)malloc(sizeof(MemoryRegion));
    if (!region) {
        return NULL;
    }
    
    region->virtual_address = virtual_address;
    region->physical_address = 0; // 由具体映射决定
    region->size = size;
    region->flags = flags;
    region->type = type;
    region->next = NULL;
    
    return region;
}

// 销毁内存区域
void vmm_destroy_memory_region(MemoryRegion* region) {
    if (region) {
        free(region);
    }
}

// 文件定位模式常量
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// 模拟文件定位函数（当前VFS不支持）
static int vfs_lseek(int fd, int offset, int whence) {
    // 简化实现，实际系统中应该根据文件系统具体实现
    kernel_printf("vfs_lseek called with fd=%d, offset=%d, whence=%d\n", fd, offset, whence);
    return 0; // 总是返回0，不执行实际操作
}

// 内存映射文件
int vmm_map_file(PageDirectory* directory, const char* filename, uint32_t virtual_address, uint32_t flags) {
    // 这里实现简化版，实际系统中应该打开文件并按需加载页面
    kernel_printf("Memory mapping file: %s\n", filename);
    
    // 打开文件获取大小
    int fd = vfs_open(filename, O_RDONLY);
    if (fd < 0) {
        kernel_printf("Failed to open file: %s\n", filename);
        return -1;
    }
    
    // 模拟获取文件大小（因为当前VFS不支持lseek）
    uint32_t file_size = 4096; // 假设文件大小为1页
    
    // 关闭文件
    vfs_close(fd);
    
    // 分配足够的页面
    uint32_t size = (file_size + PAGE_SIZE - 1) & PAGE_MASK;
    return vmm_allocate_pages(directory, virtual_address, size, flags);
}

// 解除内存映射文件
int vmm_unmap_file(PageDirectory* directory, uint32_t virtual_address) {
    // 这里实现简化版，实际系统中应该释放相关资源
    kernel_printf("Unmapping file at address 0x%x\n", virtual_address);
    
    // 假设每个文件映射至少一个页面
    return vmm_free_pages(directory, virtual_address, PAGE_SIZE);
}