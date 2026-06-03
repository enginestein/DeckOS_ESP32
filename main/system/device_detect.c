#include <stdio.h>
#include <string.h>
#include "hal.h"
#include "device_detect.h"

static const i2c_device_sig_t known_i2c[] = {
    {0x3C, "SSD1306",   "OLED display (128x64)"},
    {0x3D, "SSD1306",   "OLED display (128x32)"},
    {0x48, "ADS1115",   "16-bit ADC"},
    {0x49, "ADS1115",   "16-bit ADC (ADDR=VCC)"},
    {0x4A, "ADS1115",   "16-bit ADC (ADDR=SDA)"},
    {0x4B, "ADS1115",   "16-bit ADC (ADDR=SCL)"},
    {0x57, "AT24C32",   "EEPROM (DS3231 module)"},
    {0x68, "DS3231",    "RTC real-time clock"},
    {0x68, "MPU6050",   "6-axis IMU (same addr as DS3231)"},
    {0x69, "MPU6050",   "6-axis IMU (ADDR pin high)"},
    {0x76, "BMP280",    "pressure/temperature sensor"},
    {0x77, "BMP280",    "pressure/temperature (SDO high)"},
    {0x23, "BH1750",    "ambient light sensor"},
    {0x5C, "BH1750",    "ambient light sensor (ADDR high)"},
    {0x29, "VL53L0X",   "ToF distance sensor"},
    {0x40, "INA219",    "current/power monitor"},
    {0x44, "SHT31",     "humidity/temperature sensor"},
    {0x45, "SHT31",     "humidity/temperature (ADDR high)"},
    {0x60, "MCP4725",   "12-bit DAC"},
    {0x68, "TCA9548A",  "I2C multiplexer"},
    {0x20, "PCF8574",   "I/O expander"},
    {0x27, "PCF8574",   "LCD I2C backpack (typical)"},
    {0x1E, "HMC5883L",  "magnetometer"},
    {0x0D, "QMC5883L",  "magnetometer"},
    {0x53, "ADXL345",   "accelerometer"},
    {0x1C, "MMA8452",   "3-axis accelerometer"},
    {0x18, "LIS3DH",    "3-axis accelerometer"},
};
static const int known_i2c_count = sizeof(known_i2c) / sizeof(known_i2c[0]);

static const char* i2c_lookup(uint8_t addr) {
    for (int i = 0; i < known_i2c_count; i++)
        if (known_i2c[i].addr == addr) return known_i2c[i].name;
    return NULL;
}
static const char* i2c_desc(uint8_t addr) {
    for (int i = 0; i < known_i2c_count; i++)
        if (known_i2c[i].addr == addr) return known_i2c[i].description;
    return "unknown I2C device";
}

static int detect_i2c(detected_device_t* out, int max, int* count, uint sda, uint scl) {
    uint8_t addrs[128];
    int found_total = hal_i2c_scan(sda, scl, addrs, 128);
    int found = 0;

    for (int i = 0; i < found_total && *count < max; i++) {
        uint8_t addr = addrs[i];
        detected_device_t* d = &out[(*count)++];
        strncpy(d->bus, "I2C", 7);
        const char* nm = i2c_lookup(addr);
        strncpy(d->name, nm ? nm : "unknown", 23);
        snprintf(d->detail, sizeof(d->detail),
                 "addr=0x%02X  SDA=GP%d SCL=GP%d  %s",
                 addr, sda, scl, i2c_desc(addr));
        d->addr_or_pin = addr;
        found++;
    }
    return found;
}

static int detect_gpio_devices(detected_device_t* out, int max, int* count) {
    int found = 0;
    uint32_t all = hal_gpio_get_all();
    for (int pin = 0; pin <= 39 && *count < max; pin++) {
        if (all & (1u << pin)) {
            bool dir = true;
            hal_gpio_set_dir(pin, true);
            if (hal_gpio_get(pin)) {
                detected_device_t* d = &out[(*count)++];
                strncpy(d->bus, "GPIO", 7);
                strncpy(d->name, "GPIO output high", 23);
                snprintf(d->detail, sizeof(d->detail),
                         "pin=%d  likely relay/LED/switch", pin);
                d->addr_or_pin = (uint8_t)pin;
                found++;
            }
        }
    }
    return found;
}

static int detect_adc(detected_device_t* out, int max, int* count) {
    int found = 0;
    const int adc_pins[] = {36, 39, 34, 35, 32, 33};
    int num_adc = sizeof(adc_pins) / sizeof(adc_pins[0]);
    for (int i = 0; i < num_adc && *count < max; i++) {
        float v = hal_adc_read_voltage(i);
        if (v > 0.05f) {
            detected_device_t* d = &out[(*count)++];
            strncpy(d->bus, "ADC", 7);
            snprintf(d->name, 24, "ADC ch%d (GP%d)", i, adc_pins[i]);
            snprintf(d->detail, sizeof(d->detail),
                     "%.3f V - sensor/pot/signal present", v);
            d->addr_or_pin = (uint8_t)adc_pins[i];
            found++;
        }
    }
    return found;
}

int device_detect_all(detected_device_t* out, int max, uint sda, uint scl) {
    int count = 0;
    detect_i2c(out, max, &count, sda, scl);
    detect_gpio_devices(out, max, &count);
    detect_adc(out, max, &count);
    return count;
}

void device_detect_print(uint sda, uint scl) {
    detected_device_t devices[MAX_DETECTED];
    printf("scanning for connected devices...\n");
    printf("  I2C bus: SDA=GP%d  SCL=GP%d\n", sda, scl);

    int n = device_detect_all(devices, MAX_DETECTED, sda, scl);

    if (n == 0) {
        printf("  no external devices detected\n\n");
        printf("  tips:\n");
        printf("    I2C devices : SDA=GP%d, SCL=GP%d (current)\n", sda, scl);
        printf("    ADC devices : GPIO36/39/34/35/32/33\n");
        return;
    }

    printf("\n  BUS       NAME                   DETAIL\n");
    printf("  --------- ---------------------- -------------------------------------------\n");
    for (int i = 0; i < n; i++) {
        printf("  %-9s %-22s %s\n",
               devices[i].bus, devices[i].name, devices[i].detail);
    }
    printf("\n%d device(s) found.\n", n);
}
