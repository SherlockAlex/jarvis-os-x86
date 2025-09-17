#include <kernel/kerio.h>
#include <kernel/memory/malloc.h>
#include <kernel/multitask/process.h>
#include <kernel/string.h>
#include <stdbool.h>

// 全局进程管理器指针
ProcessManager* process_manager = NULL;

// 工具函数：查找空闲PID
static uint32_t find_free_pid(ProcessManager* manager) {
    for (uint32_t i = 0; i < PROCESS_MAX_COUNT; i++) {
        uint32_t byte_index = i / 8;
        uint32_t bit_index = i % 8;
        if (!(manager->process_bitmap[byte_index] & (1 << bit_index))) {
            return i;
        }
    }
    return -1; // 没有可用PID
}

// 工具函数：设置PID在位图中的状态
static void set_pid_in_use(ProcessManager* manager, uint32_t pid, bool in_use) {
    if (pid < PROCESS_MAX_COUNT) {
        uint32_t byte_index = pid / 8;
        uint32_t bit_index = pid % 8;
        if (in_use) {
            manager->process_bitmap[byte_index] |= (1 << bit_index);
        } else {
            manager->process_bitmap[byte_index] &= ~(1 << bit_index);
        }
    }
}

// 工具函数：将进程添加到优先级队列
static void enqueue_process(Process** queue, Process* process) {
    if (!*queue) {
        *queue = process;
        process->next = NULL;
        return;
    }
    
    // 按PID排序插入，便于管理
    Process* current = *queue;
    Process* prev = NULL;
    
    while (current && current->pid < process->pid) {
        prev = current;
        current = current->next;
    }
    
    if (prev) {
        prev->next = process;
    } else {
        *queue = process;
    }
    process->next = current;
}

// 工具函数：从队列中移除进程
static Process* dequeue_process(Process** queue) {
    if (!*queue) {
        // 如果队列为空，返回NULL
        //kernel_printf("dequeue_process: Queue is empty\n");
        return NULL;
    }
    
    Process* front = *queue;
    *queue = front->next;
    front->next = NULL;
    return front;
}

// 工具函数：从队列中查找并移除特定进程
static Process* remove_process_from_queue(Process** queue, uint32_t pid) {
    if (!*queue) {
        return NULL;
    }
    
    Process* current = *queue;
    Process* prev = NULL;
    
    while (current && current->pid != pid) {
        prev = current;
        current = current->next;
    }
    
    if (!current) {
        return NULL; // 未找到
    }
    
    if (prev) {
        prev->next = current->next;
    } else {
        *queue = current->next;
    }
    current->next = NULL;
    return current;
}

// 进程包装函数：处理任务入口和退出
static void process_wrapper() {
    // 获取当前进程
    Process* current = process_manager->current_process;
    if (!current || !current->entry_point) {
        kernel_printf("Process wrapper: Invalid process or entry point\n");
        terminate_process(current->pid, -1);
        return;
    }
    
    // 调用进程入口函数
    int exit_code = current->entry_point(current->argc, current->argv);
    
    // 进程退出
    terminate_process(current->pid, exit_code);
    
    // 不应该到达这里
    while (1) {
        asm volatile("hlt");
    }
}

// 初始化进程管理器
void process_manager_init(ProcessManager* manager, struct GDT* gdt) {
    memset(manager, 0, sizeof(ProcessManager));
    memset(manager->process_bitmap, 0, sizeof(manager->process_bitmap));
    
    manager->next_pid = 1; // PID从1开始，0保留
    manager->gdt = gdt;
    manager->current_process = NULL;
    manager->active_processes = 0;
    manager->system_ticks = 0;
    
    // 初始化所有队列
    for (int i = 0; i < MAX_PRIORITY_LEVELS; i++) {
        manager->ready_queues[i] = NULL;
    }
    manager->blocked_queue = NULL;
    manager->terminated_queue = NULL;
    
    process_manager = manager;
    
    kernel_printf("Process manager initialized successfully\n");
}

