#include <kernel/shell/shell.h>
#include <stdtype.h>
#include <kernel/string.h>
#include <kernel/shell/shell.h>
#include <kernel/kerio.h>
#include <kernel/multitask/process.h>
#include <kernel/memory/malloc.h>
#include <fs/vfs.h>
#include <driver/keyboard.h>

// 自定义实现需要的字符串函数
static char* shell_strtok(char* str, const char* delim) {
    static char* last = NULL;
    char* token;
    
    if (str == NULL) {
        str = last;
    }
    
    if (str == NULL) {
        return NULL;
    }
    
    // 跳过前导分隔符
    while (*str && strchr(delim, *str)) {
        str++;
    }
    
    if (*str == '\0') {
        last = NULL;
        return NULL;
    }
    
    token = str;
    
    // 找到下一个分隔符
    while (*str && !strchr(delim, *str)) {
        str++;
    }
    
    if (*str) {
        *str = '\0';
        last = str + 1;
    } else {
        last = NULL;
    }
    
    return token;
}

static char* shell_strrchr(const char* s, int c) {
    const char* last = NULL;
    
    while (*s) {
        if (*s == c) {
            last = s;
        }
        s++;
    }
    
    return (char*)last;
}

// 全局shell状态
ShellState g_shell_state;

// 内置命令表
static ShellCommand builtin_commands[] = {
    {"help", shell_cmd_help, "显示帮助信息"},
    {"exit", shell_cmd_exit, "退出shell"},
    {"echo", shell_cmd_echo, "显示文本"},
    {"ls", shell_cmd_ls, "列出目录内容"},
    {"cd", shell_cmd_cd, "改变当前目录"},
    {"pwd", shell_cmd_pwd, "显示当前目录"},
    {"clear", shell_cmd_clear, "清屏"},
    {"cat", shell_cmd_cat, "查看文件内容"},
    {"mkdir", shell_cmd_mkdir, "创建目录"},
    {"rmdir", shell_cmd_rmdir, "删除目录"},
    {"touch", shell_cmd_touch, "创建空文件"},
    {"rm", shell_cmd_rm, "删除文件"},
    {"ps", shell_cmd_ps, "显示进程状态"},
};

// Shell主函数
int shell_main(int argc, char** argv) {
    kernel_printf("Starting shell...\n");
    
    // 初始化shell
    shell_init();
    
    // 运行shell
    shell_run();
    
    return 0;
}

// 初始化shell
void shell_init() {
    // 初始化shell状态
    strcpy(g_shell_state.prompt, "#> ");
    strcpy(g_shell_state.current_directory, "/");
    g_shell_state.commands = builtin_commands;
    g_shell_state.num_commands = sizeof(builtin_commands) / sizeof(ShellCommand);
    
    // 打印欢迎信息
    kernel_printf("Jarvis OS Shell\n");
    kernel_printf("Type 'help' for available commands\n");
    kernel_printf("\n");
}

// 运行shell
void shell_run() {
    char command_line[MAX_COMMAND_LENGTH];
    
    while (1) {
        // 打印提示符
        shell_print_prompt();
        
        // 读取命令行
        if (shell_read_line(command_line, MAX_COMMAND_LENGTH) < 0) {
            continue;
        }
        
        // 处理命令
        shell_process_command(command_line);
    }
}

// 处理命令行
void shell_process_command(const char* command_line) {
    int argc;
    char** argv;
    
    // 解析命令行
    shell_tokenize((char*)command_line, &argc, &argv);
    
    // 如果没有命令，直接返回
    if (argc == 0) {
        return;
    }
    
    // 执行命令
    shell_execute_command(argc, argv);
    
    // 释放参数数组
    free(argv);
}

// 执行命令
int shell_execute_command(int argc, char** argv) {
    // 查找内置命令
    for (int i = 0; i < g_shell_state.num_commands; i++) {
        if (strcmp(argv[0], g_shell_state.commands[i].name) == 0) {
            // 执行内置命令
            return g_shell_state.commands[i].handler(argc, argv);
        }
    }
    
    // 外部命令（简化实现）
    kernel_printf("Command not found: %s\n", argv[0]);
    return -1;
}

// 打印提示符
void shell_print_prompt() {
    kernel_printf("%s", g_shell_state.current_directory);
    kernel_printf("%s", g_shell_state.prompt);
}

// 读取一行输入
int shell_read_line(char* buffer, int max_length) {
    int i = 0;
    char c;
    
    while (i < max_length - 1) {
        // 从键盘读取一个字符
        c = keyboard_getchar();
        
        if (c == '\n') { // 回车键
            buffer[i] = '\0'; 
            kernel_printf("\n");
            return i;
        } else if (c == 0x08) { // 退格键
            if (i > 0) {
                i--;
                kernel_printf("\b");
            }
        } else if (c >= 0x20 && c <= 0x7E) { // 可打印字符
            buffer[i] = c;
            i++;
            kernel_printf("%c", c);
        }
    }
    
    buffer[i] = '\0';
    return i;
}

// 解析命令行参数
void shell_tokenize(char* line, int* argc, char*** argv) {
    char* token;
    char** args;
    int count = 0;
    
    // 分配参数数组
    args = malloc(MAX_ARGS * sizeof(char*));
    if (!args) {
        *argc = 0;
        *argv = NULL;
        return;
    }
    
    // 解析第一个参数
    token = shell_strtok(line, " ");
    while (token && count < MAX_ARGS) {
        args[count] = token;
        count++;
        token = shell_strtok(NULL, " ");
    }
    
    *argc = count;
    *argv = args;
}

