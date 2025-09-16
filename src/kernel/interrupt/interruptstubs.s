.set IRQ_BASE, 0x20
.set SYSCALL_INT, 0x80
.section .text
.extern _handle_interrrupt
.extern _handle_syscall_interrupt

.macro HandleInterruptRequest num
.global _handle_interrupt_request_\num
_handle_interrupt_request_\num\():
    movb $\num + IRQ_BASE, (interruptnumber)
    pushl $0
    jmp int_bottom
.endm


.macro HandleException num
.global _handle_exception_\num
_handle_exception_\num:
    movb $\num, (interruptnumber)
    jmp int_bottom
.endm

HandleInterruptRequest 0x00
HandleInterruptRequest 0x01
HandleInterruptRequest 0x02
HandleInterruptRequest 0x03
HandleInterruptRequest 0x04
HandleInterruptRequest 0x05
HandleInterruptRequest 0x06
HandleInterruptRequest 0x07
HandleInterruptRequest 0x08
HandleInterruptRequest 0x09
HandleInterruptRequest 0x0A
HandleInterruptRequest 0x0B
HandleInterruptRequest 0x0C
HandleInterruptRequest 0x0D
HandleInterruptRequest 0x0E
HandleInterruptRequest 0x0F
HandleInterruptRequest 0x31

HandleException 0x00
HandleException 0x01
HandleException 0x02
HandleException 0x03
HandleException 0x04
HandleException 0x05
HandleException 0x06
HandleException 0x07
HandleException 0x08
HandleException 0x09
HandleException 0x0A
HandleException 0x0B
HandleException 0x0C
HandleException 0x0D
HandleException 0x0E
HandleException 0x0F
HandleException 0x10
HandleException 0x11
HandleException 0x12
HandleException 0x13


.global _handle_syscall
_handle_syscall:
    cli
    pushal
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    push %esp
    call _handle_syscall_interrupt
    add $4, %esp
    
    
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popal
    
    add $4, %esp
    
    sti
    iret


int_bottom:
    pushl %ebp
    pushl %edi
    pushl %esi

    pushl %edx
    pushl %ecx 
    pushl %ebx 
    pushl %eax 

    pushl %esp
    push (interruptnumber)
    call _handle_interrrupt

    movl %eax, %esp
    
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx 

    popl %esi
    popl %edi
    popl %ebp

    add $4, %esp

.global _handle_interrrupt_ignore
_handle_interrrupt_ignore:

    iret

.data
    interruptnumber: .byte 0
