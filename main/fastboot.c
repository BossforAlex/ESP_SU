#include "fastboot.h"
#include "usb_host.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "fastboot";
static volatile bool s_running = false;

#define LED_GPIO       GPIO_NUM_2
#define CMD_DELAY_MS   2000

/* =============================================================
 * Fastboot 指令序列
 * ============================================================= */
static const char *fastboot_cmds[] = {
    "oem set-gpu-preemption 0 androidboot.selinux=permissive",
    "continue",
};

static int s_cmd_count = sizeof(fastboot_cmds) / sizeof(fastboot_cmds[0]);

static void led_blink(int count, int delay_ms) {
    for (int i = 0; i < count; i++) {
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void fastboot_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "=== Fastboot started ===");
    led_blink(3, 100);

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (int i = 0; i < s_cmd_count && s_running; i++) {
        ESP_LOGI(TAG, "[%d/%d] %s", i + 1, s_cmd_count, fastboot_cmds[i]);

        if (!usb_host_send_bulk((const uint8_t *)fastboot_cmds[i],
                                 strlen(fastboot_cmds[i]))) {
            ESP_LOGW(TAG, "Send failed for command %d", i);
        }

        vTaskDelay(pdMS_TO_TICKS(CMD_DELAY_MS));
    }

    ESP_LOGI(TAG, "=== Fastboot done ===");
    led_blink(1, 500);
    s_running = false;
    vTaskDelete(NULL);
}

void fastboot_init(void) {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 1);
}

bool fastboot_is_running(void) {
    return s_running;
}

void fastboot_on_device(uint8_t dev_addr, uint8_t ep_out) {
    if (s_running) return;

    s_running = true;
    xTaskCreate(fastboot_task, "fastboot", 4096, NULL, 5, NULL);
}