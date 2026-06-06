#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "module.h"
#include "commands.h"
#include "hal.h"
#include "mpu6050.h"

static mpu6050_cal_t s_cal = {0};
static bool          s_inited = false;
static uint8_t       s_addr   = MPU6050_ADDR_LOW;
static uint          s_sda, s_scl;

static bool ensure_init(uint sda, uint scl, uint8_t addr) {
    if (!s_inited) {
        if (!mpu6050_init(sda, scl, addr)) {
            printf("mpu6050: init failed\n");
            return false;
        }
    } else if (sda != s_sda || scl != s_scl || addr != s_addr) {
        if (!mpu6050_init(sda, scl, addr)) {
            printf("mpu6050: init failed\n");
            return false;
        }
    }
    if (mpu6050_whoami() != 0x68) {
        printf("mpu6050: no device at 0x%02X\n", addr);
        return false;
    }
    s_sda = sda; s_scl = scl; s_addr = addr; s_inited = true;
    return true;
}

static void cmd_imu(int argc, char *argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  imu read      [sda] [scl] [addr]      - one sample\n");
        printf("  imu stream    <n> [ms] [sda] [scl]    - continuous samples\n");
        printf("  imu attitude  [sda] [scl]              - roll / pitch\n");
        printf("  imu calibrate [samples] [sda] [scl]   - measure and store bias\n");
        printf("default: SDA=GP4 SCL=GP5 addr=0x68\n");
        return;
    }
    const char *sub = argv[1];
    if (strcmp(sub, "calibrate") == 0) {
        int  n   = (argc >= 3) ? atoi(argv[2]) : 200;
        uint sda = (argc >= 4) ? (uint)atoi(argv[3]) : 4;
        uint scl = (argc >= 5) ? (uint)atoi(argv[4]) : 5;
        if (n < 10 || n > 2000) { printf("samples: 10-2000\n"); return; }
        if (!ensure_init(sda, scl, MPU6050_ADDR_LOW)) return;
        printf("calibrating with %d samples -- keep sensor still...\n", n);
        if (!mpu6050_calibrate(MPU6050_ACCEL_2G, MPU6050_GYRO_250DPS, n, &s_cal)) {
            printf("calibration failed\n"); return;
        }
        printf("done. accel bias: ax=%.4f ay=%.4f az=%.4f g\n",
               s_cal.ax_bias, s_cal.ay_bias, s_cal.az_bias);
        printf("gyro bias: gx=%.3f gy=%.3f gz=%.3f deg/s\n",
               s_cal.gx_bias, s_cal.gy_bias, s_cal.gz_bias);
    } else if (strcmp(sub, "attitude") == 0) {
        uint sda = (argc >= 3) ? (uint)atoi(argv[2]) : 4;
        uint scl = (argc >= 4) ? (uint)atoi(argv[3]) : 5;
        if (!ensure_init(sda, scl, MPU6050_ADDR_LOW)) return;
        mpu6050_data_t d;
        if (!mpu6050_read(MPU6050_ACCEL_2G, MPU6050_GYRO_250DPS, &d)) { printf("read failed\n"); return; }
        mpu6050_apply_cal(&d, &s_cal);
        float roll, pitch;
        mpu6050_attitude(&d, &roll, &pitch);
        printf("roll: %+7.2f deg  pitch: %+7.2f deg\n", roll, pitch);
    } else if (strcmp(sub, "stream") == 0) {
        if (argc < 3) { printf("imu stream <count> [interval_ms] [sda] [scl]\n"); return; }
        int n = atoi(argv[2]);
        uint32_t ms = (argc >= 4) ? (uint32_t)atoi(argv[3]) : 100;
        uint sda = (argc >= 5) ? (uint)atoi(argv[4]) : 4;
        uint scl = (argc >= 6) ? (uint)atoi(argv[5]) : 5;
        if (!ensure_init(sda, scl, MPU6050_ADDR_LOW)) return;
        for (int i = 0; i < n && i < 10000; i++) {
            mpu6050_data_t d;
            if (!mpu6050_read(MPU6050_ACCEL_2G, MPU6050_GYRO_250DPS, &d)) break;
            mpu6050_apply_cal(&d, &s_cal);
            printf("%d: ax=%+6.3f ay=%+6.3f az=%+6.3f gx=%+7.2f gy=%+7.2f gz=%+7.2f temp=%.1f\n",
                   i, d.ax, d.ay, d.az, d.gx, d.gy, d.gz, d.temp_c);
            hal_sleep_ms(ms);
        }
    } else {
        uint sda = (argc >= 3) ? (uint)atoi(argv[2]) : 4;
        uint scl = (argc >= 4) ? (uint)atoi(argv[3]) : 5;
        uint8_t addr = (argc >= 5) ? (uint8_t)strtol(argv[4], NULL, 16) : MPU6050_ADDR_LOW;
        if (!ensure_init(sda, scl, addr)) return;
        mpu6050_data_t d;
        if (!mpu6050_read(MPU6050_ACCEL_2G, MPU6050_GYRO_250DPS, &d)) { printf("read failed\n"); return; }
        mpu6050_apply_cal(&d, &s_cal);
        printf("ax=%+6.3f ay=%+6.3f az=%+6.3f gx=%+7.2f gy=%+7.2f gz=%+7.2f temp=%.1f C\n",
               d.ax, d.ay, d.az, d.gx, d.gy, d.gz, d.temp_c);
    }
}

static module_cmd_t s_cmds[] = {
    {"imu", "MPU6050 IMU (read/stream/attitude/calibrate)", cmd_imu},
};

static bool mod_imu_load(void) {
    if (!mpu6050_init(4, 5, MPU6050_ADDR_LOW)) {
        printf("imu: init failed\n");
        return false;
    }
    s_sda = 4; s_scl = 5; s_addr = MPU6050_ADDR_LOW; s_inited = true;
    printf("imu: loaded\n");
    return true;
}

static void mod_imu_unload(void) {
    uint8_t reg = MPU6050_REG_PWR_MGMT_1;
    uint8_t val = 0x40;
    hal_i2c_write(s_addr, &reg, 1);
    hal_i2c_write(s_addr, &val, 1);
    s_inited = false;
    printf("imu: unloaded\n");
}

plugin_api_t MOD_IMU = {
    .init = mod_imu_load,
    .deinit = mod_imu_unload,
    .commands = s_cmds,
    .command_count = sizeof(s_cmds)/sizeof(s_cmds[0]),
    .on_event = NULL,
};
