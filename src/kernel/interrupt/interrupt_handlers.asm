section .text
[bits 32]

; 全局导出函数
global handle_interrrupt_ignore
global handle_exception_0x00
global handle_exception_0x01
global handle_exception_0x02
global handle_exception_0x03
global handle_exception_0x04
global handle_exception_0x05
global handle_exception_0x06
global handle_exception_0x07
global handle_exception_0x08
global handle_exception_0x09
global handle_exception_0x0A
global handle_exception_0x0B
global handle_exception_0x0C
global handle_exception_0x0D
global handle_exception_0x0E
global handle_exception_0x0F
global handle_exception_0x10
global handle_exception_0x11
global handle_exception_0x12
global handle_exception_0x13
global handle_interrupt_request_0x00
global handle_interrupt_request_0x01
global handle_interrupt_request_0x02
global handle_interrupt_request_0x03
global handle_interrupt_request_0x04
global handle_interrupt_request_0x05
global handle_interrupt_request_0x06
global handle_interrupt_request_0x07
global handle_interrupt_request_0x08
global handle_interrupt_request_0x09
global handle_interrupt_request_0x0A
global handle_interrupt_request_0x0B
global handle_interrupt_request_0x0C
global handle_interrupt_request_0x0D
global handle_interrupt_request_0x0E
global handle_interrupt_request_0x0F
global handle_interrupt_request_0x31
global handle_syscall

extern handle_interrrupt
extern page_fault_handler

; 通用中断处理函数模板
%macro INTERRUPT_HANDLER 1
handle_interrupt_request_%1:
    ; 保存所有寄存器
    pushad
    
    ; 保存中断号
    push byte %1
    
    ; 调用C语言处理函数
    call handle_interrrupt
    
    ; 清理栈
    add esp, 1
    
    ; 恢复所有寄存器
    popad
    
    ; 中断返回
    iret
%endmacro

; 通用异常处理函数模板（无错误码）
%macro EXCEPTION_HANDLER_NO_ERROR_CODE 1
handle_exception_%1:
    ; 保存所有寄存器
    pushad
    
    ; 保存中断号
    push byte %1
    
    ; 调用C语言处理函数
    call handle_interrrupt
    
    ; 清理栈
    add esp, 1
    
    ; 恢复所有寄存器
    popad
    
    ; 中断返回
    iret
%endmacro

; 通用异常处理函数模板（有错误码）
%macro EXCEPTION_HANDLER_WITH_ERROR_CODE 1
handle_exception_%1:
    ; 保存所有寄存器（错误码已经被压入栈）
    pushad
    
    ; 保存中断号
    push byte %1
    
    ; 调用C语言处理函数
    call handle_interrrupt
    
    ; 清理栈
    add esp, 1
    
    ; 恢复所有寄存器
    popad
    
    ; 清理错误码
    add esp, 4
    
    ; 中断返回
    iret
%endmacro

; 忽略中断处理函数
handle_interrrupt_ignore:
    iret

; 系统调用处理函数
handle_syscall:
    ; 保存所有寄存器
    pushad
    
    ; 保存中断号
    push byte 0x80
    
    ; 调用C语言处理函数
    call handle_interrrupt
    
    ; 清理栈
    add esp, 1
    
    ; 恢复所有寄存器
    popad
    
    ; 中断返回
    iret

; 页面错误异常处理函数（特殊处理，因为需要获取CR2寄存器的值）
handle_exception_0x0E:
    ; 保存所有寄存器
    pushad
    
    ; 保存中断号
    push byte 0x0E
    
    ; 获取错误码（已经被压入栈）
    push dword [esp + 10*4] ; 跳过pushad的8个寄存器、中断号和返回地址
    
    ; 调用页面错误处理函数
    call page_fault_handler
    
    ; 清理栈（错误码）
    add esp, 4
    
    ; 清理栈（中断号）
    add esp, 1
    
    ; 恢复所有寄存器
    popad
    
    ; 清理错误码
    add esp, 4
    
    ; 中断返回
    iret

; 实现其他异常处理函数（无错误码）
EXCEPTION_HANDLER_NO_ERROR_CODE 0x00
EXCEPTION_HANDLER_NO_ERROR_CODE 0x01
EXCEPTION_HANDLER_NO_ERROR_CODE 0x02
EXCEPTION_HANDLER_NO_ERROR_CODE 0x03
EXCEPTION_HANDLER_NO_ERROR_CODE 0x04
EXCEPTION_HANDLER_NO_ERROR_CODE 0x05
EXCEPTION_HANDLER_NO_ERROR_CODE 0x06
EXCEPTION_HANDLER_NO_ERROR_CODE 0x07
EXCEPTION_HANDLER_NO_ERROR_CODE 0x09
EXCEPTION_HANDLER_NO_ERROR_CODE 0x0F
EXCEPTION_HANDLER_NO_ERROR_CODE 0x10
EXCEPTION_HANDLER_NO_ERROR_CODE 0x11
EXCEPTION_HANDLER_NO_ERROR_CODE 0x12
EXCEPTION_HANDLER_NO_ERROR_CODE 0x13

; 实现其他异常处理函数（有错误码）
EXCEPTION_HANDLER_WITH_ERROR_CODE 0x08
EXCEPTION_HANDLER_WITH_ERROR_CODE 0x0A
EXCEPTION_HANDLER_WITH_ERROR_CODE 0x0B
EXCEPTION_HANDLER_WITH_ERROR_CODE 0x0C
EXCEPTION_HANDLER_WITH_ERROR_CODE 0x0D

; 实现中断请求处理函数
INTERRUPT_HANDLER 0x00
INTERRUPT_HANDLER 0x01
INTERRUPT_HANDLER 0x02
INTERRUPT_HANDLER 0x03
INTERRUPT_HANDLER 0x04
INTERRUPT_HANDLER 0x05
INTERRUPT_HANDLER 0x06
INTERRUPT_HANDLER 0x07
INTERRUPT_HANDLER 0x08
INTERRUPT_HANDLER 0x09
INTERRUPT_HANDLER 0x0A
INTERRUPT_HANDLER 0x0B
INTERRUPT_HANDLER 0x0C
INTERRUPT_HANDLER 0x0D
INTERRUPT_HANDLER 0x0E
INTERRUPT_HANDLER 0x0F
INTERRUPT_HANDLER 0x31