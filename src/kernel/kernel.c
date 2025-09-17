#include <stdtype.h>
#include <kernel/kerio.h>
#include <kernel/gdt.h>
#include <kernel/interrupt/interrupt.h>
#include <kernel/pic.h>
#include <kernel/memory/malloc.h>
#include <kernel/multitask/process.h>
#include <driver/driver.h>
#include <driver/block.h>
#include <driver/keyboard.h>
#include <fs/vfs.h>
#include <fs/devfs.h>
#include <fs/ext4.h>
#include <kernel/syscall/syscall.h>

#include <user/shell/shell.h>

// 全局变量用于存储硬件自检结果
extern int system_hardware_status;

// 全局变量用于存储驱动器状态
int drive_exists = 0;

typedef void (*constructor)();

constructor start_ctors;
constructor end_ctors;

void call_constructors()
{
    for (constructor *ctor = &start_ctors; ctor != &end_ctors; ++ctor)
    {
        (*ctor)();
    }
}

// 全局变量用于存储硬件自检结果
int system_hardware_status = 0;

extern int drive_exists;

extern int vfs_mount_status;

void init_user_mode(){

    // 初始化系统调用
    kernel_printf("Initializing system calls...\n");
    syscall_init();

    // 检查是否需要启动安装程序
    // 这里简化处理，实际可以从启动参数中获取
    extern int installer_main(int argc, char** argv);
    
    // 检查是否存在安装标志（这里简单模拟）
    // 实际系统中，可以通过检查特定的内存位置或启动参数来确定
    uint32_t* install_flag = (uint32_t*)0x9000; // 使用一个特定的内存位置
    
    // 如果设置了安装标志，或者检测到需要格式化（文件系统挂载失败），则启动安装程序
    if (*install_flag == 0x12345678 || system_hardware_status == 2) {
        // 启动安装程序
        kernel_printf("Starting installer...\n");
        create_process("installer", installer_main, 0, NULL, KERNEL_MODE, 1);
    } else {
        // 创建shell进程
        kernel_printf("Creating shell process...\n");
        create_process("shell", shell_main, 0, NULL, KERNEL_MODE, 1);
    }
}

typedef struct Core{
    GDT gdt;
    TaskStateSegment tss; // 添加TSS结构体
    PICController pic_controller;
    InterruptManager interrupt_manager;

    MemoryManager memory_manager;
    VirtualMemoryManager virtual_memory_manager;
    DriverManager driver_manager; // 修改为结构体而不是指针
    ProcessManager process_manager;
} Core;

uint32_t memupper_global;
size_t memory_size;

int on_init_core(Core* core,void *multiboot_structure, unsigned int magic_number)
{
    if(!core) return -1;
    // 初始化GDT
    on_init_gdt(&core->gdt);

    on_init_pic_controller(&core->pic_controller);
    on_init_interrupt_manager(&core->interrupt_manager, &core->gdt);
    on_init_driver_manager(&core->driver_manager); // 传递结构体指针

    // 先初始化内存管理器，因为任务管理器需要分配内存
    size_t heap = 10 * 1024 * 1024; // 从10MB开始
    uint32_t *memupper = (uint32_t *)((size_t)multiboot_structure + 8);
    
    // 添加调试信息
    kernel_printf("Multiboot structure address: %x\n", multiboot_structure);
    kernel_printf("memupper address: %x\n", memupper);
    kernel_printf("memupper value: %d KB\n", *memupper);
    kernel_printf("Total memory: %d MB\n", (*memupper) / 1024);
    memupper_global = *memupper;
    
    // 计算可用内存大小 - 正确的公式应该是从heap开始到memupper*1024的所有内存
    memory_size = (*memupper) * 1024 - heap;
    kernel_printf("Heap start: %x\n", heap);
    kernel_printf("Heap size: %d bytes\n", memory_size);
    kernel_printf("Memory manager will manage memory from %x to %x\n", heap, heap + memory_size);

    on_init_memory_manager(&core->memory_manager, heap, memory_size);
    
    // 初始化虚拟内存管理器
    uint32_t kernel_start = 0x0100000;
    uint32_t kernel_end = heap;       // 内核结束于堆的起始位置
    //on_init_virtual_memory_manager(&core->virtual_memory_manager, kernel_start, kernel_end);

    process_manager_init(&core->process_manager, &core->gdt);

    select_drivers(&core->pic_controller, &core->interrupt_manager, &core->driver_manager);

    kernel_printf("block device number:%d\n", num_block_devices);
    return 0;
}

void activate(Core* core){
    // 统一激活所有驱动
    driver_activate_all(&core->driver_manager);
    activate_interrupt_manager(&core->interrupt_manager);
}

void kernel_main(void *multiboot_structure, unsigned int magic_number)
{
    Core core;

    on_init_core(&core,multiboot_structure, magic_number);    
    
    // 初始化虚拟文件系统
    vfs_init();
    kernel_printf("VFS initialized\n");
    
    // 初始化设备文件系统
    devfs_init();
    kernel_printf("devfs initialized\n");
    
    // 挂载devfs到/dev目录
    if (devfs_mount("/dev") == 0) {
        kernel_printf("devfs mounted at /dev\n");
    } else {
        kernel_printf("Failed to mount devfs\n");
    }
    
    // 初始化EXT4文件系统
    if (ext4_init() == 0) {
        kernel_printf("EXT4 file system initialized\n");
    } else {
        kernel_printf("Failed to initialize EXT4 file system\n");
    }
    
    // 硬件自检
    kernel_printf("\n=== Jarvis OS Hardware Check ===\n");
    kernel_printf("Checking IDE device...\n");
    drive_exists = ide_check_drive_exists();
    
    if (drive_exists) {
        kernel_printf("IDE device detected\n");
        // 初始化IDE块设备
        BlockDevice *ide_device = (BlockDevice*)malloc(sizeof(BlockDevice));
        if (ide_device) {
            block_device_initialize(ide_device, 0x1F0, 14);
            // 将设备添加到活动设备列表
            active_block_devices[num_block_devices++] = ide_device;
            kernel_printf("IDE block device initialized successfully\n");
            
            // 等待设备初始化完成
            kernel_printf("Waiting for block device to be ready...\n");
            for (int i = 0; i < 1000000; i++) asm volatile ("nop");
            
            // 尝试挂载EXT4文件系统到根目录
            int mount_result = vfs_mount("/dev/hda0", "/", "ext4");
            if (mount_result == 0) {
                kernel_printf("EXT4 file system mounted successfully on root directory\n");
                system_hardware_status = 1; // 硬件正常，文件系统已挂载
            } else {
                kernel_printf("Warning: Failed to mount EXT4 file system, formatting may be required\n");
                system_hardware_status = 2; // 硬件正常，但需要格式化
            }
        } else {
            kernel_printf("Failed to allocate memory for IDE device\n");
            system_hardware_status = 0; // 硬件检测失败
        }
    } else {
        kernel_printf("No IDE device detected\n");
        system_hardware_status = 0; // 硬件检测失败
    }
    
    kernel_printf("Hardware check complete\n");

    // 通过驱动管理器统一管理所有驱动
    // 键盘驱动已在create_keyboard_driver函数中自动注册到devfs
    Driver *keyboard_driver = create_keyboard_driver(&core.interrupt_manager);
    if (keyboard_driver)
    {
        append_driver(&core.driver_manager, keyboard_driver);
    }
    
    activate(&core);

    // 系统硬件抽象层初始化结束

    init_user_mode();
    
    
    while (1);

    free(keyboard_driver);
}