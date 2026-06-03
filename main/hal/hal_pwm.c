#include "driver/ledc.h"
#include "hal.h"

#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_HIGH_SPEED_MODE
#define LEDC_DUTY_RES   LEDC_TIMER_13_BIT

void hal_pwm_init(uint pin) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {
        .gpio_num = pin,
        .speed_mode = LEDC_MODE,
        .channel = pin % 8,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
    };
    ledc_channel_config(&ch);
}

void hal_pwm_set_duty(uint pin, float duty_pct, uint freq_hz) {
    uint channel = pin % 8;
    // Reconfigure timer if frequency changed
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = freq_hz,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);

    uint32_t duty = (uint32_t)(duty_pct / 100.0f * 8191);
    ledc_set_duty(LEDC_MODE, channel, duty);
    ledc_update_duty(LEDC_MODE, channel);
}

void hal_pwm_deinit(uint pin) {
    ledc_stop(LEDC_MODE, pin % 8, 0);
}
