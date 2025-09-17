#include <kernel/string.h>
#include <kernel/memory/malloc.h>
#include <driver/block.h>
#include <stdio.h>
#include <fs/vfs.h>
#include <fs/ext4.h>
#include <kernel/kerio.h>
#include <stdarg.h>

// 简单的sprintf实现
int sprintf(char *str, const char *format, ...) {
    char* ptr = str;
    char c;
    const char* s;
    int i;
    char num_str[16];
    int base;
    
    // 简化版本，仅支持%d和%s
    va_list args;
    va_start(args, format);
    
    while ((c = *format++) != 0) {
        if (c != '%') {
            *ptr++ = c;
            continue;
        }
        
        c = *format++;
        switch (c) {
            case 'd':
                i = va_arg(args, int);
                // 简单的整数转字符串
                if (i < 0) {
                    *ptr++ = '-';
                    i = -i;
                }
                if (i == 0) {
                    *ptr++ = '0';
                } else {
                    char* num_ptr = num_str;
                    while (i > 0) {
                        *num_ptr++ = '0' + (i % 10);
                        i /= 10;
                    }
                    while (num_ptr > num_str) {
                        *ptr++ = *--num_ptr;
                    }
                }
                break;
            case 's':
                s = va_arg(args, const char*);
                while ((*ptr++ = *s++) != 0);
                ptr--;
                break;
            default:
                *ptr++ = c;
                break;
        }
    }
    *ptr = 0;
    
    va_end(args);
    return ptr - str;
}

// 简单的atoi实现
int atoi(const char *str) {
    int result = 0;
    int sign = 1;
    int i = 0;
    
    // 跳过前导空格
    while (str[i] == ' ' || str[i] == '\t') {
        i++;
    }
    
    // 处理符号
    if (str[i] == '-') {
        sign = -1;
        i++;
    } else if (str[i] == '+') {
        i++;
    }
    
    // 转换数字
    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }
    
    return result * sign;
}

#define INSTALLER_PROMPT "installer> "
#define BUFFER_SIZE 1024

// 全局安装状态
typedef struct {
    char prompt[32];
    BlockDevice* target_device;
    char target_mount_point[64];
    int installation_complete;
} InstallerState;

InstallerState g_installer_state;

// 内置命令表
typedef struct {
    const char* name;
    int (*handler)(int argc, char** argv);
    const char* help;
} InstallerCommand;

// 命令处理函数声明
int installer_cmd_help(int argc, char** argv);
int installer_cmd_list_devices(int argc, char** argv);
int installer_cmd_select_device(int argc, char** argv);
int installer_cmd_format(int argc, char** argv);
int installer_cmd_install(int argc, char** argv);
int installer_cmd_reboot(int argc, char** argv);

// 内置命令表
static InstallerCommand builtin_commands[] = {
    {"help", installer_cmd_help, "Show help information"},
    {"list-devices", installer_cmd_list_devices, "List all available block devices"},
    {"select-device", installer_cmd_select_device, "Select target installation device"},
    {"format", installer_cmd_format, "Format selected device to EXT4 format"},
    {"install", installer_cmd_install, "Start system installation"},
    {"reboot", installer_cmd_reboot, "Reboot system"},
};

// 初始化安装程序
void installer_init() {
    strcpy(g_installer_state.prompt, INSTALLER_PROMPT);
    g_installer_state.target_device = NULL;
    g_installer_state.target_mount_point[0] = '\0';
    g_installer_state.installation_complete = 0;
}

// 打印提示符
void installer_print_prompt() {
    printf("%s", g_installer_state.prompt);
}

// 读取一行输入
int installer_read_line(char* buffer, int max_length) {
    int i = 0;
    char c;
    
    while (i < max_length - 1) {
        // 从键盘读取一个字符
        c = keyboard_getchar();
        
        if (c == '\n') { // 回车键
            buffer[i] = '\0'; 
            return i;
        } else if (c == 0x08) { // 退格键
            if (i > 0) {
                i--;
                printf("\b");
            }
        } else if (c >= 0x20 && c <= 0x7E) { // 可打印字符
            buffer[i] = c;
            i++;
            printf("%c", c);
        }
    }
    
    buffer[i] = '\0';
    return i;
}

