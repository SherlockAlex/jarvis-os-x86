#include <kernel/pic.h>
#include <kernel/ioctl.h>
#include <kernel/interrupt/interrupt.h>
#include <kernel/kerio.h>
#include <driver/block.h>
#include <driver/driver.h>
#include <kernel/memory/malloc.h>

void on_init_pic_controller(PICController *pic_controller)
{
    pic_controller->data_port32 = 0xcfc;
    pic_controller->command_port32 = 0xcf8;
}

uint32_t read(
    PICController *pic_controller, uint8_t bus, uint8_t device, uint8_t function, uint8_t registeroffset)
{
    uint32_t id =
        1 << 31 |
        ((bus & 0xff) << 16) |
        ((device & 0x1f) << 11) |
        ((function & 0x07) << 8) |
        (registeroffset & 0xfc);
    write_32bit(pic_controller->command_port32, id);
    uint32_t result = read_32bit(pic_controller->data_port32);
    return result >> (8 * (registeroffset % 4));
}

uint8_t device_has_functions(PICController *pic_controller, uint8_t bus, uint8_t device)
{
    return read(pic_controller, bus, device, 0, 0x0e) & (1 << 7);
}

PICDeviceDescriptor get_device_descriptor(
    PICController *pic_controller, uint8_t bus,
    uint8_t device,
    uint8_t function)
{
    PICDeviceDescriptor result;

    result.bus = bus;
    result.device = device;
    result.function = function;

    result.vendor_id = read(pic_controller, bus, device, function, 0);
    result.device_id = read(pic_controller, bus, device, function, 0x02);

    result.class_id = read(pic_controller, bus, device, function, 0x0b);
    result.subclass_id = read(pic_controller, bus, device, function, 0x0a);
    result.interface_id = read(pic_controller, bus, device, function, 0x09);
    result.revision = read(pic_controller, bus, device, function, 0x08);

    result.interrupt = read(pic_controller, bus, device, function, 0x3c);
    return result;
}

BaseAddressRegister get_base_address_register(
    PICController *pic_controller, uint8_t bus, uint8_t device, uint8_t function, uint8_t bar)
{
    BaseAddressRegister result;

    uint32_t headertype = read(pic_controller, bus, device, function, 0x0e) & 0x7e;
    int maxBARs = 6 - 4 * headertype;
    if (bar >= maxBARs)
        return result;

    uint32_t bar_value = read(pic_controller, bus, device, function, 0x10 + 4 * bar);
    result.type = (bar_value & 1) ? InputOutput : MemoryMapping;

    if (result.type == MemoryMapping)
    {
        switch ((bar_value >> 1) & 0x3)
        {
        case 0: // 32
        case 1: // 20
        case 2: // 64
            break;
        }
    }
    else
    {
        result.address = (uint8_t *)(bar_value & ~0x3);
        result.prefetchable = 0;
    }
    return result;
}

extern Driver *get_driver(PICController *controller, PICDeviceDescriptor dev, InterruptManager *interrupts);

