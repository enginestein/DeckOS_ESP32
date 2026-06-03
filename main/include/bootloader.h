#pragma once
#include <stdbool.h>
#include "config.h"

void bootloader_run(void);
void bootloader_print_banner(const flash_config_t* cfg);
bool bootloader_check_recovery(void);
