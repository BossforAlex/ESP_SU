# ESP32-S3 USB Fastboot Tool

基于 ESP32-S3 的 USB Host 外设，手机进入 fastboot 模式后自动发送 fastboot 指令。

**参考项目**：[rp2040-fastboot-rebooter](https://github.com/AHA1GE/rp2040-fastboot-rebooter)

**全程只需连接 ESP32-S3，无需电脑、无需 Termux、无需 ADB。**

## 工作原理

```
ESP32-S3 作为 USB 主机（Host）
手机进入 fastboot 模式 → 手机作为 USB 设备
  → ESP32-S3 检测到 fastboot 设备
  → 通过 USB Bulk 端点直接发送 fastboot 协议指令：
    1. fastboot oem set-gpu-preemption 0 androidboot.selinux=permissive
    2. fastboot continue
  → 手机以宽容模式启动
  → 30 秒冷却后，等待下次连接
```

## 与 ESP32-C3 HID 方案的区别

| | 旧方案 (HID 键盘) | 新方案 (USB Host) |
|------|------|------|
| 芯片 | ESP32-C3 | ESP32-S3 |
| 模式 | USB Device | **USB Host** |
| 需要 Termux | 是 | **否** |
| 需要 ADB | 是 | **否** |
| 通信方式 | 模拟键盘打字 | **直接 USB Bulk 传输** |

## 为什么 ESP32-C3 不行

ESP32-C3 只有 USB Serial/JTAG 控制器（Device 模式），不支持 USB OTG（Host 模式）。要实现 USB Host 模式，必须使用带有 USB OTG 的芯片：

- **ESP32-S3**：支持 USB OTG，可作 USB Host
- **ESP32-S2**：支持 USB OTG，可作 USB Host  
- **RP2040**：支持 USB Host，参考项目已实现

## 硬件要求

- **ESP32-S3** 开发板（如 Waveshare ESP32-S3-Zero、ESP32-S3-DevKitC 等）
- USB-C 转接线（连接手机）

## 刷写固件

### 从 Release 下载

前往 [Releases](../../releases/latest) 下载 `merged-flash.bin`。

### 刷写

```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 write_flash 0x0 merged-flash.bin
```

## 使用方式

1. 将手机进入 fastboot 模式（关机后按住音量下+电源键）
2. 用 USB 线连接 ESP32-S3 和手机
3. ESP32-S3 自动检测并发送 fastboot 指令
4. 手机自动启动进入宽容模式

## 项目结构

```
├── main/
│   ├── main.c            # 主程序（状态机）
│   ├── usb_host.h/c      # USB Host 驱动（TinyUSB）
│   ├── fastboot.h/c      # Fastboot 协议指令
│   └── tusb_config.h     # TinyUSB Host 配置
├── CMakeLists.txt        # ESP-IDF 项目配置
├── partitions.csv        # 分区表
├── sdkconfig.defaults    # 配置覆盖
└── .github/workflows/    # CI/CD 自动编译
```

## 构建

```bash
idf.py set-target esp32s3
idf.py build
```