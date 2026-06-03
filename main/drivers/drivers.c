#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "drivers.h"
#include "spi_bus.h"

static int drv_adc_init(void) {
    hal_adc_init();
    printf("[drivers] adc: ok\n");
    return 0;
}

static int drv_gpio_init(void) { return 0; }
static int drv_pwm_init(void)  { return 0; }

static int drv_i2c0_init(void) {
    hal_i2c_init(21, 22, 100000);
    return 0;
}
static void drv_i2c0_deinit(void) {}

static int drv_spi0_init(void) {
    spi_bus_init(18, 23, 19, 1000000);
    return 0;
}
static void drv_spi0_deinit(void) { spi_bus_deinit(); }

static driver_t registry[MAX_DRIVERS];
static int      driver_count = 0;

int driver_register(const char* name, int (*init)(void), void (*deinit)(void)) {
    if (driver_count >= MAX_DRIVERS) return -1;
    registry[driver_count].name   = name;
    registry[driver_count].status = DRIVER_UNLOADED;
    registry[driver_count].init   = init;
    registry[driver_count].deinit = deinit;
    driver_count++;
    return 0;
}

void drivers_init_all(void) {
    driver_register("adc",  drv_adc_init,  NULL);
    driver_register("gpio", drv_gpio_init, NULL);
    driver_register("pwm",  drv_pwm_init,  NULL);
    driver_register("i2c0", drv_i2c0_init, drv_i2c0_deinit);
    driver_register("spi0", drv_spi0_init, drv_spi0_deinit);

    printf("[drivers] initialising %d drivers\n", driver_count);
    for (int i = 0; i < driver_count; i++) {
        int rc = registry[i].init ? registry[i].init() : 0;
        registry[i].status = (rc == 0) ? DRIVER_OK : DRIVER_ERROR;
        printf("  %-8s  [%s]\n",
               registry[i].name,
               registry[i].status == DRIVER_OK ? "OK" : "FAIL");
    }
}

void drivers_list(void) {
    printf("DRIVER   STATUS\n");
    for (int i = 0; i < driver_count; i++) {
        const char* s = "unloaded";
        if (registry[i].status == DRIVER_OK)   s = "ok";
        if (registry[i].status == DRIVER_ERROR) s = "error";
        printf("  %-8s %s\n", registry[i].name, s);
    }
}

driver_status_t driver_status(const char* name) {
    for (int i = 0; i < driver_count; i++)
        if (strcmp(registry[i].name, name) == 0)
            return registry[i].status;
    return DRIVER_UNLOADED;
}
