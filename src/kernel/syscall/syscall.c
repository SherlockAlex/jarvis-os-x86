#include <kernel/syscall/syscall.h>
#include <kernel/multitask/process.h>
#include <fs/vfs.h>
#include <kernel/memory/malloc.h>
#include <kernel/string.h>
#include <kernel/kerio.h>
#include <stdbool.h>
#include <kernel/memory/paging.h>
#include <stdtype.h>

// 声明外部的process_manager变量
extern ProcessManager* process_manager;

// 系统调用表
static syscall_handler syscall_table[128];

// 进程文件描述符表大小
#define FD_TABLE_SIZE 64
#define MAX_FILE_DESCRIPTOR_TABLES 64

// 文件描述符表结构
typedef struct {
    FileDescriptor* descriptors[FD_TABLE_SIZE];
    uint32_t pid; // 关联的进程ID
    bool in_use;  // 是否在使用中
} FileDescriptorTable;

// 全局文件描述符表数组
static FileDescriptorTable g_file_descriptor_tables[MAX_FILE_DESCRIPTOR_TABLES];

// 获取当前进程的文件描述符表
static FileDescriptorTable* get_file_descriptor_table() {
    Process* current = process_manager->current_process;
    if (!current) {
        return NULL;
    }
    
    uint32_t pid = current->pid;
    
    // 查找与当前进程关联的文件描述符表
    for (int i = 0; i < MAX_FILE_DESCRIPTOR_TABLES; i++) {
        if (g_file_descriptor_tables[i].in_use && g_file_descriptor_tables[i].pid == pid) {
            return &g_file_descriptor_tables[i];
        }
    }
    
    // 如果没有找到，创建一个新的文件描述符表
    for (int i = 0; i < MAX_FILE_DESCRIPTOR_TABLES; i++) {
        if (!g_file_descriptor_tables[i].in_use) {
            memset(&g_file_descriptor_tables[i], 0, sizeof(FileDescriptorTable));
            g_file_descriptor_tables[i].pid = pid;
            g_file_descriptor_tables[i].in_use = true;
            return &g_file_descriptor_tables[i];
        }
    }
    
    return NULL; // 没有可用的文件描述符表
}

// 分配一个空闲的文件描述符
static int alloc_fd(FileDescriptor* fd) {
    FileDescriptorTable* table = get_file_descriptor_table();
    if (!table) {
        return -1;
    }
    
    for (int i = 0; i < FD_TABLE_SIZE; i++) {
        if (!table->descriptors[i]) {
            table->descriptors[i] = fd;
            return i;
        }
    }
    
    return -1;
}

// 获取文件描述符对应的FileDescriptor结构
static FileDescriptor* get_fd(int fd) {
    FileDescriptorTable* table = get_file_descriptor_table();
    if (!table || fd < 0 || fd >= FD_TABLE_SIZE) {
        return NULL;
    }
    
    return table->descriptors[fd];
}

// 释放文件描述符
static void free_fd(int fd) {
    FileDescriptorTable* table = get_file_descriptor_table();
    if (!table || fd < 0 || fd >= FD_TABLE_SIZE) {
        return;
    }
    
    table->descriptors[fd] = NULL;
}

// exit系统调用：终止当前进程
int syscall_handler_exit(uint32_t exit_code, uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4) {
    uint32_t pid = get_current_pid();
    terminate_process(pid, exit_code);
    return 0; // 不会返回
}

// read系统调用：从文件描述符读取数据
int syscall_handler_read(uint32_t fd, uint32_t buf, uint32_t count, uint32_t unused1, uint32_t unused2) {
    FileDescriptor* file_desc = get_fd(fd);
    if (!file_desc || !file_desc->ops || !file_desc->ops->read) {
        return -1;
    }
    
    // 检查缓冲区是否有效（在用户空间）
    // 简化实现，实际系统中需要检查内存安全性
    
    size_t bytes_read = file_desc->ops->read(file_desc->inode, (void*)buf, count, file_desc->offset);
    if (bytes_read > 0) {
        file_desc->offset += bytes_read;
    }
    
    return bytes_read;
}

