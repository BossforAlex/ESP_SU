#include "usb_host.h"
#include "tusb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "usb_host";
static usb_host_device_cb_t s_device_cb = NULL;
static SemaphoreHandle_t s_xfer_done = NULL;

static volatile uint8_t  s_dev_addr = 0;
static volatile uint8_t  s_ep_out   = 0;
static volatile bool     s_busy     = false;

/* ---- TinyUSB host callbacks ---- */

void tuh_mount_cb(uint8_t dev_addr) {
    ESP_LOGI(TAG, "Device %d mounted", dev_addr);

    static uint8_t desc_buf[512];
    if (!tuh_descriptor_get_configuration(dev_addr, 0, desc_buf, sizeof(desc_buf),
                                           NULL, 0)) {
        ESP_LOGE(TAG, "Failed to request descriptor");
    }
}

void tuh_mount_failed_cb(uint8_t dev_addr) {
    ESP_LOGI(TAG, "Device %d mount failed", dev_addr);
}

void tuh_umount_cb(uint8_t dev_addr) {
    ESP_LOGI(TAG, "Device %d unmounted", dev_addr);
    s_dev_addr = 0;
    s_ep_out = 0;
}

/* ---- 内部传输完成回调 ---- */
static void xfer_complete(tuh_xfer_t *xfer) {
    ESP_LOGI(TAG, "Xfer done: result=%d len=%d", xfer->result, xfer->actual_len);
    s_busy = false;
    xSemaphoreGive(s_xfer_done);
}

void usb_host_init(void) {
    ESP_LOGI(TAG, "Init USB Host...");

    s_xfer_done = xSemaphoreCreateBinary();
    tuh_init(TUH_OPT_RHPORT);

    ESP_LOGI(TAG, "USB Host ready");
}

void usb_host_set_device_callback(usb_host_device_cb_t cb) {
    s_device_cb = cb;
}

bool usb_host_send_bulk(const uint8_t *data, uint32_t len) {
    if (s_dev_addr == 0 || s_ep_out == 0) {
        ESP_LOGE(TAG, "No device");
        return false;
    }
    if (s_busy) {
        ESP_LOGW(TAG, "Busy");
        return false;
    }

    static uint8_t tx_buf[256];
    if (len >= sizeof(tx_buf)) {
        ESP_LOGE(TAG, "Data too long");
        return false;
    }

    memcpy(tx_buf, data, len);
    tx_buf[len] = '\0';

    tuh_xfer_t xfer = {
        .daddr = s_dev_addr,
        .ep_addr = s_ep_out,
        .buffer = tx_buf,
        .buflen = len + 1,
        .complete_cb = xfer_complete,
        .user_data = 0,
    };

    s_busy = true;
    if (!tuh_edpt_xfer(&xfer)) {
        s_busy = false;
        ESP_LOGE(TAG, "Xfer submit failed");
        return false;
    }

    /* 等待传输完成（最多 5 秒） */
    if (xSemaphoreTake(s_xfer_done, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGW(TAG, "Xfer timeout");
        s_busy = false;
    }

    return true;
}

/* ---- 主循环中调用 ---- */
void usb_host_task(void) {
    tuh_task();

    /* 检测新设备并解析端点 */
    if (s_dev_addr == 0) {
        for (uint8_t addr = 1; addr <= CFG_TUH_DEVICE_MAX; addr++) {
            if (tuh_mounted(addr)) {
                uint8_t desc[512];
                if (tuh_descriptor_get_configuration_sync(addr, 0, desc, sizeof(desc))) {
                    /* 解析描述符找 Bulk OUT 端点 */
                    uint8_t *ptr = desc;
                    uint8_t ep_out = 0;
                    while (ptr < desc + sizeof(desc) && ptr[0] != 0) {
                        if (ptr[1] == TUSB_DESC_ENDPOINT) {
                            if ((ptr[2] & TUSB_DIR_IN_MASK) == 0 &&
                                (ptr[3] & TUSB_XFER_BULK) != 0) {
                                ep_out = ptr[2];
                                break;
                            }
                        }
                        ptr += ptr[0];
                    }

                    if (ep_out != 0) {
                        if (tuh_edpt_open(addr, ptr)) {
                            s_dev_addr = addr;
                            s_ep_out = ep_out;
                            ESP_LOGI(TAG, "Device %d EP OUT=0x%02x", addr, ep_out);
                            if (s_device_cb) {
                                s_device_cb(addr, ep_out);
                            }
                        }
                    }
                }
            }
        }
    }
}