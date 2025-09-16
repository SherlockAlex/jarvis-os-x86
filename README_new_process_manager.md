# 高性能进程管理系统设计文档

## 1. 系统概述

本设计提供了一个全新的、高性能的进程管理系统，替代了旧的任务管理系统。新系统采用优先级队列调度算法，支持内核态和用户态进程，提供了完整的进程创建、调度、阻塞、唤醒和终止功能。

## 2. 系统架构

新的进程管理系统包含以下主要组件：

### 2.1 核心数据结构

1. **Process（进程控制块PCB）**：存储每个进程的所有相关信息
2. **ProcessManager（进程管理器）**：管理所有进程和调度
3. **RegisterState（寄存器状态）**：保存进程执行上下文

### 2.2 主要文件

- `include/kernel/multitask/process.h`：定义进程管理系统的核心数据结构和接口
- `src/kernel/multitask/process.c`：实现进程管理系统的核心功能
- `include/kernel/multitask/syscall_adapter.h`：定义系统调用适配层接口
- `src/kernel/multitask/syscall_adapter.c`：实现系统调用适配层功能
- `src/kernel/multitask/usertask_new.c`：新的用户态任务测试代码

## 3. 关键特性

### 3.1 基于优先级的多级队列调度

- 实现了0-3级的优先级队列（优先级0最高，3最低）
- 高优先级任务优先获得CPU时间
- 支持优先级动态调整，防止优先级倒置
- 时间片大小与优先级成反比，高优先级任务获得更长时间片

### 3.2 高效的进程管理

- 使用位图（bitmap）快速分配和查找空闲PID
- 使用链表实现进程队列，支持快速插入和删除
- 定期自动回收终止的进程资源
- 支持内核态和用户态进程的隔离和保护

### 3.3 完整的进程状态管理

支持的进程状态：
- `PROCESS_CREATED`：进程创建但未就绪
- `PROCESS_READY`：进程就绪，等待CPU
- `PROCESS_RUNNING`：进程正在运行
- `PROCESS_BLOCKED`：进程被阻塞
- `PROCESS_WAITING`：进程等待其他资源
- `PROCESS_TERMINATED`：进程已终止

### 3.4 系统调用支持

提供了完整的系统调用适配层，支持：
- 进程创建和终止
- 内存分配和释放
- CPU让出
- 优先级调整
- 进程信息查询

## 4. 使用方法

### 4.1 初始化进程管理器

```c
ProcessManager process_manager;
process_manager_init(&process_manager, &gdt);
```

### 4.2 创建新进程

```c
// 创建内核态进程
create_process("main_task", main_task_func, argc, argv, KERNEL_MODE, 0);

// 创建用户态进程
create_process("user_task", user_task_func, argc, argv, USER_MODE, 3);
```

### 4.3 进程控制操作

```c
// 终止进程
terminate_process(pid, exit_code);

// 阻塞进程
block_process(pid, wait_time);

// 解除进程阻塞
unblock_process(pid);

// 主动让出CPU
yield_cpu();

// 设置进程优先级
syscall_nice(pid, new_priority);
```

### 4.4 查询进程信息

```c
// 获取当前进程PID
get_current_pid();

// 获取特定进程
Process* process = get_process(pid);

// 打印进程信息
dump_process_info(pid);
```

## 5. 与旧系统的区别

### 5.1 架构改进

- 从任务（Task）概念升级为进程（Process）概念，更符合现代操作系统设计
- 引入了优先级队列调度，替代了简单的循环调度
- 使用位图管理PID，提高了PID分配和查找效率
- 增加了更多进程状态，提供更精细的进程控制

### 5.2 性能优化

- 优化了进程调度算法，提高了系统整体响应速度
- 实现了优先级动态调整，改善了高优先级任务的响应性
- 改进了资源管理，减少了内存泄漏风险
- 优化了上下文切换，减少了系统开销

### 5.3 功能增强

- 支持更完整的进程控制功能
- 提供了更详细的进程信息查询接口
- 增强了系统调用支持
- 优化了用户态和内核态进程的隔离

## 6. 测试用例

新系统提供了`user_task_new.c`作为测试用例，包含了：
- 进程创建和初始化
- 系统调用使用
- 内存分配和释放
- CPU让出测试
- 进程退出处理

## 7. 注意事项

- 系统当前支持的最大进程数为256个
- 内核栈大小为8KB，用户栈大小为16KB
- 进程优先级范围为0-3，其中0为最高优先级
- 在用户态进程中，必须使用系统调用进行IO操作和内存管理