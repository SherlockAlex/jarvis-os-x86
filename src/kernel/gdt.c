#include "stdtype.h"
#include "kernel/gdt.h"
#include "kernel/kerio.h"

// 全局TSS变量
TaskStateSegment tss;

// GDT描述符类型常量定义
const uint8_t GDT_CODE_PL0 = 0x9a; // 内核态代码段: 10011010
const uint8_t GDT_DATA_PL0 = 0x92; // 内核态数据段: 10010010
const uint8_t GDT_CODE_PL3 = 0xfa; // 用户态代码段: 11111010
const uint8_t GDT_DATA_PL3 = 0xf2; // 用户态数据段: 11110010
const uint8_t GDT_TSS = 0x8b;       // TSS段描述符类型: 10001011 (32位可用TSS)

// 函数前向声明
void init_segement_descriptor(SegmentDescriptor* descriptor, uint32_t base, uint32_t limit, uint8_t type);
uint32_t get_base(SegmentDescriptor*descriptor);
uint32_t get_limit(SegmentDescriptor*descriptor);
uint16_t get_code_selector(GDT* gdt);
uint16_t get_data_selector(GDT* gdt);
uint16_t get_user_code_selector(GDT* gdt);
uint16_t get_user_data_selector(GDT* gdt);
uint16_t get_tss_selector(GDT* gdt);

// 初始化TSS描述符
void load_tss(uint16_t tss_selector) {
    asm volatile ("ltr %0" : : "r" (tss_selector));
}

// 获取TSS选择器
uint16_t get_tss_selector(GDT* gdt) {
    return ((uint8_t*)&gdt->tss_segment_descriptor - (uint8_t*)gdt) >> 3;
}

void on_init_gdt(GDT*gdt)
{
    init_segement_descriptor(&gdt->null_segment_descriptor,0, 0, 0);
    init_segement_descriptor(&gdt->unused_segment_descriptor,0, 0, 0);
    // 内核态段描述符
    init_segement_descriptor(&gdt->code_segment_descriptor,0, 64 * 1024 * 1024, GDT_CODE_PL0);
    init_segement_descriptor(&gdt->data_segment_descriptor,0, 64 * 1024 * 1024, GDT_DATA_PL0);
    // 用户态段描述符
    init_segement_descriptor(&gdt->user_code_segment_descriptor,0, 64 * 1024 * 1024, GDT_CODE_PL3);
    init_segement_descriptor(&gdt->user_data_segment_descriptor,0, 64 * 1024 * 1024, GDT_DATA_PL3);

    // 初始化TSS结构
    tss.prev_tss = 0;
    tss.ss0 = get_data_selector(gdt);  // 使用内核数据段作为ss0
    tss.esp0 = 0x000A0000;  // 设置内核栈地址
    tss.esp1 = 0;
    tss.ss1 = 0;
    tss.esp2 = 0;
    tss.ss2 = 0;
    tss.cr3 = 0;
    tss.eip = 0;
    tss.eflags = 0;
    tss.eax = 0;
    tss.ecx = 0;
    tss.edx = 0;
    tss.ebx = 0;
    tss.esp = 0;
    tss.ebp = 0;
    tss.esi = 0;
    tss.edi = 0;
    tss.es = get_user_data_selector(gdt);
    tss.cs = get_user_code_selector(gdt);
    tss.ss = get_user_data_selector(gdt);
    tss.ds = get_user_data_selector(gdt);
    tss.fs = get_user_data_selector(gdt);
    tss.gs = get_user_data_selector(gdt);
    tss.ldt = 0;
    tss.trap = 0;
    tss.iomap_base = 0;

    // 初始化TSS描述符，设置正确的基址
    init_segement_descriptor(&gdt->tss_segment_descriptor, (uint32_t)&tss, sizeof(TaskStateSegment) - 1, GDT_TSS);

    uint32_t i[2];
    i[1] = (uint32_t)gdt;
    i[0] = sizeof(GDT) << 16;
    asm volatile("lgdt (%0)": :"p" (((uint8_t *)i) + 2));

    // 加载TSS
    //load_tss(get_tss_selector(gdt));

    printk("initailize the GDT and TSS success\n");
}

void init_segement_descriptor(
    SegmentDescriptor* descriptor,
    uint32_t base
    , uint32_t limit
    , uint8_t type
){
    uint8_t* target = (uint8_t*)descriptor;

    if (limit < 1048576) {
        target[6] = 0x40;
    } else {
        if ((limit & 0xfff) != 0xfff) {
            limit = (limit >> 12) - 1;
        } else {
            limit = limit >> 12;
        }
        target[6] = 0xC0;
    }

    target[0] = limit & 0xff;
    target[1] = (limit >> 8) & 0xff;
    target[6] |= (limit >> 16) & 0xf;

    target[2] = base & 0xff;
    target[3] = (base >> 8) & 0xff;
    target[4] = (base >> 16) & 0xff;
    target[7] = (base >> 24) & 0xff;

    target[5] = type;
}

uint32_t get_base(SegmentDescriptor*descriptor){
    uint8_t* target = (uint8_t*)descriptor;
    uint32_t result = target[7];
    result = (result << 8) + target[4];
    result = (result << 8) + target[3];
    result = (result << 8) + target[2];
    return result;
}

uint32_t get_limit(SegmentDescriptor*descriptor)
{
    uint8_t* target = (uint8_t*)descriptor;
    uint32_t result = target[6] & 0xf;
    result = (result << 8) + target[1];
    result = (result << 8) + target[0];

    if ((target[6] & 0xC0) == 0xC0)
        result = (result << 12) | 0xfff;
    return result;
}

uint16_t get_code_selector(GDT* gdt) {
    return ((uint8_t*)&gdt->code_segment_descriptor - (uint8_t*)gdt) >> 3;
}

uint16_t get_data_selector(GDT* gdt) {
    return ((uint8_t*)&gdt->data_segment_descriptor - (uint8_t*)gdt) >> 3;
}

uint16_t get_user_code_selector(GDT* gdt) {
    return ((uint8_t*)&gdt->user_code_segment_descriptor - (uint8_t*)gdt) >> 3;
}

uint16_t get_user_data_selector(GDT* gdt) {
    return ((uint8_t*)&gdt->user_data_segment_descriptor - (uint8_t*)gdt) >> 3;
}