// 在select_drivers函数中添加更详细的PCI设备信息
void select_drivers(PICController *pic_controller, InterruptManager *int_manager, DriverManager *driver_manager)
{
    int device_count = 0;
    int storage_controller_count = 0;

    kernel_printf("Scanning PCI devices...\n");

    for (uint16_t bus = 0; bus < 256; bus++)
    {
        for (uint8_t device = 0; device < 32; device++)
        {
            // 检查设备是否存在
            PICDeviceDescriptor dev = get_device_descriptor(pic_controller, bus, device, 0);
            if (dev.vendor_id == 0 || dev.vendor_id == 0xffff)
            {
                continue; // 设备不存在
            }

            int numFunctions = device_has_functions(pic_controller, (uint8_t)bus, device) ? 8 : 1;

            for (uint8_t function = 0; function < numFunctions; function++)
            {
                PICDeviceDescriptor dev = get_device_descriptor(pic_controller, bus, device, function);
                if (dev.vendor_id == 0 || dev.vendor_id == 0xffff)
                {
                    continue;
                }

                device_count++;

                // 特别关注存储控制器
                if (dev.class_id == 0x01)
                {
                    storage_controller_count++;
                    kernel_printf("STORAGE CONTROLLER: ");
                }
                else
                {
                    kernel_printf("PCI Device: ");
                }

                kernel_printf("BUS %x, Device %x, Function %x = Vendor %x, Device %x, Class %x, Subclass %x\n",
                              bus, device, function, dev.vendor_id, dev.device_id, dev.class_id, dev.subclass_id);

                // 打印设备详细信息
                kernel_printf("  Revision: %x, Interface: %x, Interrupt: %x\n",
                              dev.revision, dev.interface_id, dev.interrupt);

                // 检查所有BAR寄存器
                for (uint8_t barNum = 0; barNum < 6; barNum++)
                {
                    BaseAddressRegister bar = get_base_address_register(pic_controller, bus, device, function, barNum);
                    if (bar.address)
                    {
                        if (bar.type == InputOutput)
                        {
                            dev.port_base = (uint32_t)bar.address;
                            kernel_printf("  BAR%d: I/O port base = %x\n", barNum, dev.port_base);
                        }
                        else
                        {
                            kernel_printf("  BAR%d: Memory address = %x\n", barNum, (uint32_t)bar.address);
                        }
                    }
                }

                // 尝试获取驱动程序
                Driver *driver = get_driver(pic_controller, dev, int_manager);
                if (driver)
                {
                    append_driver(driver_manager, driver);
                    kernel_printf("  Driver loaded successfully\n");
                }
                else
                {
                    kernel_printf("  No driver available for this device\n");
                }
            }
        }
    }

    kernel_printf("Total PCI devices found: %d\n", device_count);
    kernel_printf("Storage controllers found: %d\n", storage_controller_count);

    if (storage_controller_count == 0)
    {
        kernel_printf("WARNING: No storage controllers detected!\n");
        // kernel_printf("Make sure QEMU is configured with a hard disk (-hda option)\n");
    }
}

void set_8086_device(PICDeviceDescriptor dev, InterruptManager *interrupts)
{
    kernel_printf("Supported IDE controller found\n");

    // 创建块设备
    BlockDevice *block_dev = (BlockDevice *)malloc(sizeof(BlockDevice));
    if (!block_dev)
    {
        kernel_printf("Failed to allocate block device\n");
        return 0;
    }

    // 初始化块设备
    block_device_initialize(block_dev, dev.port_base, dev.interrupt);

    // 创建块设备驱动
    BlockDriver *block_driver = create_block_driver(block_dev);

    if (block_driver)
    {
        // 注册中断处理函数
        if (dev.interrupt >= 0 && dev.interrupt < 16)
        {
            uint8_t interrupt_num = INTERRUPT_OFFSET + dev.interrupt;
            if (interrupts->handlers[interrupt_num] == 0)
            {
                interrupts->handlers[interrupt_num] = (InterruptHandler *)malloc(sizeof(InterruptHandler));
                if (interrupts->handlers[interrupt_num])
                {
                    interrupts->handlers[interrupt_num]->handle_interrupt_function = block_interrupt_handler;
                    kernel_printf("Registered block device interrupt handler for IRQ %d\n", dev.interrupt);
                }
            }
        }

        // 添加到活动块设备列表
        if (num_block_devices < MAX_BLOCK_DEVICES)
        {
            active_block_devices[num_block_devices] = block_dev;
            num_block_devices++;
            kernel_printf("IDE controller initialized successfully\n");
            kernel_printf("  Base port: %x, Interrupt: %d\n", dev.port_base, dev.interrupt);
            return (Driver *)block_driver;
        }
        else
        {
            kernel_printf("Maximum block devices reached\n");
            free(block_driver);
        }
    }

    free(block_dev);
}