// 内置命令实现

// help命令
int shell_cmd_help(int argc, char** argv) {
    kernel_printf("Available commands:\n");
    
    for (int i = 0; i < g_shell_state.num_commands; i++) {
        kernel_printf("  %-10s %s\n", 
                     g_shell_state.commands[i].name, 
                     g_shell_state.commands[i].help);
    }
    
    return 0;
}

// exit命令
int shell_cmd_exit(int argc, char** argv) {
    kernel_printf("Exiting shell...\n");
    
    // 调用exit系统调用
    asm volatile (
        "movl $1, %eax\n"
        "movl $0, %ebx\n"
        "int $0x80\n"
    );
    
    return 0;
}

// echo命令
int shell_cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        kernel_printf("%s", argv[i]);
        if (i < argc - 1) {
            kernel_printf(" ");
        }
    }
    kernel_printf("\n");
    
    return 0;
}

// ls命令（简化实现）
int shell_cmd_ls(int argc, char** argv) {
    kernel_printf("ls: Not fully implemented\n");
    // 实际实现需要通过VFS接口读取目录内容
    return 0;
}

// cd命令
int shell_cmd_cd(int argc, char** argv) {
    if (argc != 2) {
        kernel_printf("Usage: cd directory\n");
        return -1;
    }
    
    // 简化实现，实际需要检查目录是否存在
    if (strcmp(argv[1], "~") == 0 || strcmp(argv[1], "..") == 0 || strcmp(argv[1], ".") == 0 || strncmp(argv[1], "/", 1) == 0) {
        // 处理特殊路径
        if (strcmp(argv[1], "~") == 0) {
            strcpy(g_shell_state.current_directory, "/");
        } else if (strcmp(argv[1], "..") == 0) {
            // 返回上一级目录
            // 简化实现，实际需要解析路径
            if (strcmp(g_shell_state.current_directory, "/") != 0) {
                char* last_slash = shell_strrchr(g_shell_state.current_directory, '/');
                if (last_slash && last_slash != g_shell_state.current_directory) {
                    *last_slash = '\0';
                } else {
                    strcpy(g_shell_state.current_directory, "/");
                }
            }
        } else if (strcmp(argv[1], ".") == 0) {
            // 当前目录，无需操作
        } else {
            // 绝对路径
            strcpy(g_shell_state.current_directory, argv[1]);
        }
    } else {
        // 相对路径
        if (strcmp(g_shell_state.current_directory, "/") == 0) {
            snprintf(g_shell_state.current_directory, 256, "/%s", argv[1]);
        } else {
            snprintf(g_shell_state.current_directory, 256, "%s/%s", g_shell_state.current_directory, argv[1]);
        }
    }
    
    return 0;
}

// pwd命令
int shell_cmd_pwd(int argc, char** argv) {
    kernel_printf("%s\n", g_shell_state.current_directory);
    return 0;
}

// clear命令
int shell_cmd_clear(int argc, char** argv) {
    // 清屏
    kernel_printf("\033[2J\033[H");
    return 0;
}

// cat命令（简化实现）
int shell_cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        kernel_printf("Usage: cat file\n");
        return -1;
    }
    
    kernel_printf("cat: Not fully implemented\n");
    // 实际实现需要通过VFS接口读取文件内容
    return 0;
}

// mkdir命令（简化实现）
int shell_cmd_mkdir(int argc, char** argv) {
    if (argc != 2) {
        kernel_printf("Usage: mkdir directory\n");
        return -1;
    }
    
    kernel_printf("mkdir: Not fully implemented\n");
    // 实际实现需要通过VFS接口创建目录
    return 0;
}

// rmdir命令（简化实现）
int shell_cmd_rmdir(int argc, char** argv) {
    if (argc != 2) {
        kernel_printf("Usage: rmdir directory\n");
        return -1;
    }
    
    kernel_printf("rmdir: Not fully implemented\n");
    // 实际实现需要通过VFS接口删除目录
    return 0;
}

// touch命令（简化实现）
int shell_cmd_touch(int argc, char** argv) {
    if (argc != 2) {
        kernel_printf("Usage: touch file\n");
        return -1;
    }
    
    kernel_printf("touch: Not fully implemented\n");
    // 实际实现需要通过VFS接口创建文件
    return 0;
}

// rm命令（简化实现）
int shell_cmd_rm(int argc, char** argv) {
    if (argc != 2) {
        kernel_printf("Usage: rm file\n");
        return -1;
    }
    
    kernel_printf("rm: Not fully implemented\n");
    // 实际实现需要通过VFS接口删除文件
    return 0;
}

// ps命令（显示进程状态）
int shell_cmd_ps(int argc, char** argv) {
    kernel_printf("  PID  PRIO  STATE  NAME\n");
    kernel_printf("-----  ----  -----  ----\n");
    
    // 显示当前进程信息
    // 简化实现，实际需要遍历所有进程
    uint32_t current_pid = get_current_pid();
    Process* current_process = get_process(current_pid);
    
    if (current_process) {
        const char* state_str = "RUNNING";
        kernel_printf("%5d  %4d  %5s  %s\n", 
                     current_pid, 
                     current_process->priority, 
                     state_str, 
                     current_process->name);
    }
    
    kernel_printf("ps: Only showing current process\n");
    return 0;
}