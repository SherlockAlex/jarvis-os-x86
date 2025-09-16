#ifndef OS_KERNEL_GDT
#define OS_KERNEL_GDT

#include "stdtype.h"

typedef struct SegmentDescriptor
{
    uint16_t limit_lo;
    uint16_t base_lo;
    uint8_t base_hi;
    uint8_t type;
    uint8_t flags_limit_hi;
    uint8_t base_vhi;

} SegmentDescriptor;__attribute__((packed))

// 任务状态段(TSS)结构
typedef struct TaskStateSegment
{
    uint32_t prev_tss;    // 前一个TSS的选择器
    uint32_t esp0;        // 特权级0的栈指针
    uint32_t ss0;         // 特权级0的栈段选择器
    uint32_t esp1;        // 特权级1的栈指针
    uint32_t ss1;         // 特权级1的栈段选择器
    uint32_t esp2;        // 特权级2的栈指针
    uint32_t ss2;         // 特权级2的栈段选择器
    uint32_t cr3;         // 页目录基址
    uint32_t eip;         // 指令指针
    uint32_t eflags;      // 标志寄存器
    uint32_t eax;         // 通用寄存器
    uint32_t ecx;         // 通用寄存器
    uint32_t edx;         // 通用寄存器
    uint32_t ebx;         // 通用寄存器
    uint32_t esp;         // 栈指针
    uint32_t ebp;         // 基址指针
    uint32_t esi;         // 源索引
    uint32_t edi;         // 目标索引
    uint32_t es;          // 段寄存器
    uint32_t cs;          // 段寄存器
    uint32_t ss;          // 段寄存器
    uint32_t ds;          // 段寄存器
    uint32_t fs;          // 段寄存器
    uint32_t gs;          // 段寄存器
    uint32_t ldt;         // LDT选择器
    uint16_t trap;        // 陷阱标志
    uint16_t iomap_base;  // I/O映射基址
} TaskStateSegment;__attribute__((packed))

typedef struct GDT
{
    SegmentDescriptor null_segment_descriptor;
    SegmentDescriptor unused_segment_descriptor;
    SegmentDescriptor code_segment_descriptor;        // 内核态代码段
    SegmentDescriptor data_segment_descriptor;        // 内核态数据段
    SegmentDescriptor user_code_segment_descriptor;   // 用户态代码段
    SegmentDescriptor user_data_segment_descriptor;   // 用户态数据段
    SegmentDescriptor tss_segment_descriptor;         // 任务状态段描述符
} GDT;__attribute__((packed))

// TSS类型常量
extern const uint8_t GDT_TSS; // TSS段描述符类型

// GDT描述符类型常量
extern const uint8_t GDT_CODE_PL0; // 内核态代码段
extern const uint8_t GDT_DATA_PL0; // 内核态数据段
extern const uint8_t GDT_CODE_PL3; // 用户态代码段
extern const uint8_t GDT_DATA_PL3; // 用户态数据段

extern void on_init_gdt(GDT*);
extern uint16_t get_code_selector(GDT*);
extern uint16_t get_data_selector(GDT*);
extern uint16_t get_user_code_selector(GDT*);
extern uint16_t get_user_data_selector(GDT*);
extern uint16_t get_tss_selector(GDT*);
extern void load_tss(uint16_t tss_selector);

#endif