// 创建新进程
uint32_t create_process(const char* name, int (*entry)(int, char**), int argc, char** argv, PrivilegeLevel privilege, uint32_t priority) {
    if (!process_manager || !entry) {
        return -1;
    }
    
    // 查找空闲PID
    uint32_t pid = find_free_pid(process_manager);
    if (pid == (uint32_t)-1) {
        kernel_printf("No available PID for new process\n");
        return -1;
    }
    
    // 分配进程控制块
    Process* process = (Process*)malloc(sizeof(Process));
    if (!process) {
        kernel_printf("Failed to allocate memory for process\n");
        return -1;
    }
    memset(process, 0, sizeof(Process));
    
    // 初始化进程信息
    process->pid = pid;
    strncpy(process->name, name ? name : "unnamed", sizeof(process->name) - 1);
    process->state = PROCESS_CREATED;
    process->privilege = privilege;
    process->base_priority = priority > MAX_PRIORITY_LEVELS - 1 ? MAX_PRIORITY_LEVELS - 1 : priority;
    process->priority = process->base_priority;
    process->time_slice = TIME_SLICE_BASE * (MAX_PRIORITY_LEVELS - process->priority);
    process->total_runtime = 0;
    process->wakeup_time = 0;
    process->parent_pid = process_manager->current_process ? process_manager->current_process->pid : 0;
    process->argc = argc;
    process->argv = argv;
    process->exit_code = 0;
    process->memory_regions = NULL;
    
    // 创建进程页目录
    process->page_directory = pd_create();
    if (!process->page_directory) {
        kernel_printf("Failed to create page directory\n");
        free(process);
        return -1;
    }
    
    // 分配并初始化内核栈
    process->kernel_stack_size = KERNEL_STACK_SIZE;
    process->kernel_stack = malloc(process->kernel_stack_size);
    if (!process->kernel_stack) {
        kernel_printf("Failed to allocate kernel stack\n");
        pd_destroy(process->page_directory);
        free(process);
        return -1;
    }
    
    if (privilege == USER_MODE) {
        // 为用户态进程分配用户栈
        process->user_stack_size = USER_STACK_SIZE;
        
        // 用户栈在虚拟地址空间的底部
        uint32_t user_stack_virtual = USER_STACK_BASE - process->user_stack_size;
        
        // 分配并映射用户栈
        if (vmm_allocate_pages(process->page_directory, user_stack_virtual, process->user_stack_size, 
                              PTE_PRESENT | PTE_WRITABLE | PTE_USER) != 0) {
            kernel_printf("Failed to allocate user stack\n");
            pd_destroy(process->page_directory);
            free(process->kernel_stack);
            free(process);
            return -1;
        }
        
        // 记录用户栈内存区域
        MemoryRegion* stack_region = vmm_create_memory_region(user_stack_virtual, 
                                                           process->user_stack_size, 
                                                           PTE_PRESENT | PTE_WRITABLE | PTE_USER, 
                                                           MEMORY_STACK);
        if (stack_region) {
            stack_region->next = process->memory_regions;
            process->memory_regions = stack_region;
        }
        
        // 保存用户栈的虚拟地址
        process->user_stack = (uint32_t*)user_stack_virtual;
    }
    
    // 设置寄存器状态
    process->regs = (struct RegisterState*)((uint8_t*)process->kernel_stack + process->kernel_stack_size - sizeof(struct RegisterState));
    memset(process->regs, 0, sizeof(struct RegisterState));
    
    // 设置进程入口点和参数
    process->entry_point = entry;
    process->regs->eax = (uint32_t)entry;
    process->regs->ebx = argc;
    process->regs->ecx = (uint32_t)argv;
    
    // 设置EIP为进程包装函数
    process->regs->eip = (uint32_t)process_wrapper;
    process->regs->eflags = 0x202; // 启用中断
    process->regs->esp = (uint32_t)process->regs;
    
    // 设置段选择器
    if (privilege == USER_MODE) {
        // 用户态进程使用用户态段选择器
        process->regs->cs = (get_user_code_selector(process_manager->gdt) << 3) | (privilege & 3);
        process->regs->ss = (get_user_data_selector(process_manager->gdt) << 3) | (privilege & 3);
    } else {
        // 内核态进程使用内核态段选择器
        process->regs->cs = (get_code_selector(process_manager->gdt) << 3) | (privilege & 3);
        process->regs->ss = (get_data_selector(process_manager->gdt) << 3) | (privilege & 3);
    }
    
    // 将进程添加到进程数组和就绪队列
    process_manager->processes[pid] = process;
    set_pid_in_use(process_manager, pid, true);
    process_manager->active_processes++;
    
    // 标记为就绪状态并添加到就绪队列
    process->state = PROCESS_READY;
    enqueue_process(&process_manager->ready_queues[process->priority], process);
    
    kernel_printf("Created process %s (PID: %d, priority: %d)\n", 
                 process->name, process->pid, process->priority);
    
    return pid;
}

