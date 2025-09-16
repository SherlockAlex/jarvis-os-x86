#ifndef OS_KERNEL_INTERRUPT
#define OS_KERNEL_INTERRUPT

#include <stdtype.h>
#include <kernel/gdt.h>
#include <kernel/multitask/process.h>

#define INTERRUPT_OFFSET 0x20

extern uint32_t handle_interrrupt(
    uint8_t interrupt_number
    , uint32_t esp
);
extern void handle_interrrupt_ignore();
extern void handle_syscall();

extern void handle_interrupt_request_0x00();
extern void handle_interrupt_request_0x01();
extern void handle_interrupt_request_0x02();
extern void handle_interrupt_request_0x03();
extern void handle_interrupt_request_0x04();
extern void handle_interrupt_request_0x05();
extern void handle_interrupt_request_0x06();
extern void handle_interrupt_request_0x07();
extern void handle_interrupt_request_0x08();
extern void handle_interrupt_request_0x09();
extern void handle_interrupt_request_0x0A();
extern void handle_interrupt_request_0x0B();
extern void handle_interrupt_request_0x0C();
extern void handle_interrupt_request_0x0D();
extern void handle_interrupt_request_0x0E();
extern void handle_interrupt_request_0x0F();
extern void handle_interrupt_request_0x31();

extern void handle_exception_0x00();
extern void handle_exception_0x01();
extern void handle_exception_0x02();
extern void handle_exception_0x03();
extern void handle_exception_0x04();
extern void handle_exception_0x05();
extern void handle_exception_0x06();
extern void handle_exception_0x07();
extern void handle_exception_0x08();
extern void handle_exception_0x09();
extern void handle_exception_0x0A();
extern void handle_exception_0x0B();
extern void handle_exception_0x0C();
extern void handle_exception_0x0D();
extern void handle_exception_0x0E();
extern void handle_exception_0x0F();
extern void handle_exception_0x10();
extern void handle_exception_0x11();
extern void handle_exception_0x12();
extern void handle_exception_0x13();

typedef struct InterruptHandler
{
    uint32_t (*handle_interrupt_function)(uint32_t);
} InterruptHandler;

struct GateDescriptor
{
    uint16_t handler_address_low_bits;
    uint16_t gdt_get_code_selector;
    uint8_t reserved;
    uint8_t access;
    uint16_t handler_address_high_bits;
} __attribute__((packed));
typedef struct GateDescriptor GateDescriptor;

struct InterruptDescriptorTablePointer
{
    uint16_t size;
    uint32_t base;
} __attribute__((packed));
typedef struct InterruptDescriptorTablePointer InterruptDescriptorTablePointer;

typedef struct InterruptManager
{
    InterruptHandler *handlers[256];
    GateDescriptor descriptor_table[256];

    uint16_t pic_master_command_8bit_slow;
    uint16_t pic_master_data_8bit_slow;
    uint16_t pic_slaver_command_8bit_slow;
    uint16_t pic_slaver_data_8bit_slow;
} InterruptManager;

extern void on_init_interrupt_manager(InterruptManager *manager,GDT *gdt);
extern void activate_interrupt_manager(InterruptManager *manager);

#endif