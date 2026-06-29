#include "hid_handler.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"

static const char *TAG = "hid_handler";
static SemaphoreHandle_t s_ready_sem = NULL;
static volatile bool s_mounted = false;
static hid_event_cb_t s_event_cb = NULL;

/* USB HID Keyboard Report Descriptor */
static const uint8_t hid_keyboard_report_desc[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD))
};

static void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report,
                                        uint16_t len) {
    (void)instance;
    (void)report;
    (void)len;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t *buffer,
                                uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type, uint8_t const *buffer,
                            uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

void tud_mount_cb(void) {
    ESP_LOGI(TAG, "USB mounted");
    s_mounted = true;
    xSemaphoreGive(s_ready_sem);
    if (s_event_cb) s_event_cb(true);
}

void tud_umount_cb(void) {
    ESP_LOGI(TAG, "USB unmounted");
    s_mounted = false;
    if (s_event_cb) s_event_cb(false);
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
}

void tud_resume_cb(void) {
}

void hid_init(void) {
    s_ready_sem = xSemaphoreCreateBinary();
    if (s_ready_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    const tinyusb_config_hid_t hid_cfg = {
        .report_descriptor = hid_keyboard_report_desc,
        .report_descriptor_len = sizeof(hid_keyboard_report_desc),
        .report_callback = tud_hid_report_complete_cb,
        .use_out_report = false,
        .report_id = 0,
        .protocol_mode = HID_PROTOCOL_NONE,
    };

    ESP_ERROR_CHECK(tusb_hid_keyboard_init(&hid_cfg));
    ESP_LOGI(TAG, "HID keyboard initialized");
}

void hid_set_event_callback(hid_event_cb_t cb) {
    s_event_cb = cb;
}

bool hid_is_connected(void) {
    return s_mounted;
}

bool hid_is_ready(void) {
    if (s_ready_sem == NULL) return false;
    return xSemaphoreTake(s_ready_sem, 0) == pdTRUE;
}

void hid_send_key(uint8_t modifier, uint8_t keycode) {
    if (!tud_hid_ready()) {
        return;
    }
    uint8_t report[8] = {modifier, 0, keycode, 0, 0, 0, 0, 0};
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, report);
    vTaskDelay(pdMS_TO_TICKS(5));
}

void hid_release_all(void) {
    if (!tud_hid_ready()) return;
    uint8_t report[8] = {0};
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, report);
    vTaskDelay(pdMS_TO_TICKS(5));
}

static uint8_t char_to_keycode(char c) {
    if (c >= 'a' && c <= 'z') return HID_KEY_A + (c - 'a');
    if (c >= 'A' && c <= 'Z') return HID_KEY_A + (c - 'A');
    if (c >= '0' && c <= '9') return HID_KEY_0 + (c - '0');
    if (c >= '1' && c <= '9') return HID_KEY_1 + (c - '1');

    switch (c) {
        case '0': return HID_KEY_0;
        case ' ': return HID_KEY_SPACE;
        case '.': return HID_KEY_PERIOD;
        case '/': return HID_KEY_SLASH;
        case ':': return HID_KEY_SEMICOLON;
        case ';': return HID_KEY_SEMICOLON;
        case '-': return HID_KEY_MINUS;
        case '_': return HID_KEY_MINUS;
        case '=': return HID_KEY_EQUAL;
        case '[': return HID_KEY_BRACKET_LEFT;
        case ']': return HID_KEY_BRACKET_RIGHT;
        case '\\': return HID_KEY_BACKSLASH;
        case '\'': return HID_KEY_APOSTROPHE;
        case ',': return HID_KEY_COMMA;
        case '\n': return HID_KEY_ENTER;
        default: return 0;
    }
}

static bool char_need_shift(char c) {
    return (c >= 'A' && c <= 'Z') || c == ':' || c == '_' || c == '\\';
}

void hid_send_string(const char *text) {
    while (*text) {
        char c = *text++;
        uint8_t keycode = char_to_keycode(c);
        if (keycode == 0) continue;
        uint8_t modifier = char_need_shift(c) ? KEYBOARD_MODIFIER_LEFTSHIFT : 0;
        hid_send_key(modifier, keycode);
        hid_release_all();
        vTaskDelay(pdMS_TO_TICKS(8));
    }
}

void hid_send_enter(void) {
    hid_send_key(0, HID_KEY_ENTER);
    hid_release_all();
    vTaskDelay(pdMS_TO_TICKS(50));
}