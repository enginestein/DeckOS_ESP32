#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "module.h"
#include "commands.h"
#include "camera_esp32.h"

static void cmd_camera(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  camera init                  init the camera\n");
        printf("  camera capture               capture and dump JPEG info\n");
        printf("  camera save [path]           save capture to SPIFFS\n");
        printf("  camera ls                    list saved captures\n");
        printf("  camera info                  show camera info\n");
        printf("  camera res <qqvga|qvga|vga|svga|xga|sxga|uxga>\n");
        printf("  camera quality <10-63>       set JPEG quality\n");
        printf("  camera brightness <-2..2>    set brightness\n");
        printf("  camera contrast <-2..2>      set contrast\n");
        printf("  camera stream                start MJPEG stream on port 80\n");
        printf("  camera stream stop           stop MJPEG stream\n");
        return;
    }
    if (strcmp(argv[1], "init") == 0) {
        if (deck_cam_init()) printf("Camera initialised\n");
        else printf("Camera init FAILED\n");
    } else if (strcmp(argv[1], "capture") == 0) {
        cam_frame_t f;
        if (deck_cam_capture(&f)) {
            printf("captured %dx%d %s (%zu bytes)\n",
                   f.width, f.height, f.jpeg ? "JPEG" : "RAW", f.len);
            deck_cam_return_frame(&f);
        } else printf("capture failed\n");
    } else if (strcmp(argv[1], "ls") == 0) {
        deck_cam_list();
    } else if (strcmp(argv[1], "save") == 0) {
        if (!deck_cam_is_ready()) { printf("camera not initialised\n"); return; }
        if (argc >= 3) deck_cam_save(argv[2]);
        else deck_cam_save_auto();
    } else if (strcmp(argv[1], "info") == 0) {
        deck_cam_print_info();
    } else if (strcmp(argv[1], "res") == 0 && argc >= 3) {
        const char *res_str[] = {"qqvga","qvga","vga","svga","xga","sxga","uxga"};
        for (int i = 0; i < 7; i++) {
            if (strcmp(argv[2], res_str[i]) == 0) {
                deck_cam_set_resolution((cam_resolution_t)i);
                printf("resolution set to %s\n", res_str[i]);
                return;
            }
        }
        printf("unknown resolution\n");
    } else if (strcmp(argv[1], "quality") == 0 && argc >= 3) {
        deck_cam_set_quality((uint8_t)atoi(argv[2]));
        printf("quality set\n");
    } else if (strcmp(argv[1], "brightness") == 0 && argc >= 3) {
        deck_cam_set_brightness((int8_t)atoi(argv[2]));
        printf("brightness set\n");
    } else if (strcmp(argv[1], "contrast") == 0 && argc >= 3) {
        deck_cam_set_contrast((int8_t)atoi(argv[2]));
        printf("contrast set\n");
    } else if (strcmp(argv[1], "stream") == 0) {
        if (argc >= 3 && strcmp(argv[2], "stop") == 0) {
            deck_cam_stream_stop();
            printf("stream stopped\n");
        } else if (deck_cam_stream_start()) {
            printf("MJPEG stream started\n");
        } else {
            printf("stream start failed\n");
        }
    } else printf("unknown camera: %s\n", argv[1]);
}

static module_cmd_t s_cmds[] = {
    {"camera", "Camera (init/capture/save/ls/info/res/quality/brightness/contrast/stream)", cmd_camera},
};

static bool mod_camera_load(void) {
    printf("camera: loaded\n");
    return true;
}

static void mod_camera_unload(void) {
    if (deck_cam_is_ready()) {
        deck_cam_stream_stop();
        deck_cam_deinit();
    }
    printf("camera: unloaded\n");
}

plugin_api_t MOD_CAMERA = {
    .init = mod_camera_load,
    .deinit = mod_camera_unload,
    .commands = s_cmds,
    .command_count = sizeof(s_cmds)/sizeof(s_cmds[0]),
    .on_event = NULL,
};
