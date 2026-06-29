#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "hid_handler.h"
#include "script_runner.h"

static const char *TAG = "main";

#define TRIGGER_GPIO  GPIO_NUM_0
#define LED_GPIO      GPIO_NUM_2

static uint32_t last_trigger_time = 0;

void app_main(void) {
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, " ESP32-C3 USB Fastboot Tool v1.0");
    ESP_LOGI(TAG, "====================================");

    /* 初始化 NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 初始化 USB HID 键盘 */
    hid_init();

    /* 初始化脚本运行器 */
    script_runner_init();

    ESP_LOGI(TAG, "System ready. Connect phone via USB-C.");
    ESP_LOGI(TAG, "Press GPIO0 (BOOT button) to trigger script.");

    /* 主循环：检测按键触发 */
    while (1) {
        /* GPIO0 拉低 = 按键按下，带 1 秒防抖 */
        if (gpio_get_level(TRIGGER_GPIO) == 0) {
            uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (now - last_trigger_time > 1000) {
                last_trigger_time = now;

                if (!script_runner_is_running()) {
                    ESP_LOGI(TAG, "Button pressed, executing script...");
                    script_runner_execute();
                } else {
                    ESP_LOGI(TAG, "Button pressed, aborting script...");
                    script_runner_abort();
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}