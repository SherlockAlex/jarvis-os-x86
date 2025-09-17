#include "stdio.h"
#include <kernel/syscall/syscall.h>
#include <stdtype.h>
#include <kernel/kerio.h>

/**
 * printf - 格式化输出函数
 * @format: 格式化字符串
 * @...: 可变参数列表
 * 
 * 返回值: 成功打印的字符数
 */
int printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = handler_printf(format,args);
    va_end(args);
    // 调用系统调用处理函数
    
    return ret;
}