// write系统调用：向文件描述符写入数据
int syscall_handler_write(uint32_t fd, uint32_t buf, uint32_t count, uint32_t unused1, uint32_t unused2) {
    FileDescriptor* file_desc = get_fd(fd);
    if (!file_desc || !file_desc->ops || !file_desc->ops->write) {
        return -1;
    }
    
    // 检查缓冲区是否有效（在用户空间）
    // 简化实现，实际系统中需要检查内存安全性
    
    size_t bytes_written = file_desc->ops->write(file_desc->inode, (const void*)buf, count, file_desc->offset);
    if (bytes_written > 0) {
        file_desc->offset += bytes_written;
    }
    
    return bytes_written;
}

// open系统调用：打开文件
int syscall_handler_open(uint32_t pathname, uint32_t flags, uint32_t unused1, uint32_t unused2, uint32_t unused3) {
    // 检查路径名是否有效
    if (!pathname) {
        return -1;
    }
    
    // 调用VFS的open函数
    int fd = vfs_open((const char*)pathname, flags);
    return fd;
}

// close系统调用：关闭文件描述符
int syscall_handler_close(uint32_t fd, uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4) {
    FileDescriptor* file_desc = get_fd(fd);
    if (!file_desc) {
        return -1;
    }
    
    // 调用VFS的close函数
    vfs_close(fd);
    
    // 释放文件描述符
    free_fd(fd);
    
    return 0;
}

// ioctl系统调用：控制设备
int syscall_handler_ioctl(uint32_t fd, uint32_t request, uint32_t argp, uint32_t unused1, uint32_t unused2) {
    FileDescriptor* file_desc = get_fd(fd);
    if (!file_desc || !file_desc->ops || !file_desc->ops->ioctl) {
        return -1;
    }
    
    return file_desc->ops->ioctl(file_desc->inode, request, (void*)argp);
}

// fork系统调用：创建新进程（简化实现）
int syscall_handler_fork(uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4, uint32_t unused5) {
    // 简化实现，实际系统中需要复制进程的内存空间和资源
    kernel_printf("fork system call is not fully implemented yet\n");
    return -1;
}

// execve系统调用：执行新程序（简化实现）
int syscall_handler_execve(uint32_t pathname, uint32_t argv, uint32_t envp, uint32_t unused1, uint32_t unused2) {
    // 简化实现，实际系统中需要加载程序并替换当前进程的内存空间
    kernel_printf("execve system call is not fully implemented yet\n");
    return -1;
}

// waitpid系统调用：等待子进程结束（简化实现）
int syscall_handler_waitpid(uint32_t pid, uint32_t status, uint32_t options, uint32_t unused1, uint32_t unused2) {
    // 简化实现，实际系统中需要等待指定的子进程结束
    kernel_printf("waitpid system call is not fully implemented yet\n");
    return -1;
}

// getpid系统调用：获取当前进程ID
int syscall_handler_getpid(uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4, uint32_t unused5) {
    return get_current_pid();
}

// sbrk系统调用：调整进程的堆大小
int syscall_handler_sbrk(uint32_t increment, uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4) {
    // 简化实现，实际系统中需要调整进程的堆空间
    kernel_printf("sbrk system call is not fully implemented yet\n");
    return -1;
}

// yield系统调用：让出CPU
int syscall_handler_yield(uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4, uint32_t unused5) {
    yield_cpu();
    return 0;
}

// 系统调用中断处理函数
extern uint32_t handle_syscall_interrupt(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi) {
    uint32_t syscall_num = eax;
    
    // 检查系统调用号是否有效
    if (syscall_num >= sizeof(syscall_table) / sizeof(syscall_handler)) {
        kernel_printf("Invalid system call number: %d\n", syscall_num);
        return -1;
    }
    
    // 获取系统调用处理函数
    syscall_handler handler = syscall_table[syscall_num];
    if (!handler) {
        kernel_printf("Unimplemented system call: %d\n", syscall_num);
        return -1;
    }
    
    // 调用系统调用处理函数
    int result = handler(ebx, ecx, edx, esi, edi);
    
    // 设置当前进程的系统调用结果
    Process* current = process_manager->current_process;
    if (current) {
        current->syscall_result = result;
    }
    
    return result;
}

