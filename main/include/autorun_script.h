#pragma once

static const char AUTORUN_SCRIPT[] =
    "# DeckOS ESP32 autorun\n"
    "gpio_write 33 1\n"
    "sleep 200\n"
    "gpio_write 33 0\n"
    "sleep 200\n"
    "gpio_write 33 1\n"
    "echo === DeckOS ESP32 Ready ===\n"
    "echo Quick start:\n"
    "echo   module load wifi\n"
    "echo   wifi join MySSID MyPassword\n"
    "echo   serve start\n"
    "echo   wifi ip\n"
    "echo\n"
    "echo Camera: module load camera\n"
    "echo Swarm:  module load swarm\n"
    "echo NRF24:  module load nrf24\n"
    "echo OLED:   module load oled\n"
    "echo\n"
    "echo Serial shell ready. 115200 8N1.\n"
    "gpio_write 33 1\n";