// 终止进程
void terminate_process(uint32_t pid, int exit_code) {
    if (!process_manager || pid >= PROCESS_MAX_COUNT) {
        return;
    }
    
    Process* process = process_manager->processes[pid];
    if (!process) {
        return;
    }
    
    // 设置进程状态为终止
    process->state = PROCESS_TERMINATED;
    process->exit_code = exit_code;
    
    // 从当前队列中移除
    if (process->state == PROCESS_READY) {
        remove_process_from_queue(&process_manager->ready_queues[process->priority], pid);
    } else if (process->state == PROCESS_BLOCKED) {
        remove_process_from_queue(&process_manager->blocked_queue, pid);
    } else if (process == process_manager->current_process) {
        process_manager->current_process = NULL;
    }
    
    // 添加到终止队列
    enqueue_process(&process_manager->terminated_queue, process);
    
    kernel_printf("Process %s (PID: %d) terminated with code %d\n", 
                 process->name, process->pid, exit_code);
    
    // 触发调度
    if (process == process_manager->current_process) {
        asm volatile("int $0x20");
    }
}

// 阻塞进程
void block_process(uint32_t pid, uint32_t wait_time) {
    if (!process_manager || pid >= PROCESS_MAX_COUNT) {
        return;
    }
    
    Process* process = process_manager->processes[pid];
    if (!process || process->state != PROCESS_READY && process != process_manager->current_process) {
        return;
    }
    
    // 从当前队列中移除
    if (process->state == PROCESS_READY) {
        remove_process_from_queue(&process_manager->ready_queues[process->priority], pid);
    } else if (process == process_manager->current_process) {
        process_manager->current_process = NULL;
    }
    
    // 设置为阻塞状态
    process->state = PROCESS_BLOCKED;
    process->wakeup_time = process_manager->system_ticks + wait_time;
    
    // 添加到阻塞队列
    enqueue_process(&process_manager->blocked_queue, process);
    
    // 触发调度
    if (process == process_manager->current_process) {
        asm volatile("int $0x20");
    }
}

// 解除进程阻塞
void unblock_process(uint32_t pid) {
    if (!process_manager || pid >= PROCESS_MAX_COUNT) {
        return;
    }
    
    Process* process = process_manager->processes[pid];
    if (!process || process->state != PROCESS_BLOCKED) {
        return;
    }
    
    // 从阻塞队列中移除
    remove_process_from_queue(&process_manager->blocked_queue, pid);
    
    // 恢复为就绪状态
    process->state = PROCESS_READY;
    
    // 添加到就绪队列（根据优先级）
    enqueue_process(&process_manager->ready_queues[process->priority], process);
}

// 主动让出CPU
void yield_cpu() {
    if (!process_manager || !process_manager->current_process) {
        return;
    }
    
    // 强制触发调度
    asm volatile("int $0x20");
}

