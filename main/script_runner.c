#include "script_runner.h"
#include "hid_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

static const char *TAG = "script_runner";
static volatile bool s_running = false;
static volatile uint32_t s_completed_at = 0;

#define LED_GPIO  GPIO_NUM_2

/* =============================================================
 * 指令序列 — 每次手机重启后自动执行
 * adb reboot bootloader → fastboot oem set-gpu-preemption 0
 *   androidboot.selinux=permissive → fastboot continue
 * ============================================================= */

typedef struct {
    const char *command;
    uint32_t delay_ms; /* 发送后等待 */
} script_line_t;

static const script_line_t script[] = {
    /* 进入 fastboot 模式 */
    {.command = "adb reboot bootloader", .delay_ms = 6000},

    /* 设置 GPU 抢占 + SELinux 宽容模式启动参数 */
    {.command = "fastboot oem set-gpu-preemption 0 androidboot.selinux=permissive", .delay_ms = 4000},

    /* 继续启动进入 Android */
    {.command = "fastboot continue", .delay_ms = 5000},
};

#define SCRIPT_LINES (sizeof(script) / sizeof(script[0]))

static void led_blink(int count, int delay_ms) {
    for (int i = 0; i < count; i++) {
        gpio_set_level(LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
        gpio_set_level(LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void runner_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "=== Script started ===");

    led_blink(2, 100);

    /* 等待 USB HID 就绪 */
    int wait_count = 0;
    while (!hid_is_ready() && wait_count < 150) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_count++;
    }
    if (!hid_is_ready()) {
        ESP_LOGE(TAG, "HID not ready, abort");
        led_blink(5, 80);
        s_running = false;
        vTaskDelete(NULL);
        return;
    }

    /* 等手机识别键盘 */
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* 逐条执行 */
    for (int i = 0; i < SCRIPT_LINES; i++) {
        if (!s_running) {
            ESP_LOGI(TAG, "Aborted");
            break;
        }

        ESP_LOGI(TAG, "[%d/%d] %s", i + 1, SCRIPT_LINES, script[i].command);
        hid_send_string(script[i].command);
        hid_send_enter();

        /* 等待指令执行 + 手机可能的断开/重连 */
        vTaskDelay(pdMS_TO_TICKS(script[i].delay_ms));
    }

    s_completed_at = xTaskGetTickCount() * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "=== Script done ===");
    led_blink(1, 500);

    s_running = false;
    vTaskDelete(NULL);
}

void script_runner_init(void) {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 1); /* 低电平点亮 */
    ESP_LOGI(TAG, "Initialized");
}

bool script_runner_is_running(void) {
    return s_running;
}

uint32_t script_runner_completed_at(void) {
    return s_completed_at;
}

void script_runner_execute(void) {
    if (s_running) {
        ESP_LOGW(TAG, "Already running");
        return;
    }
    s_running = true;
    xTaskCreate(runner_task, "script_runner", 4096, NULL, 5, NULL);
}

void script_runner_abort(void) {
    s_running = false;
    ESP_LOGI(TAG, "Abort requested");
}