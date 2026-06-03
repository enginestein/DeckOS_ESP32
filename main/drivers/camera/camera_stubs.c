#include <stdio.h>
#include "camera_esp32.h"

bool cam_init(void) {
    printf("[camera] not available (driver commented out)\n");
    return false;
}

bool cam_detect(void) {
    return false;
}

bool cam_capture(cam_frame_t* frame) {
    (void)frame;
    return false;
}

void cam_return_frame(cam_frame_t* frame) {
    (void)frame;
}

bool cam_set_resolution(cam_resolution_t res) {
    (void)res;
    return false;
}

void cam_set_quality(uint8_t q) {
    (void)q;
}

void cam_set_brightness(int8_t v) {
    (void)v;
}

void cam_set_contrast(int8_t v) {
    (void)v;
}

bool cam_start_stream(uint16_t port) {
    (void)port;
    return false;
}

void cam_stop_stream(void) {
}

void cam_deinit(void) {
}

bool cam_is_ready(void) {
    return false;
}

void cam_print_info(void) {
    printf("[camera] not available\n");
}