// 进程调度器
uint32_t schedule(uint32_t esp) {
    if (!process_manager) {
        return esp;
    }
    
    // 保存当前进程状态
    if (process_manager->current_process && process_manager->current_process->state == PROCESS_RUNNING) {
        process_manager->current_process->regs = (struct RegisterState*)esp;
        
        // 如果时间片未用完，放回就绪队列
        if (process_manager->current_process->time_slice > 0) {
            process_manager->current_process->state = PROCESS_READY;
            enqueue_process(&process_manager->ready_queues[process_manager->current_process->priority], 
                          process_manager->current_process);
        } else {
            // 时间片用完，根据调度策略调整优先级
            if (process_manager->current_process->priority < MAX_PRIORITY_LEVELS - 1) {
                process_manager->current_process->priority++;
            }
            // 重置时间片
            process_manager->current_process->time_slice = TIME_SLICE_BASE * (MAX_PRIORITY_LEVELS - process_manager->current_process->priority);
            process_manager->current_process->state = PROCESS_READY;
            enqueue_process(&process_manager->ready_queues[process_manager->current_process->priority], 
                          process_manager->current_process);
        }
    }

    
    
    // 查找下一个要运行的进程（从高优先级到低优先级）
    Process* next_process = NULL;
    for (int i = 0; i < MAX_PRIORITY_LEVELS && !next_process; i++) {
        next_process = dequeue_process(&process_manager->ready_queues[i]);
    }
    
    // 如果没有就绪进程，返回当前esp
    if (!next_process) {
        
        process_manager->current_process = NULL;
        return esp;
    }
    
    // 设置为运行状态
    next_process->state = PROCESS_RUNNING;
    process_manager->current_process = next_process;
    
    // 切换到新进程的页目录
    if (next_process->page_directory) {
        pd_switch(next_process->page_directory);
    }
    
    return (uint32_t)next_process->regs;
}

// 进程管理器时间tick处理
void process_manager_tick() {
    if (!process_manager) {
        return;
    }
    
    // 更新系统tick计数
    process_manager->system_ticks++;
    
    // 减少当前进程的时间片
    if (process_manager->current_process && process_manager->current_process->state == PROCESS_RUNNING) {
        process_manager->current_process->time_slice--;
        process_manager->current_process->total_runtime++;
        
        // 如果时间片用完，触发调度
        if (process_manager->current_process->time_slice <= 0) {
            asm volatile("int $0x20");
        }
    }
    
    // 检查阻塞队列中的进程是否需要唤醒
    Process* current = process_manager->blocked_queue;
    while (current) {
        Process* next = current->next;
        if (current->wakeup_time > 0 && process_manager->system_ticks >= current->wakeup_time) {
            unblock_process(current->pid);
        }
        current = next;
    }
    
    // 定期清理终止的进程
    if (process_manager->system_ticks % 100 == 0) {
        current = process_manager->terminated_queue;
        while (current) {
            Process* to_free = current;
            current = current->next;
            
            // 释放资源
            if (to_free->user_stack) {
                free(to_free->user_stack);
            }
            free(to_free->kernel_stack);
            free(to_free);
            
            // 从进程数组中移除
            process_manager->processes[to_free->pid] = NULL;
            set_pid_in_use(process_manager, to_free->pid, false);
            process_manager->active_processes--;
        }
        process_manager->terminated_queue = NULL;
    }
}

// 获取进程
Process* get_process(uint32_t pid) {
    if (!process_manager || pid >= PROCESS_MAX_COUNT) {
        return NULL;
    }
    return process_manager->processes[pid];
}

// 获取当前进程PID
uint32_t get_current_pid() {
    if (!process_manager || !process_manager->current_process) {
        return -1;
    }
    return process_manager->current_process->pid;
}

// 打印进程信息
void dump_process_info(uint32_t pid) {
    Process* process = get_process(pid);
    if (!process) {
        kernel_printf("Process %d not found\n", pid);
        return;
    }
    
    const char* state_str[] = {"CREATED", "READY", "RUNNING", "BLOCKED", "WAITING", "TERMINATED"};
    const char* privilege_str[] = {"KERNEL_MODE", "", "", "USER_MODE"};
    
    kernel_printf("Process Info:\n");
    kernel_printf("  PID: %d\n", process->pid);
    kernel_printf("  Name: %s\n", process->name);
    kernel_printf("  State: %s\n", state_str[process->state]);
    kernel_printf("  Privilege: %s\n", privilege_str[process->privilege]);
    kernel_printf("  Priority: %d (base: %d)\n", process->priority, process->base_priority);
    kernel_printf("  Time Slice: %d\n", process->time_slice);
    kernel_printf("  Total Runtime: %d ticks\n", process->total_runtime);
    kernel_printf("  Parent PID: %d\n", process->parent_pid);
    kernel_printf("  Exit Code: %d\n", process->exit_code);
}