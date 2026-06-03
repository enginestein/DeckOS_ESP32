#include "print_lock.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static SemaphoreHandle_t s_print_mutex = NULL;

void print_lock_init(void) {
    s_print_mutex = xSemaphoreCreateMutex();
}

void print_lock_acquire(void) {
    if (s_print_mutex)
        xSemaphoreTake(s_print_mutex, portMAX_DELAY);
}

void print_lock_release(void) {
    if (s_print_mutex)
        xSemaphoreGive(s_print_mutex);
}
