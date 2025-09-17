#ifndef OS_KERNEL_MULTITASK_PROCESS_H
#define OS_KERNEL_MULTITASK_PROCESS_H

#include <stdtype.h>
#include <kernel/gdt.h>
#include <kernel/memory/paging.h>

// 进程相关常量定义
#define PROCESS_MAX_COUNT 64          // 最大进程数量
#define KERNEL_STACK_SIZE 4096        // 内核栈大小
#define USER_STACK_SIZE 8192          // 用户栈大小
#define USER_STACK_BASE 0x80000000    // 用户栈基地址（虚拟地址空间上限）
#define MAX_PRIORITY_LEVELS 16        // 优先级队列数量
#define TIME_SLICE_BASE 10            // 基础时间片（毫秒）
#define DEFAULT_PRIORITY 8            // 默认优先级

// 进程状态定义
typedef enum {
    PROCESS_CREATED,    // 已创建
    PROCESS_READY,      // 就绪状态
    PROCESS_RUNNING,    // 运行状态
    PROCESS_BLOCKED,    // 阻塞状态
    PROCESS_WAITING,    // 等待状态
    PROCESS_TERMINATED  // 终止状态
} ProcessState;

// 特权级别定义 - 如果未定义则定义
#ifndef PRIVILEGE_LEVEL_DEFINED
typedef enum {
    KERNEL_MODE = 0,    // 内核模式（Ring 0）
    USER_MODE = 3       // 用户模式（Ring 3）
} PrivilegeLevel;
#define PRIVILEGE_LEVEL_DEFINED
#endif

// CPU寄存器状态结构 - 如果未定义则定义
#ifndef REGISTER_STATE_DEFINED
struct RegisterState {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp;
    uint32_t error, eip, cs, eflags, esp, ss;
} __attribute__((packed));
#define REGISTER_STATE_DEFINED
#endif

// 进程控制块(PCB)结构
typedef struct Process {
    uint32_t pid;                      // 进程ID
    char name[32];                     // 进程名称
    ProcessState state;                // 进程状态
    PrivilegeLevel privilege;          // 特权级别
    uint32_t priority;                 // 当前优先级
    uint32_t base_priority;            // 基础优先级
    uint32_t time_slice;               // 剩余时间片
    uint32_t total_runtime;            // 总运行时间（毫秒）
    uint32_t wakeup_time;              // 唤醒时间（如果阻塞）
    
    // CPU状态
    struct RegisterState* regs;        // 寄存器状态
    uint32_t* kernel_stack;            // 内核栈
    uint32_t kernel_stack_size;        // 内核栈大小
    uint32_t* user_stack;              // 用户栈
    uint32_t user_stack_size;          // 用户栈大小
    
    // 进程关系
    uint32_t parent_pid;               // 父进程ID
    
    // 虚拟内存相关
    PageDirectory* page_directory;     // 进程页目录
    MemoryRegion* memory_regions;      // 进程内存区域链表
    
    // 参数和退出码
    int argc;                          // 参数数量
    char** argv;                       // 参数数组
    int exit_code;                     // 退出码
    
    // 系统调用相关
    uint32_t syscall_result;           // 系统调用结果
    
    // 调试信息
    uint32_t last_tick;                // 最后运行的tick
    
    // 进程入口点
    int (*entry_point)(int, char**);   // 进程入口函数指针
    
    // 链表指针，用于优先级队列管理
    struct Process* next;              // 指向下一个进程
} Process;

// 进程管理器结构
typedef struct ProcessManager {
    Process* processes[PROCESS_MAX_COUNT]; // 进程数组
    uint8_t process_bitmap[PROCESS_MAX_COUNT / 8]; // 进程存在位图
    uint32_t next_pid;                 // 下一个可用的PID
    
    // 多级优先级队列
    Process* ready_queues[MAX_PRIORITY_LEVELS];
    Process* blocked_queue;            // 阻塞队列
    Process* terminated_queue;         // 终止队列
    
    Process* current_process;          // 当前运行的进程
    uint32_t active_processes;         // 活动进程数量
    
    // 调度相关
    uint32_t system_ticks;             // 系统总tick数
    struct GDT* gdt;                   // 全局描述符表
} ProcessManager;

// 进程管理器接口函数
void process_manager_init(ProcessManager* manager, struct GDT* gdt);
uint32_t create_process(const char* name, int (*entry)(int, char**), int argc, char** argv, PrivilegeLevel privilege, uint32_t priority);
void terminate_process(uint32_t pid, int exit_code);
void block_process(uint32_t pid, uint32_t wait_time);
void unblock_process(uint32_t pid);
void yield_cpu();

// 调度器函数
uint32_t schedule(uint32_t esp);
void process_manager_tick();

// 进程查询和控制
Process* get_process(uint32_t pid);
uint32_t get_current_pid();
void dump_process_info(uint32_t pid);

#endif // OS_KERNEL_MULTITASK_PROCESS_H