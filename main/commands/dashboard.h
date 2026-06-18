#pragma once
#include <stdbool.h>

bool dashboard_start(void);
void dashboard_stop(void);
bool dashboard_running(void);
void cmd_dashboard(int argc, char *argv[]);
