#ifndef OS_DRIVER_DRIVER
#define OS_DRIVER_DRIVER

#include <stdtype.h>

#define MAX_DRIVER_SIZE 256

typedef struct Driver
{
    void (*activate)();
    int (*reset)();
    void (*deactivate)();
} __attribute__((packed)) Driver;

typedef struct DriverManager
{
    Driver drivers[MAX_DRIVER_SIZE];
    uint8_t size;
}__attribute__((packed)) DriverManager;

extern Driver create_driver(void (*activate)(),int (*reset)(),void (*deactivate)());

extern void on_init_driver_manager(DriverManager*);
extern void append_driver(DriverManager*,Driver*);
extern void driver_activate_all(DriverManager*);

#endif