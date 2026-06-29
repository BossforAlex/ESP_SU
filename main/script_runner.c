#include "script_runner.h"
#include "hid_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

static const char *TAG = "script_runner";
static volatile bool s_running = false;
static TaskHandle_t s_runner_task = NULL;

#define LED_GPIO      GPIO_NUM_2
#define TRIGGER_GPIO  GPIO_NUM_0

/* =============================================================
 * 指令列表 — 连接手机后通过 HID 键盘逐条发送
 * 功能：ADB 进 fastboot → 执行特定指令 → 进入宽容模式
 * ============================================================= */

typedef struct {
    const char *command;
    bool is_comment;   /* 仅注释，不发送 */
    uint32_t delay_ms; /* 发送后等待时间 */
} script_line_t;

static const script_line_t script[] = {
    /* ------ 阶段1: ADB 检测与连接 ------ */
    {.command = "adb devices", .is_comment = false, .delay_ms = 2000},
    {.command = "", .is_comment = true, .delay_ms = 0},  /* 等待设备响应 */

    /* ------ 阶段2: 获取 root 权限 ------ */
    {.command = "adb root", .is_comment = false, .delay_ms = 3000},
    {.command = "", .is_comment = true, .delay_ms = 0},

    /* ------ 阶段3: 设置 SELinux 为宽容模式(permissive) ------ */
    {.command = "adb shell setenforce 0", .is_comment = false, .delay_ms = 2000},
    {.command = "", .is_comment = true, .delay_ms = 0},

    /* ------ 阶段4: 验证 SELinux 状态 ------ */
    {.command = "adb shell getenforce", .is_comment = false, .delay_ms = 2000},
    {.command = "", .is_comment = true, .delay_ms = 0},

    /* ------ 阶段5: 进入 fastboot 模式 ------ */
    {.command = "adb reboot bootloader", .is_comment = false, .delay_ms = 5000},
    {.command = "", .is_comment = true, .delay_ms = 0},

    /* ------ 阶段6: Fastboot 解锁与宽容模式 ------ */
    {.command = "fastboot devices", .is_comment = false, .delay_ms = 2000},
    {.command = "", .is_comment = true, .delay_ms = 0},
    {.command = "fastboot oem unlock", .is_comment = false, .delay_ms = 3000},
    {.command = "", .is_comment = true, .delay_ms = 0},
    {.command = "fastboot flashing unlock", .is_comment = false, .delay_ms = 3000},
    {.command = "", .is_comment = true, .delay_ms = 0},
    {.command = "fastboot oem permissive", .is_comment = false, .delay_ms = 3000},
    {.command = "", .is_comment = true, .delay_ms = 0},

    /* ------ 阶段7: 重启手机 ------ */
    {.command = "fastboot reboot", .is_comment = false, .delay_ms = 5000},

    /* ------ 结束标记 ------ */
    {.command = NULL, .is_comment = true, .delay_ms = 0},
};

/* LED 闪烁指示 */
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
    ESP_LOGI(TAG, "=== Script execution started ===");

    led_blink(3, 150);

    /* 等待 USB HID 就绪 */
    ESP_LOGI(TAG, "Waiting for USB HID ready...");
    int wait_count = 0;
    while (!hid_is_ready() && wait_count < 100) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_count++;
    }
    if (!hid_is_ready()) {
        ESP_LOGE(TAG, "USB HID not ready, abort");
        led_blink(5, 100);
        s_running = false;
        vTaskDelete(NULL);
        return;
    }

    /* 延时 2 秒让手机识别键盘 */
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* 逐条执行指令 */
    for (int i = 0; script[i].command != NULL; i++) {
        if (!s_running) {
            ESP_LOGI(TAG, "Script aborted by user");
            break;
        }

        if (script[i].is_comment) {
            continue;
        }

        ESP_LOGI(TAG, "Sending: %s", script[i].command);
        hid_send_string(script[i].command);
        hid_send_enter();

        vTaskDelay(pdMS_TO_TICKS(script[i].delay_ms));
    }

    ESP_LOGI(TAG, "=== Script execution finished ===");
    led_blink(2, 300);

    s_running = false;
    vTaskDelete(NULL);
}

void script_runner_init(void) {
    /* 配置 LED GPIO */
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 1); /* 低电平点亮 */

    /* 配置触发按键 GPIO */
    gpio_reset_pin(TRIGGER_GPIO);
    gpio_set_direction(TRIGGER_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(TRIGGER_GPIO, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "Script runner initialized");
}

bool script_runner_is_running(void) {
    return s_running;
}

void script_runner_execute(void) {
    if (s_running) {
        ESP_LOGW(TAG, "Script already running");
        return;
    }
    s_running = true;
    xTaskCreate(runner_task, "script_runner", 4096, NULL, 5, &s_runner_task);
}

void script_runner_abort(void) {
    s_running = false;
    ESP_LOGI(TAG, "Script abort requested");
}