.set MAGIC, 0x1BADB002
.set FLAGS, (1<<0 | 1<<1)
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
	.long MAGIC
	.long FLAGS
	.long CHECKSUM

.section .text
.extern _kernel_main
.extern _call_constructors
.global boot

boot:
    mov $_kernel_stack,%esp

    mov %cr0,%eax
    and $0xfffb,%ax
    or $0x22,%ax
    mov %eax,%cr0
    fninit

    call _call_constructors
    
    # 保存multiboot结构和魔术数字
    push %eax
    push %ebx
    
    # 解析启动参数，检查是否有--install标志
    # %ebx包含multiboot信息结构的地址
    # 检查multiboot信息结构中的flags是否包含命令行标志
    mov 8(%ebx), %eax       # 读取flags字段
    test $0x4, %eax         # 检查BIT2是否设置（表示存在命令行）
    jz no_install_flag      # 如果不存在命令行，跳过参数检查
    
    # 获取命令行地址
    mov 16(%ebx), %eax      # 读取cmdline字段
    
    # 搜索"--install"参数
    # 设置安装标志的内存位置
    mov $0x9000, %ecx       # 安装标志的内存位置
    movl $0, (%ecx)         # 默认设置为0
    
    # 简单的字符串搜索逻辑
    movl $0, %esi           # 字符索引
search_loop:
    movb (%eax, %esi), %dl  # 读取当前字符
    cmpb $0, %dl            # 检查是否到达字符串末尾
    je no_install_flag      # 如果到达末尾且没有找到，退出搜索
    
    # 检查是否是"--install"的开始
    cmpb $'-', %dl          # 检查是否是'-'
    jne next_char
    cmpb $'-', 1(%eax, %esi)  # 检查下一个字符是否也是'-'
    jne next_char
    cmpb $'i', 2(%eax, %esi)  # 检查是否是'i'
    jne next_char
    cmpb $'n', 3(%eax, %esi)  # 检查是否是'n'
    jne next_char
    cmpb $'s', 4(%eax, %esi)  # 检查是否是's'
    jne next_char
    cmpb $'t', 5(%eax, %esi)  # 检查是否是't'
    jne next_char
    cmpb $'a', 6(%eax, %esi)  # 检查是否是'a'
    jne next_char
    cmpb $'l', 7(%eax, %esi)  # 检查是否是'l'
    jne next_char
    cmpb $'l', 8(%eax, %esi)  # 检查是否是'l'
    jne next_char
    
    # 如果找到"--install"参数，设置安装标志
    movl $0x12345678, (%ecx)  # 设置安装标志
    
next_char:
    incl %esi               # 增加字符索引
    jmp search_loop         # 继续搜索
    
no_install_flag:
    // 调用内核主函数
    call _kernel_main

_stop:
    cli
    hlt
    jmp _stop

.section .bss
.space 2*1024*1024
.global _kernel_stack
_kernel_stack:
