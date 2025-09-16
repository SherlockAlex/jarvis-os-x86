#ifndef OS_KERNEL_SYSCALL
#define OS_KERNEL_SYSCALL

#include <stdtype.h>
#include <kernel/multitask/process.h>
#include <fs/vfs.h>

// 系统调用号定义
#define SYS_exit       1
#define SYS_read       3
#define SYS_write      4
#define SYS_open       5
#define SYS_close      6
#define SYS_ioctl      16
#define SYS_fork       2
#define SYS_execve     11
#define SYS_waitpid    7
#define SYS_getpid     20
#define SYS_sbrk       45
#define SYS_yield      64

// 系统调用表项类型定义
typedef int (*syscall_handler)(uint32_t arg1, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5);

// 系统调用初始化函数
extern void syscall_init();

// 系统调用处理函数
extern int syscall_handler_exit(uint32_t exit_code, uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4);
extern int syscall_handler_read(uint32_t fd, uint32_t buf, uint32_t count, uint32_t unused1, uint32_t unused2);
extern int syscall_handler_write(uint32_t fd, uint32_t buf, uint32_t count, uint32_t unused1, uint32_t unused2);
extern int syscall_handler_open(uint32_t pathname, uint32_t flags, uint32_t unused1, uint32_t unused2, uint32_t unused3);
extern int syscall_handler_close(uint32_t fd, uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4);
extern int syscall_handler_ioctl(uint32_t fd, uint32_t request, uint32_t argp, uint32_t unused1, uint32_t unused2);
extern int syscall_handler_fork(uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4, uint32_t unused5);
extern int syscall_handler_execve(uint32_t pathname, uint32_t argv, uint32_t envp, uint32_t unused1, uint32_t unused2);
extern int syscall_handler_waitpid(uint32_t pid, uint32_t status, uint32_t options, uint32_t unused1, uint32_t unused2);
extern int syscall_handler_getpid(uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4, uint32_t unused5);
extern int syscall_handler_sbrk(uint32_t increment, uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4);
extern int syscall_handler_yield(uint32_t unused1, uint32_t unused2, uint32_t unused3, uint32_t unused4, uint32_t unused5);

// 系统调用入口点，在中断处理中调用
extern uint32_t handle_syscall_interrupt(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi);

#endif // OS_KERNEL_SYSCALL