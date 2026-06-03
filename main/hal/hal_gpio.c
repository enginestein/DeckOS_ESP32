#include "driver/gpio.h"
#include "soc/gpio_reg.h"
#include "hal.h"

void hal_gpio_init(uint pin) {
    gpio_reset_pin(pin);
}

void hal_gpio_set_dir(uint pin, bool out) {
    gpio_set_direction(pin, out ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
}

void hal_gpio_put(uint pin, bool val) {
    gpio_set_level(pin, val);
}

bool hal_gpio_get(uint pin) {
    return gpio_get_level(pin);
}

void hal_gpio_set_pull(uint pin, bool up, bool down) {
    gpio_set_pull_mode(pin,
        up ? GPIO_PULLUP_ONLY :
        down ? GPIO_PULLDOWN_ONLY :
        GPIO_FLOATING);
}

void hal_gpio_set_function(uint pin, uint8_t fn) {
    gpio_set_direction(pin, GPIO_MODE_INPUT_OUTPUT);
}

uint32_t hal_gpio_get_all(void) {
    return REG_READ(GPIO_IN_REG);
}
