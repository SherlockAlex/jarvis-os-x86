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
    push %eax
    push %ebx

    call _kernel_main

_stop:
    cli
    hlt
    jmp _stop

.section .bss
.space 2*1024*1024
.global _kernel_stack
_kernel_stack:
