# ESP32-C3 USB Fastboot Tool

基于 ESP32-C3 Super Mini 的 USB HID 外设，手机连接后自动通过 ADB/Fastboot 指令将 Android 设备设置为 SELinux 宽容模式。

**全程只需连接 ESP32，无需电脑。**

## 工作原理

```
手机开机 → 连接 ESP32（USB-C OTG）
  → ESP32 模拟 HID 键盘，自动键入：
    1. adb reboot bootloader          → 手机进入 Fastboot
    2. fastboot oem set-gpu-preemption 0 androidboot.selinux=permissive
    3. fastboot continue              → 手机以宽容模式启动
  → 30 秒冷却后，等待下次手机重启再次触发
```

## 硬件要求

- **ESP32-C3 Super Mini** 开发板（[购买链接](https://www.aliexpress.com/w/wholesale-esp32-c3-super-mini.html)）
- USB-C OTG 转接线（连接手机和 ESP32）

## 手机端配置（Termux）

由于 Android 没有内置终端，需要安装 Termux 来执行 ADB/Fastboot 指令。

### 1. 安装 Termux

从 **F-Droid** 下载安装（不要用 Google Play 版，已停更）：

https://f-droid.org/packages/com.termux/

### 2. 安装 Termux:Boot

从 F-Droid 安装 [Termux:Boot](https://f-droid.org/packages/com.termux.boot/)，打开一次以激活开机自启权限。

### 3. 配置 Termux

打开 Termux，依次执行：

```bash
# 更新包管理器
pkg update && pkg upgrade -y

# 安装 ADB 和 Fastboot
pkg install android-tools termux-api -y

# 配置开机自启
mkdir -p ~/.termux/boot
cat > ~/.termux/boot/start.sh << 'EOF'
#!/data/data/com.termux/files/usr/bin/bash
termux-wake-lock
adb start-server
EOF
chmod +x ~/.termux/boot/start.sh
```

### 4. 开启无线 ADB

手机设置 → 开发者选项 → 开启「无线调试」。

在 Termux 中执行一次配对（仅首次需要）：

```bash
adb pair localhost:XXXXX    # 输入无线调试页面显示的配对码和端口
adb connect localhost:XXXXX # 连接
```

### 5. 授权 Termux 后台弹出

手机设置 → 应用 → Termux → 开启「允许后台弹出界面」。

## 刷写固件

### 从 Release 下载

前往 [Releases](../../releases/latest) 页面下载 `merged-flash.bin`。

### 连接 ESP32-C3

按住 BOOT 键 → 插入 USB → 松开 BOOT 键，进入下载模式。

### 刷写

```bash
# 一键刷写合并固件
esptool.py --chip esp32c3 --port /dev/ttyUSB0 write_flash 0x0 merged-flash.bin
```

刷写完成后拔掉 USB，重新插入，ESP32 即开始工作。

## 使用方式

1. 手机开机，Termux 自动启动 ADB 服务
2. 打开 Termux 窗口（保持在屏幕）
3. 用 OTG 线连接 ESP32-C3
4. ESP32 自动检测连接，执行指令序列
5. 手机自动重启进入宽容模式

> 每次手机重启后，ESP32 都会自动重新执行脚本。

## 项目结构

```
├── main/
│   ├── main.c              # 主程序（状态机：IDLE→RUNNING→COOLDOWN）
│   ├── hid_handler.c/h     # USB HID 键盘驱动（TinyUSB）
│   └── script_runner.c/h   # 指令脚本执行器
├── CMakeLists.txt          # ESP-IDF 项目配置
├── partitions.csv          # 分区表（4MB Flash）
├── sdkconfig               # ESP32-C3 + TinyUSB 配置
└── .github/workflows/      # CI/CD 自动编译发布
```

## 构建

需要 ESP-IDF v5.x：

```bash
idf.py set-target esp32c3
idf.py build
```

或通过 GitHub Actions 自动编译（推送代码到 main 分支即触发）。