Driver *get_driver(PICController *controller, PICDeviceDescriptor dev, InterruptManager *interrupts)
{
    Driver *driver = 0;

    // 特别处理存储控制器

    kernel_printf("vendor_id:%x deivce_id:%d\n",dev.vendor_id,dev.device_id);

    switch (dev.vendor_id)
    {
    case 0x1022: // AMD
        switch (dev.device_id)
        {
        case 0x2000: // am79c973
            return driver;
            break;
        }
        break;
    case 0x8086: // intel
        set_8086_device(dev,interrupts);
        break;
    }

    switch (dev.class_id)
    {
    case 0x03:
        switch (dev.subclass_id)
        {
        case 0x00: // VGA
            kernel_printf("VGA \n");
            break;
        }
        break;
    }

    /*

    switch (dev.subclass_id)
    {
    case 0x01: // IDE控制器
        kernel_printf("IDE Controller\n");

        // 检查是否是有效的IDE控制器
        if (dev.vendor_id == 0x8086 || dev.vendor_id == 0x1022 ||
            dev.vendor_id == 0x10DE || dev.vendor_id == 0x1106)
        {
            kernel_printf("Supported IDE controller found\n");

            // 创建块设备
            BlockDevice *block_dev = (BlockDevice *)malloc(sizeof(BlockDevice));
            if (!block_dev)
            {
                kernel_printf("Failed to allocate block device\n");
                return 0;
            }

            // 初始化块设备
            block_device_initialize(block_dev, dev.port_base, dev.interrupt);

            // 创建块设备驱动
            BlockDriver *block_driver = create_block_driver(block_dev);

            if (block_driver)
            {
                // 注册中断处理函数
                if (dev.interrupt >= 0 && dev.interrupt < 16)
                {
                    uint8_t interrupt_num = INTERRUPT_OFFSET + dev.interrupt;
                    if (interrupts->handlers[interrupt_num] == 0)
                    {
                        interrupts->handlers[interrupt_num] = (InterruptHandler *)malloc(sizeof(InterruptHandler));
                        if (interrupts->handlers[interrupt_num])
                        {
                            interrupts->handlers[interrupt_num]->handle_interrupt_function = block_interrupt_handler;
                            kernel_printf("Registered block device interrupt handler for IRQ %d\n", dev.interrupt);
                        }
                    }
                }

                // 添加到活动块设备列表
                if (num_block_devices < MAX_BLOCK_DEVICES)
                {
                    active_block_devices[num_block_devices] = block_dev;
                    num_block_devices++;
                    kernel_printf("IDE controller initialized successfully\n");
                    kernel_printf("  Base port: %x, Interrupt: %d\n", dev.port_base, dev.interrupt);
                    return (Driver *)block_driver;
                }
                else
                {
                    kernel_printf("Maximum block devices reached\n");
                    free(block_driver);
                }
            }

            free(block_dev);
        }
        else
        {
            kernel_printf("Unsupported IDE controller vendor: %x\n", dev.vendor_id);
        }
        break;

    case 0x02: // 软盘控制器
        kernel_printf("Floppy Controller (not implemented)\n");
        break;

    case 0x03: // IPI总线控制器
        kernel_printf("IPI Bus Controller (not implemented)\n");
        break;

    case 0x04: // RAID控制器
        kernel_printf("RAID Controller (not implemented)\n");
        break;

    case 0x05: // ATA控制器
        kernel_printf("ATA Controller (not implemented)\n");
        break;

    case 0x06: // SATA控制器
        kernel_printf("SATA Controller (not implemented)\n");
        break;

    case 0x07: // 串行SCSI控制器
        kernel_printf("Serial SCSI Controller (not implemented)\n");
        break;

    case 0x08: // NVMe控制器
        kernel_printf("NVMe Controller (not implemented)\n");
        break;

    default:
        kernel_printf("Unknown storage controller subclass: %x\n", dev.subclass_id);
        break;
    }

    */

    return driver;
}