// 初始化系统调用
extern void syscall_init() {
    // 初始化系统调用表
    memset(syscall_table, 0, sizeof(syscall_table));
    
    // 注册系统调用处理函数
    syscall_table[SYS_exit] = syscall_handler_exit;
    syscall_table[SYS_read] = syscall_handler_read;
    syscall_table[SYS_write] = syscall_handler_write;
    syscall_table[SYS_open] = syscall_handler_open;
    syscall_table[SYS_close] = syscall_handler_close;
    syscall_table[SYS_ioctl] = syscall_handler_ioctl;
    syscall_table[SYS_fork] = syscall_handler_fork;
    syscall_table[SYS_execve] = syscall_handler_execve;
    syscall_table[SYS_waitpid] = syscall_handler_waitpid;
    syscall_table[SYS_getpid] = syscall_handler_getpid;
    syscall_table[SYS_sbrk] = syscall_handler_sbrk;
    syscall_table[SYS_yield] = syscall_handler_yield;
    syscall_table[SYS_mmap] = syscall_handler_mmap;
    syscall_table[SYS_munmap] = syscall_handler_munmap;
    
    kernel_printf("System call table initialized\n");
}

// mmap系统调用：将文件或匿名内存映射到进程的虚拟地址空间
int syscall_handler_mmap(uint32_t addr, uint32_t len, uint32_t prot, uint32_t flags, uint32_t fd) {
    Process* current = process_manager->current_process;
    if (!current) {
        return -1;
    }

    // 验证参数有效性
    if (len == 0) {
        return -1;
    }

    // 确保长度是页大小的整数倍
    len = (len + PAGE_SIZE - 1) & PAGE_MASK;

    // 检查保护标志的有效性
    uint32_t page_flags = PTE_PRESENT;
    if (prot & PROT_WRITE) {
        page_flags |= PTE_WRITABLE;
    }
    // 添加用户模式标志
    page_flags |= PTE_USER;

    // 处理文件映射或匿名映射
    if (fd != -1 && !(flags & MAP_ANONYMOUS)) {
        // 文件映射
        FileDescriptor* file_desc = get_fd(fd);
        if (!file_desc || !file_desc->inode) {
            return -1;
        }

        // 在实际系统中，需要实现按需加载页面（demand paging）
        // 这里我们只是简单地分配物理内存并映射
        int result = vmm_allocate_pages(current->page_directory, addr, len, page_flags);
        if (result != 0) {
            return -1;
        }

        // 创建内存区域记录
        MemoryRegion* region = vmm_create_memory_region(addr, len, page_flags, MEMORY_MAPPED_FILE);
        if (!region) {
            vmm_free_pages(current->page_directory, addr, len);
            return -1;
        }

        // 将内存区域添加到进程的内存区域链表
        region->next = current->memory_regions;
        current->memory_regions = region;

        return addr;
    } else {
        // 匿名映射
        int result = vmm_allocate_pages(current->page_directory, addr, len, page_flags);
        if (result != 0) {
            return -1;
        }

        // 创建内存区域记录
        MemoryRegion* region = vmm_create_memory_region(addr, len, page_flags, MEMORY_DATA);
        if (!region) {
            vmm_free_pages(current->page_directory, addr, len);
            return -1;
        }

        // 将内存区域添加到进程的内存区域链表
        region->next = current->memory_regions;
        current->memory_regions = region;

        return addr;
    }
}

// munmap系统调用：解除内存映射
int syscall_handler_munmap(uint32_t addr, uint32_t len, uint32_t unused1, uint32_t unused2, uint32_t unused3) {
    Process* current = process_manager->current_process;
    if (!current) {
        return -1;
    }

    // 验证参数有效性
    if (len == 0 || (addr & ~PAGE_MASK) != 0) {
        return -1;
    }

    // 确保长度是页大小的整数倍
    len = (len + PAGE_SIZE - 1) & PAGE_MASK;

    // 解除内存映射
    int result = vmm_free_pages(current->page_directory, addr, len);
    if (result != 0) {
        return -1;
    }

    // 简化实现：从进程的内存区域链表中移除对应的区域
    // 这里应该查找并移除对应的内存区域，但为简化实现，我们省略具体的内存区域链表操作

    return 0;
}

extern uint32_t memory_size;
size_t syscall_handler_mm_size(){
    return memory_size;
}