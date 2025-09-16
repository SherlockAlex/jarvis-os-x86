#ifndef OS_KERNEL_PIC
#define OS_KERNEL_PIC

#include <stdtype.h>

struct DriverManager;
struct InterruptManager;

// PIC地址无关执行代码
// PIE地址无关可执行文件
// 该文件为操作系统的
enum BaseAddressRegisterType{
    MemoryMapping = 0,
    InputOutput = 1
};

typedef enum BaseAddressRegisterType BaseAddressRegisterType;

typedef struct BaseAddressRegister{
    uint8_t prefetchable;
    uint8_t* address;
    uint32_t size;
    BaseAddressRegisterType type;
}BaseAddressRegister;

typedef struct PICDeviceDescriptor{
    uint32_t port_base;
    uint32_t interrupt;

    uint8_t bus;
    uint8_t device;
    uint8_t function;

    uint16_t device_id;
    uint16_t vendor_id;

    uint8_t class_id;
    uint8_t subclass_id;
    uint8_t interface_id;

    uint8_t revision;

}PICDeviceDescriptor;

typedef struct PICController{
    uint32_t data_port32;
    uint32_t command_port32;
}PICController;

extern void on_init_pic_controller(PICController*);
extern uint32_t read(
    PICController* pic
    ,uint8_t bus
    ,uint8_t device
    ,uint8_t function
    ,uint8_t registeroffset
);

extern void write(
    PICController* pic
    ,uint8_t bus
    ,uint8_t device
    ,uint8_t function
    ,uint8_t register_offset
    ,uint32_t value
);

extern uint8_t device_has_functions(
    PICController* pic
    ,uint8_t bus
    , uint8_t device
);

extern PICDeviceDescriptor get_device_descriptor(
    PICController* pic_controller
    ,uint8_t bus,
    uint8_t device,
    uint8_t function);

extern BaseAddressRegister get_base_address_register(
    PICController* pic_controller
    , uint8_t bus
    , uint8_t device
    , uint8_t function
    , uint8_t bar);

extern void select_drivers(PICController*,struct InterruptManager*,struct DriverManager*);
#endif