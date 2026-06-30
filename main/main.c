#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "usb_host.h"
#include "fastboot.h"

static const char *TAG = "main";
#define LED_GPIO      GPIO_NUM_2
#define COOLDOWN_MS   30000

typedef enum { STATE_IDLE, STATE_RUNNING, STATE_COOLDOWN } state_t;
static volatile state_t s_state = STATE_IDLE;
static volatile uint32_t s_completed_at = 0;

static void on_device(uint8_t dev_addr, uint8_t ep_out) {
    if (s_state == STATE_IDLE && !fastboot_is_running()) {
        s_state = STATE_RUNNING;
        fastboot_on_device(dev_addr, ep_out);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "====================================");
    ESP_LOGI(TAG, " ESP32-S3 USB Fastboot Tool v3.0");
    ESP_LOGI(TAG, " USB Host mode - direct fastboot");
    ESP_LOGI(TAG, "====================================");

    nvs_flash_init();

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 1);

    fastboot_init();
    usb_host_init();
    usb_host_set_device_callback(on_device);

    ESP_LOGI(TAG, "Ready. Connect phone in fastboot mode.");

    while (1) {
        usb_host_task();

        uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

        if (s_state == STATE_RUNNING && !fastboot_is_running()) {
            s_completed_at = now;
            s_state = STATE_COOLDOWN;
            ESP_LOGI(TAG, "Cooldown %dms", COOLDOWN_MS);
        } else if (s_state == STATE_COOLDOWN && now - s_completed_at >= COOLDOWN_MS) {
            s_state = STATE_IDLE;
            ESP_LOGI(TAG, "Ready");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}