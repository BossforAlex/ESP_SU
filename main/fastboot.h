#ifndef FASTBOOT_H
#define FASTBOOT_H

#include <stdint.h>
#include <stdbool.h>

void fastboot_init(void);
void fastboot_on_device(uint8_t dev_addr, uint8_t ep_out);
bool fastboot_is_running(void);

#endif