#ifndef OS_KERNEL_SHELL
#define OS_KERNEL_SHELL

#include <stdtype.h>
#include <kernel/syscall/syscall.h>
#include <kernel/string.h>

// 命令行最大长度
#define MAX_COMMAND_LENGTH 256

// 参数最大数量
#define MAX_ARGS 16

// Shell命令结构
typedef struct {
    const char* name;
    int (*handler)(int argc, char** argv);
    const char* help;
} ShellCommand;

// Shell状态结构
typedef struct {
    char prompt[32];
    char current_directory[256];
    ShellCommand* commands;
    int num_commands;
} ShellState;

// Shell函数原型
extern int shell_main(int argc, char** argv);
extern void shell_init();
extern void shell_run();
extern void shell_process_command(const char* command_line);
extern int shell_execute_command(int argc, char** argv);
extern void shell_print_prompt();
extern int shell_read_line(char* buffer, int max_length);
extern void shell_tokenize(char* line, int* argc, char*** argv);


// Shell内置命令处理函数
extern int shell_cmd_help(int argc, char** argv);
extern int shell_cmd_exit(int argc, char** argv);
extern int shell_cmd_echo(int argc, char** argv);
extern int shell_cmd_ls(int argc, char** argv);
extern int shell_cmd_cd(int argc, char** argv);
extern int shell_cmd_pwd(int argc, char** argv);
extern int shell_cmd_clear(int argc, char** argv);
extern int shell_cmd_cat(int argc, char** argv);
extern int shell_cmd_mkdir(int argc, char** argv);
extern int shell_cmd_rmdir(int argc, char** argv);
extern int shell_cmd_touch(int argc, char** argv);
extern int shell_cmd_rm(int argc, char** argv);
extern int shell_cmd_ps(int argc, char** argv);
extern int shell_cmd_memory(int argc, char** argv);

extern ShellState g_shell_state;

#endif // OS_KERNEL_SHELL