#pragma once
#include <stdio.h>

void print_lock_init(void);
void print_lock_acquire(void);
void print_lock_release(void);

#define PRINT_LOCK()   print_lock_acquire()
#define PRINT_UNLOCK() print_lock_release()
