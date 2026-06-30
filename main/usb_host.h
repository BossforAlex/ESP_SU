#ifndef USB_HOST_H
#define USB_HOST_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*usb_host_device_cb_t)(uint8_t dev_addr, uint8_t ep_out);

void usb_host_init(void);
void usb_host_set_device_callback(usb_host_device_cb_t cb);
bool usb_host_send_bulk(const uint8_t *data, uint32_t len);

#endif