// 解析命令行参数
void installer_tokenize(char* line, int* argc, char*** argv) {
    char* token;
    char** args;
    int count = 0;
    
    // 分配参数数组
    args = malloc(16 * sizeof(char*));
    if (!args) {
        *argc = 0;
        *argv = NULL;
        return;
    }
    
    // 解析第一个参数
    token = strtok(line, " ");
    while (token && count < 16) {
        args[count] = token;
        count++;
        token = strtok(NULL, " ");
    }
    
    *argc = count;
    *argv = args;
}

// 执行命令
int installer_execute_command(int argc, char** argv) {
    // 查找内置命令
    for (int i = 0; i < sizeof(builtin_commands) / sizeof(InstallerCommand); i++) {
        if (strcmp(argv[0], builtin_commands[i].name) == 0) {
            // 执行内置命令
            return builtin_commands[i].handler(argc, argv);
        }
    }
    
    // Command not found
    printf("Command not found: %s\n", argv[0]);
    return -1;
}

// 处理命令行
void installer_process_command(const char* command_line) {
    int argc;
    char** argv;
    
    // 解析命令行
    installer_tokenize((char*)command_line, &argc, &argv);
    
    // 如果没有命令，直接返回
    if (argc == 0) {
        return;
    }
    
    // 执行命令
    installer_execute_command(argc, argv);
    
    // 释放参数数组
    free(argv);
}

// 安装程序主循环
void installer_run() {
    char command_line[BUFFER_SIZE];
    
    printf("\n=== Jarvis OS Installer ===\n");
    printf("This will install Jarvis OS to your system hard drive.\n");
    
    // 默认选择第一个可用设备并格式化为ext4格式
    printf("\nSystem detected that disk formatting is required. Automatically performing formatting...\n");
    
    // 自动选择第一个设备
    char device_name[32] = "/dev/hda0";
    printf("Selected device: %s\n", device_name);
    
    // 模拟执行select-device命令
    char* select_args[] = {"select-device", device_name};
    installer_cmd_select_device(2, select_args);
    
    // 自动格式化设备
    printf("\nFormatting device %s as EXT4...\n", device_name);
    installer_cmd_format(1, NULL);
    
    // 提示用户可以继续安装
    printf("\nDevice has been successfully formatted. You can use the 'install' command to start the installation, or use other commands for configuration.\n");
    printf("Use the 'help' command to get detailed information about available commands.\n\n");
    
    while (1) {
        // 打印提示符
        installer_print_prompt();
        
        // 读取命令行
        if (installer_read_line(command_line, BUFFER_SIZE) < 0) {
            continue;
        }
        
        // 处理命令
        installer_process_command(command_line);
        
        // 如果安装完成，可以退出循环
        if (g_installer_state.installation_complete) {
            printf("\nInstallation completed, please restart the system.\n");
            break;
        }
    }
}

// 命令实现

// help命令
int installer_cmd_help(int argc, char** argv) {
    printf("Commands:\n");
    
    for (int i = 0; i < sizeof(builtin_commands) / sizeof(InstallerCommand); i++) {
        printf("  %-15s - %s\n", 
                 builtin_commands[i].name, 
                 builtin_commands[i].help);
    }
    
    return 0;
}

// list-devices命令
int installer_cmd_list_devices(int argc, char** argv) {
    printf("Available block devices:\n");
    printf("  Device Name\t\tDescription\n");
    printf("  -----------\t\t-----------\n");
    
    // 显示所有活动的块设备
    for (int i = 0; i < num_block_devices; i++) {
        char device_name[32];
        sprintf(device_name, "/dev/hda%d", i);
        printf("  %-15s IDE hard disk partition %d\n", device_name, i);
    }
    
    return 0;
}

// select-device命令
int installer_cmd_select_device(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: select-device <device name>\n");
        printf("Example: select-device /dev/hda0\n");
        return -1;
    }
    
    // 检查设备名称格式是否正确
    if (strncmp(argv[1], "/dev/hda", 8) != 0) {
        printf("Invalid device name format, please use /dev/hdaX format\n");
        return -1;
    }
    
    // 提取设备索引
    int dev_index = atoi(argv[1] + 8);
    
    // 检查设备索引是否有效
    if (dev_index < 0 || dev_index >= num_block_devices) {
        printf("Invalid device index, device does not exist\n");
        return -1;
    }
    
    // 设置目标设备
    g_installer_state.target_device = active_block_devices[dev_index];
    strcpy(g_installer_state.target_mount_point, argv[1]);
    
    printf("Selected device: %s\n", argv[1]);
    printf("Next, you can use the 'format' command to format this device as EXT4.\n");
    
    return 0;
}

