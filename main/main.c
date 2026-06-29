#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "hid_handler.h"
#include "script_runner.h"

static const char *TAG = "main";

#define LED_GPIO        GPIO_NUM_2
#define COOLDOWN_MS     30000   /* 脚本完成后冷却 30 秒，防止 fastboot→Android 重连误触发 */

typedef enum {
    STATE_IDLE,       /* 等待手机连接 */
    STATE_RUNNING,    /* 脚本执行中 */
    STATE_COOLDOWN,   /* 冷却期，忽略重连事件 */
} state_t;

static volatile state_t s_state = STATE_IDLE;
static volatile bool s_trigger_pending = false;

/* USB 连接/断开 事件回调 */
static void on_usb_event(bool mounted) {
    if (mounted) {
        ESP_LOGI(TAG, "Phone connected");
        if (s_state == STATE_IDLE && !script_runner_is_running()) {
            s_trigger_pending = true;
        }
    } else {
        ESP_LOGI(TAG, "Phone disconnected");
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, " ESP32-C3 USB Fastboot Tool v2.0");
    ESP_LOGI(TAG, " Auto-trigger on phone connect");
    ESP_LOGI(TAG, "====================================");

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* LED */
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 1);

    /* 初始化 USB HID，注册连接事件回调 */
    hid_init();
    hid_set_event_callback(on_usb_event);

    /* 初始化脚本 */
    script_runner_init();

    ESP_LOGI(TAG, "Ready. Connect phone via USB-C to auto-trigger.");

    while (1) {
        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        switch (s_state) {

        case STATE_IDLE:
            if (s_trigger_pending) {
                s_trigger_pending = false;
                s_state = STATE_RUNNING;
                ESP_LOGI(TAG, "Auto-trigger: executing script...");
                script_runner_execute();
            }
            break;

        case STATE_RUNNING:
            if (!script_runner_is_running()) {
                /* 脚本执行完毕，进入冷却期 */
                s_state = STATE_COOLDOWN;
                ESP_LOGI(TAG, "Cooldown started (%d ms)", COOLDOWN_MS);
            }
            break;

        case STATE_COOLDOWN: {
            uint32_t completed = script_runner_completed_at();
            if (now - completed >= COOLDOWN_MS) {
                s_state = STATE_IDLE;
                ESP_LOGI(TAG, "Cooldown ended, ready for next trigger");
            }
            break;
        }
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}