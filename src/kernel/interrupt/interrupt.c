#include <kernel/interrupt/interrupt.h>
#include <kernel/ioctl.h>
#include <kernel/kerio.h>
#include <kernel/multitask/process.h>

InterruptManager *activated_interrupt_manager = 0;
extern ProcessManager *process_manager;

// 在 interrupt.c 中添加
static void init_pit()
{
    // 设置 PIT 频率为 100Hz (每10ms一次中断)
    uint16_t divisor = 1193; // 1193180Hz / 100Hz

    // 发送命令字节
    write_8bit_slow(0x43, 0x36);

    // 设置除数
    write_8bit_slow(0x40, divisor & 0xFF);
    write_8bit_slow(0x40, divisor >> 8);
}

uint32_t do_handle_interrupt(
    InterruptManager *manager, uint8_t interrupt_number, uint32_t esp)
{
    if (manager->handlers[interrupt_number] != 0)
    {
        if (manager->handlers[interrupt_number]->handle_interrupt_function)
        {
            esp = manager->handlers[interrupt_number]->handle_interrupt_function(esp);
        }
    }
    else if (interrupt_number != INTERRUPT_OFFSET)
    {
        
    }

    if (interrupt_number == INTERRUPT_OFFSET + 0x00)
    {
        // 处理进程管理时间tick
        process_manager_tick();
        // 调用进程调度
        esp = schedule(esp);
    }

    if (INTERRUPT_OFFSET <= interrupt_number && interrupt_number < INTERRUPT_OFFSET + 16)
    {
        write_8bit_slow(manager->pic_master_command_8bit_slow, 0x20);
        if (INTERRUPT_OFFSET + 8 <= interrupt_number)
        {
            write_8bit_slow(manager->pic_slaver_command_8bit_slow, 0x20);
        }
    }

    return esp;
}

uint32_t handle_interrrupt(
    uint8_t interrupt_number, uint32_t esp)
{
    if (activated_interrupt_manager != 0)
    {
        return do_handle_interrupt(activated_interrupt_manager, interrupt_number, esp);
    }
    return esp;
}

void set_interrupt_descriptor_table_entry(
    InterruptManager *manager,
    uint8_t interruptNumber,
    uint16_t get_code_selectorOffset,
    void (*handler)(),
    uint8_t DescriptorPrivilegelLevel,
    uint8_t DescriptorType)
{
    const uint8_t IDT_DESC_PRESENT = 0x80;

    manager->descriptor_table[interruptNumber].handler_address_low_bits = ((uint32_t)handler) & 0xffff;
    manager->descriptor_table[interruptNumber].handler_address_high_bits = ((uint32_t)handler >> 16) & 0xffff;
    manager->descriptor_table[interruptNumber].gdt_get_code_selector = get_code_selectorOffset;
    manager->descriptor_table[interruptNumber].access = IDT_DESC_PRESENT | ((DescriptorPrivilegelLevel & 3) << 5) | DescriptorType;
    manager->descriptor_table[interruptNumber].reserved = 0;
}

