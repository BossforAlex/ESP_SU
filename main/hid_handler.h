#ifndef HID_HANDLER_H
#define HID_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

void hid_init(void);
void hid_send_key(uint8_t modifier, uint8_t keycode);
void hid_release_all(void);
void hid_send_string(const char *text);
void hid_send_enter(void);
bool hid_is_ready(void);

#endif