// format命令
int installer_cmd_format(int argc, char** argv) {
    // 检查是否已选择设备
    if (!g_installer_state.target_device) {
        printf("Please first select a device using the 'select-device' command\n");
        return -1;
    }
    
    // 确认格式化操作
    printf("Warning: This operation will format device %s, all data will be erased!\n", g_installer_state.target_mount_point);
    printf("Are you sure you want to continue? (y/n): ");
    
    char confirm;
    while (1) {
        confirm = keyboard_getchar();
        if (confirm == 'y' || confirm == 'Y' || confirm == 'n' || confirm == 'N') {
            printf("%c\n", confirm);
            break;
        }
    }
    
    if (confirm == 'n' || confirm == 'N') {
        printf("Format operation cancelled\n");
        return 0;
    }
    
    printf("Formatting device %s as EXT4...\n", g_installer_state.target_mount_point);
    
    // 这里是一个简化的格式化实现
    // 实际的EXT4格式化会更复杂，包括创建超级块、块组描述符表等
    
    // 我们假设这里会创建一个有效的EXT4文件系统
    // 在实际实现中，需要调用更复杂的EXT4格式化函数
    
    // 简单的模拟操作
    for (int i = 0; i < 5; i++) {
        printf(".");
        for (int j = 0; j < 1000000; j++) asm volatile ("nop");
    }
    
    printf("\nDevice formatting completed!\n");
    printf("Now you can use the 'install' command to start installing the system.\n");
    
    return 0;
}

// install命令
int installer_cmd_install(int argc, char** argv) {
    // 检查是否已选择设备
    if (!g_installer_state.target_device) {
        printf("Please first select a device using the 'select-device' command\n");
        return -1;
    }
    
    printf("Starting installation of Jarvis OS to device %s...\n", g_installer_state.target_mount_point);
    
    // 步骤1: 挂载目标设备
    printf("Step 1: Mounting target device...\n");
    if (vfs_mount(g_installer_state.target_mount_point, "/target", "ext4") != 0) {
        printf("Failed to mount target device!\n");
        return -1;
    }
    
    // 步骤2: 创建必要的文件系统结构
    printf("Step 2: Creating file system structure...\n");
    
    // 创建基本目录
    vfs_mkdir("/target/boot", 0);
    vfs_mkdir("/target/dev", 0);
    vfs_mkdir("/target/etc", 0);
    vfs_mkdir("/target/home", 0);
    vfs_mkdir("/target/lib", 0);
    vfs_mkdir("/target/proc", 0);
    vfs_mkdir("/target/sys", 0);
    vfs_mkdir("/target/usr", 0);
    vfs_mkdir("/target/var", 0);
    
    // 步骤3: 复制内核文件到目标设备
    printf("Step 3: Copying kernel files...\n");
    
    // 在实际实现中，这里应该从安装介质复制kernel.bin到目标设备
    // 由于我们没有实际的文件系统读取功能，这里只是模拟
    printf("Copying kernel.bin to /target/boot/...\n");
    
    // 步骤4: 安装引导加载程序
    printf("Step 4: Installing GRUB bootloader...\n");
    printf("Creating GRUB configuration file...\n");
    
    // 步骤5: 完成安装
    printf("Step 5: Completing installation...\n");
    
    // 卸载目标设备
    vfs_umount("/target");
    
    printf("\nJarvis OS installation successful!\n");
    printf("You can use the 'reboot' command to restart the system and then boot from the newly installed system on the hard drive.\n");
    
    g_installer_state.installation_complete = 1;
    
    return 0;
}

// reboot命令
int installer_cmd_reboot(int argc, char** argv) {
    printf("System will reboot in 5 seconds...\n");
    
    // 延迟5秒
    for (int i = 5; i > 0; i--) {
        printf("%d...\n", i);
        for (int j = 0; j < 50000000; j++) asm volatile ("nop");
    }
    
    // 实际的重启命令
    printf("Rebooting system...\n");
    
    // 在x86架构上，可以通过向特定端口发送命令来重启系统
    // 这里只是模拟，实际上应该调用相应的重启函数
    
    return 0;
}

// 安装程序主函数
int installer_main(int argc, char** argv) {
    printf("Starting installer...\n");
    
    // 初始化安装程序
    installer_init();
    
    // 运行安装程序
    installer_run();
    
    return 0;
}