void on_init_interrupt_manager(InterruptManager *manager, GDT *gdt)
{

    manager->pic_master_command_8bit_slow = 0x20;
    manager->pic_master_data_8bit_slow = 0x21;
    manager->pic_slaver_command_8bit_slow = 0xA0;
    manager->pic_slaver_data_8bit_slow = 0xA1;

    uint16_t code_segement = (get_code_selector(gdt)) << 3;

    const uint8_t IDT_INTERRUPT_GATE = 0xe;
    for (uint16_t i = 0; i < 256; i++)
    {
        manager->handlers[i] = 0;
        set_interrupt_descriptor_table_entry(manager, i, code_segement, &handle_interrrupt_ignore, 0, IDT_INTERRUPT_GATE);
    }

    set_interrupt_descriptor_table_entry(manager, 0x00, code_segement, &handle_exception_0x00, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x01, code_segement, &handle_exception_0x01, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x02, code_segement, &handle_exception_0x02, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x03, code_segement, &handle_exception_0x03, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x04, code_segement, &handle_exception_0x04, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x05, code_segement, &handle_exception_0x05, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x06, code_segement, &handle_exception_0x06, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x07, code_segement, &handle_exception_0x07, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x08, code_segement, &handle_exception_0x08, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x09, code_segement, &handle_exception_0x09, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x0A, code_segement, &handle_exception_0x0A, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x0B, code_segement, &handle_exception_0x0B, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x0C, code_segement, &handle_exception_0x0C, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x0D, code_segement, &handle_exception_0x0D, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x0E, code_segement, &handle_exception_0x0E, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x0F, code_segement, &handle_exception_0x0F, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x10, code_segement, &handle_exception_0x10, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x11, code_segement, &handle_exception_0x11, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x12, code_segement, &handle_exception_0x12, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, 0x13, code_segement, &handle_exception_0x13, 0, IDT_INTERRUPT_GATE);

    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x00, code_segement, &handle_interrupt_request_0x00, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x01, code_segement, &handle_interrupt_request_0x01, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x02, code_segement, &handle_interrupt_request_0x02, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x03, code_segement, &handle_interrupt_request_0x03, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x04, code_segement, &handle_interrupt_request_0x04, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x05, code_segement, &handle_interrupt_request_0x05, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x06, code_segement, &handle_interrupt_request_0x06, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x07, code_segement, &handle_interrupt_request_0x07, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x08, code_segement, &handle_interrupt_request_0x08, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x09, code_segement, &handle_interrupt_request_0x09, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x0A, code_segement, &handle_interrupt_request_0x0A, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x0B, code_segement, &handle_interrupt_request_0x0B, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x0C, code_segement, &handle_interrupt_request_0x0C, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x0D, code_segement, &handle_interrupt_request_0x0D, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x0E, code_segement, &handle_interrupt_request_0x0E, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x0F, code_segement, &handle_interrupt_request_0x0F, 0, IDT_INTERRUPT_GATE);
    set_interrupt_descriptor_table_entry(manager, INTERRUPT_OFFSET + 0x31, code_segement, &handle_interrupt_request_0x31, 0, IDT_INTERRUPT_GATE);
    // 注册系统调用中断，特权级别为3，表示用户态程序可以调用
    set_interrupt_descriptor_table_entry(manager, 0x80, code_segement, &handle_syscall, 3, IDT_INTERRUPT_GATE);

    write_8bit_slow(manager->pic_master_command_8bit_slow, 0x11);
    write_8bit_slow(manager->pic_slaver_command_8bit_slow, 0x11);

    write_8bit_slow(manager->pic_master_data_8bit_slow, INTERRUPT_OFFSET);
    write_8bit_slow(manager->pic_slaver_data_8bit_slow, INTERRUPT_OFFSET + 8);

    write_8bit_slow(manager->pic_master_data_8bit_slow, 0x04);
    write_8bit_slow(manager->pic_slaver_data_8bit_slow, 0x02);

    write_8bit_slow(manager->pic_master_data_8bit_slow, 0x01);
    write_8bit_slow(manager->pic_slaver_data_8bit_slow, 0x01);

    write_8bit_slow(manager->pic_master_data_8bit_slow, 0x00);
    write_8bit_slow(manager->pic_slaver_data_8bit_slow, 0x00);

    InterruptDescriptorTablePointer idt;
    idt.size = 256 * sizeof(GateDescriptor) - 1;
    idt.base = (uint32_t)manager->descriptor_table;

    asm volatile("lidt %0" : : "m"(idt));
    init_pit();

    kernel_printf("Initlize interrupt manager success\n");
}

void deactivate_interrupt_manager(InterruptManager *manager)
{
    if (activated_interrupt_manager == manager)
    {
        activated_interrupt_manager = 0;
        asm("cli");
    }
}

void activate_interrupt_manager(InterruptManager *manager)
{
    if (activated_interrupt_manager != 0)
    {
        deactivate_interrupt_manager(activated_interrupt_manager);
    }
    activated_interrupt_manager = manager;
    asm("sti");
}