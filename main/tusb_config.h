#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#define CFG_TUSB_MCU          OPT_MCU_ESP32S3
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_HOST | OPT_MODE_FULL_SPEED)
#define CFG_TUSB_OS           OPT_OS_FREERTOS
#define CFG_TUSB_DEBUG        0

/* USB Host 配置 */
#define CFG_TUH_HUB           0
#define CFG_TUH_CDC           0
#define CFG_TUH_MSC           0
#define CFG_TUH_HID           0
#define CFG_TUH_VENDOR        0

/* 启用自定义类用于 fastboot 协议 */
#define CFG_TUH_DEVICE_MAX    1
#define CFG_TUH_ENUMERATION_BUFSIZE 512

/* 内存配置 */
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN    __attribute__((aligned(4)))

#endif