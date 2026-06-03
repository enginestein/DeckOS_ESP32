#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    CAM_RES_QQVGA = 0, // 160x120
    CAM_RES_QVGA  = 1, // 320x240
    CAM_RES_VGA   = 2, // 640x480
    CAM_RES_SVGA  = 3, // 800x600
    CAM_RES_XGA   = 4, // 1024x768
    CAM_RES_SXGA  = 5, // 1280x1024
    CAM_RES_UXGA  = 6, // 1600x1200
} cam_resolution_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t* buf;
    size_t   len;
    bool     jpeg;
} cam_frame_t;

bool     deck_cam_init(void);
bool     deck_cam_detect(void);
bool     deck_cam_capture(cam_frame_t* frame);
void     deck_cam_return_frame(cam_frame_t* frame);
bool     deck_cam_set_resolution(cam_resolution_t res);
void     deck_cam_set_quality(uint8_t q); // 10-63, lower = better
void     deck_cam_set_brightness(int8_t v); // -2 to 2
void     deck_cam_set_contrast(int8_t v);   // -2 to 2
bool     deck_cam_save(const char* path);
bool     deck_cam_save_auto(void);
void     deck_cam_list(void);
bool     deck_cam_stream_start(void);
void     deck_cam_stream_stop(void);
bool     deck_cam_stream_is_active(void);
void     deck_cam_deinit(void);
bool     deck_cam_is_ready(void);
void     deck_cam_print_info(void);
