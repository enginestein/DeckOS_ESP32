#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hal.h"
#include "mpu6050.h"

static mpu6050_cal_t s_cal = {0};
static bool          s_inited = false;
static uint8_t       s_addr   = MPU6050_ADDR_LOW;
static uint          s_sda, s_scl;

static bool ensure_init(uint sda, uint scl, uint8_t addr) {
    if (!s_inited) {
        if (mpu6050_whoami() != 0x68 && mpu6050_whoami() != 0x71) {
            if (!mpu6050_init(sda, scl, addr)) {
                printf("mpu6050: init failed\n");
                return false;
            }
        }
    } else if (sda != s_sda || scl != s_scl || addr != s_addr) {
        if (!mpu6050_init(sda, scl, addr)) {
            printf("mpu6050: init failed\n");
            return false;
        }
    }

    if (mpu6050_whoami() != 0x68) {
        printf("mpu6050: no device at 0x%02X (check wiring / AD0 pin)\n", addr);
        return false;
    }
    s_sda    = sda;
    s_scl    = scl;
    s_addr   = addr;
    s_inited = true;
    return true;
}

void cmd_imu(int argc, char* argv[]) {
    if (argc < 2) {
        printf("usage:\n");
        printf("  imu read      [sda] [scl] [addr]      - one sample\n");
        printf("  imu stream    <n> [ms] [sda] [scl]    - continuous samples\n");
        printf("  imu attitude  [sda] [scl]              - roll / pitch\n");
        printf("  imu calibrate [samples] [sda] [scl]   - measure and store bias\n");
        printf("  imu raw       [sda] [scl]              - raw hex register dump\n");
        printf("  imu whoami    [sda] [scl]              - WHO_AM_I check\n");
        printf("default: SDA=GP4 SCL=GP5 addr=0x68\n");
        return;
    }

    const char* sub = argv[1];

    if (strcmp(sub, "whoami") == 0) {
        uint sda  = (argc >= 3) ? (uint)atoi(argv[2]) : 4;
        uint scl  = (argc >= 4) ? (uint)atoi(argv[3]) : 5;
        uint8_t addr = (argc >= 5) ? (uint8_t)strtol(argv[4], NULL, 16) : MPU6050_ADDR_LOW;
        mpu6050_init(sda, scl, addr);
        uint8_t id = mpu6050_whoami();
        printf("WHO_AM_I = 0x%02X  (%s)\n", id,
               id == 0x68 ? "MPU6050 confirmed" : "unexpected -- check wiring");
        return;
    }

    if (strcmp(sub, "raw") == 0) {
        uint sda  = (argc >= 3) ? (uint)atoi(argv[2]) : 4;
        uint scl  = (argc >= 4) ? (uint)atoi(argv[3]) : 5;
        uint8_t addr = MPU6050_ADDR_LOW;
        if (!ensure_init(sda, scl, addr)) return;
        uint8_t raw[14];
        uint8_t reg = 0x3B;
        hal_i2c_write(s_addr, &reg, 1);
        hal_i2c_read(s_addr, raw, 14);
        printf("raw registers 0x3B..0x48:\n");
        const char* labels[] = {"AX_H","AX_L","AY_H","AY_L","AZ_H","AZ_L",
                                "TMP_H","TMP_L",
                                "GX_H","GX_L","GY_H","GY_L","GZ_H","GZ_L"};
        for (int i = 0; i < 14; i++)
            printf("  0x%02X  %s = 0x%02X (%d)\n", 0x3B + i, labels[i], raw[i], raw[i]);
        return;
    }

    if (strcmp(sub, "calibrate") == 0) {
        int  n   = (argc >= 3) ? atoi(argv[2]) : 200;
        uint sda = (argc >= 4) ? (uint)atoi(argv[3]) : 4;
        uint scl = (argc >= 5) ? (uint)atoi(argv[4]) : 5;
        if (n < 10 || n > 2000) { printf("samples: 10-2000\n"); return; }
        if (!ensure_init(sda, scl, MPU6050_ADDR_LOW)) return;
        printf("calibrating with %d samples -- keep sensor still and level...\n", n);
        if (!mpu6050_calibrate(MPU6050_ACCEL_2G, MPU6050_GYRO_250DPS, n, &s_cal)) {
            printf("calibration failed\n"); return;
        }
        printf("done.\n");
        printf("  accel bias : ax=%.4f  ay=%.4f  az=%.4f  g\n",
               s_cal.ax_bias, s_cal.ay_bias, s_cal.az_bias);
        printf("  gyro  bias : gx=%.3f  gy=%.3f  gz=%.3f  deg/s\n",
               s_cal.gx_bias, s_cal.gy_bias, s_cal.gz_bias);
        printf("  (bias stored in RAM -- will be applied to all subsequent reads)\n");
        return;
    }

    if (strcmp(sub, "attitude") == 0) {
        uint sda = (argc >= 3) ? (uint)atoi(argv[2]) : 4;
        uint scl = (argc >= 4) ? (uint)atoi(argv[3]) : 5;
        if (!ensure_init(sda, scl, MPU6050_ADDR_LOW)) return;
        mpu6050_data_t d;
        if (!mpu6050_read(MPU6050_ACCEL_2G, MPU6050_GYRO_250DPS, &d)) { printf("read failed\n"); return; }
        mpu6050_apply_cal(&d, &s_cal);
        float roll, pitch;
        mpu6050_attitude(&d, &roll, &pitch);
        printf("attitude (static, accel only):\n");
        printf("  roll  : %+7.2f deg\n", roll);
        printf("  pitch : %+7.2f deg\n", pitch);
        return;
    }

    if (strcmp(sub, "stream") == 0) {
        if (argc < 3) { printf("imu stream <count> [interval_ms] [sda] [scl]\n"); return; }
        int      n    = atoi(argv[2]);
        uint32_t ms   = (argc >= 4) ? (uint32_t)atoi(argv[3]) : 100;
        uint     sda  = (argc >= 5) ? (uint)atoi(argv[4]) : 4;
        uint     scl  = (argc >= 6) ? (uint)atoi(argv[5]) : 5;
        if (n < 1 || n > 10000) { printf("count: 1-10000\n"); return; }
        if (ms < 10 || ms > 5000) ms = 100;
        if (!ensure_init(sda, scl, MPU6050_ADDR_LOW)) return;

        printf("streaming %d samples @ %lu ms  (any key to stop)\n", n, ms);
        printf("  #      ax       ay       az      gx      gy      gz    roll   pitch  temp\n");
        printf("  ---  ------   ------   ------  ------  ------  ------  -----  -----  ----\n");

        for (int i = 0; i < n; i++) {
            if (hal_console_getchar() >= 0) { printf("stopped.\n"); return; }
            mpu6050_data_t d;
            if (!mpu6050_read(MPU6050_ACCEL_2G, MPU6050_GYRO_250DPS, &d)) { printf("read error\n"); return; }
            mpu6050_apply_cal(&d, &s_cal);
            float roll, pitch;
            mpu6050_attitude(&d, &roll, &pitch);
            printf("  %-4d %+6.3f  %+6.3f  %+6.3f  %+6.1f  %+6.1f  %+6.1f  %+5.1f  %+5.1f  %.1f\n",
                   i, d.ax, d.ay, d.az, d.gx, d.gy, d.gz, roll, pitch, d.temp_c);
            hal_sleep_ms(ms);
        }
        return;
    }

    if (strcmp(sub, "read") == 0 || true) {
        uint    sda  = (argc >= 3) ? (uint)atoi(argv[2]) : 4;
        uint    scl  = (argc >= 4) ? (uint)atoi(argv[3]) : 5;
        uint8_t addr = (argc >= 5) ? (uint8_t)strtol(argv[4], NULL, 16) : MPU6050_ADDR_LOW;
        if (!ensure_init(sda, scl, addr)) return;
        mpu6050_data_t d;
        if (!mpu6050_read(MPU6050_ACCEL_2G, MPU6050_GYRO_250DPS, &d)) { printf("read failed\n"); return; }
        mpu6050_apply_cal(&d, &s_cal);
        float roll, pitch;
        mpu6050_attitude(&d, &roll, &pitch);
        printf("MPU6050  (0x%02X)\n", s_addr);
        printf("  accel  : ax=%+6.3f  ay=%+6.3f  az=%+6.3f  g\n", d.ax, d.ay, d.az);
        printf("  gyro   : gx=%+7.2f  gy=%+7.2f  gz=%+7.2f  deg/s\n", d.gx, d.gy, d.gz);
        printf("  temp   : %.1f C\n", d.temp_c);
        printf("  roll   : %+.2f deg    pitch : %+.2f deg\n", roll, pitch);
        printf("  cal    : %s\n", s_cal.calibrated ? "applied" : "none (run 'imu calibrate')");
    }
}
