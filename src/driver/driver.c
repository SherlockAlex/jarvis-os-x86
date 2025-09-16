#include <stdtype.h>
#include <kernel/kerio.h>
#include <driver/driver.h>

static int driver_reset()
{
    return 0;
}

Driver create_driver(void (*activate)(),int (*reset)(),void (*deactivate)()){
    Driver driver;
    driver.activate = activate;
    driver.reset = reset?reset:driver_reset;
    driver.deactivate = deactivate;
}

void on_init_driver_manager(DriverManager* manager)
{
    manager->size = 0;
    kernel_printf("initialize driver manager success\n");
}

void append_driver(DriverManager* manager,Driver* driver) {
    if (manager && driver && manager->size < MAX_DRIVER_SIZE) {
        manager->drivers[manager->size] = *driver;
        manager->size++;
        kernel_printf("Driver added successfully\n");
    } else if (manager && manager->size >= MAX_DRIVER_SIZE) {
        kernel_printf("Driver cannot be added: driver limit reached\n");
    }
}
extern void driver_activate_all(DriverManager* manager)
{
    for(uint8_t i = 0;i<manager->size;i++)
    {
        if(!manager->drivers[i].activate)
        {
            continue;
        }
        manager->drivers[i].activate();
    }
}