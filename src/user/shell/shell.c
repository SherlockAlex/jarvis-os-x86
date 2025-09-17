#include <user/shell/shell.h>
#include <kernel/string.h>
#include <kernel/memory/malloc.h>
#include <driver/keyboard.h>
#include <stdio.h>

// 全局shell状态
ShellState g_shell_state;

int test_main(){
    printf("test_main %d %d %d %d %d %d %d %d",1,2,3,4,5,6,7,8);
    return 0;
}

// 内置命令表
static ShellCommand builtin_commands[] = {
    {"help", shell_cmd_help, "显示帮助信息"},
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
    {"memory", shell_cmd_memory, "显示内存信息"},
    {"test", test_main, "测试命令"},
};

// Shell主函数
int shell_main(int argc, char** argv) {
    printf("Starting shell...\n");
    
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
    printf("\n");
    printf("Jarvis OS\n");
    printf("Type 'help' for available commands\n");
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
    
    printf("\n");
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
    printf("Command not found: %s\n", argv[0]);
    return -1;
}

// 打印提示符
void shell_print_prompt() {
    printf("\n%s", g_shell_state.current_directory);
    printf("%s", g_shell_state.prompt);
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
    token = strtok(line, " ");
    while (token && count < MAX_ARGS) {
        args[count] = token;
        count++;
        token = strtok(NULL, " ");
    }
    
    *argc = count;
    *argv = args;
}

// 内置命令实现

// help命令
int shell_cmd_help(int argc, char** argv) {
    printf("Available commands:\n");
    
    for (int i = 0; i < g_shell_state.num_commands; i++) {
        printf(" - %s\n", 
                     g_shell_state.commands[i].name, 
                     g_shell_state.commands[i].help);
    }
    
    return 0;
}

// echo命令
int shell_cmd_echo(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) {
            printf(" ");
        }
    }
    printf("\n");
    
    return 0;
}

// ls命令实现
int shell_cmd_ls(int argc, char** argv) {
    printf("ls is not implemented yet.\n");
    return 0;
}

// cd命令实现
int shell_cmd_cd(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: cd directory\n");
        return -1;
    }

    printf("cd is not implemented yet.\n");
    return 0;
}

// pwd命令
int shell_cmd_pwd(int argc, char** argv) {
    printf("%s\n", g_shell_state.current_directory);
    return 0;
}

// clear命令
int shell_cmd_clear(int argc, char** argv) {
    // 清屏
    
    return 0;
}

// cat命令 - 显示文件内容
int shell_cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: cat file\n");
        return -1;
    }
    
    printf("cat is not implemented yet.\n");
    
    return 0;
}

// mkdir命令
int shell_cmd_mkdir(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: mkdir directory\n");
        return -1;
    }
    
    printf("mkdir is not implemented yet.\n");
    
    return 0;
}

// rmdir命令实现
int shell_cmd_rmdir(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: rmdir directory\n");
        return -1;
    }
    
    printf("rmdir is not implemented yet.\n");
    
    return 0;
}

// touch命令 - 创建空文件
int shell_cmd_touch(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: touch file\n");
        return -1;
    }
    
    printf("touch is not implemented yet.\n");
    return 0;
}

// rm命令（简化实现）
int shell_cmd_rm(int argc, char** argv) {
    if (argc != 2) {
        printf("Usage: rm file\n");
        return -1;
    }
    
    printf("rm: Not fully implemented\n");
    // 实际实现需要通过VFS接口删除文件
    return 0;
}

// ps命令（显示进程状态）
int shell_cmd_ps(int argc, char** argv) {
    printf("  PID  PRIO  STATE  NAME\n");
    printf("-----  ----  -----  ----\n");
    
    // 显示当前进程信息
    // 简化实现，实际需要遍历所有进程
    uint32_t current_pid = get_current_pid();
    Process* current_process = get_process(current_pid);
    
    if (current_process) {
        const char* state_str = "RUNNING";
        printf("%5d  %4d  %5s  %s\n", 
                     current_pid, 
                     current_process->priority, 
                     state_str, 
                     current_process->name);
    }
    
    printf("ps: Only showing current process\n");
    return 0;
}


int shell_cmd_memory(int argc, char** argv) {
    printf("Heap size: %d bytes\n", syscall_handler_mm_size());
    return 0;
}