  # ESP32 实时信号处理与物联网系统

<div align="center">

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/framework-ESP--IDF-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

**基于 ESP32 的边缘计算信号处理系统 | 波形生成 · 实时采集 · 频谱分析 · 云端上报 · Web 监控**

</div>

---

## 📖 项目简介

本项目设计并实现了一套完整的嵌入式信号处理系统，从底层驱动到上层应用，涵盖了**波形生成、数据采集、FFT 频谱分析、WiFi 连接、MQTT 云端上报、Web 实时监控**的全链路闭环。采用 FreeRTOS 多任务架构，体现了嵌入式全栈开发能力。

---

## ✨ 功能特性

### 🔧 本地功能

| 功能 | 说明 |
|------|------|
| **波形生成** | DAC 输出正弦波/方波/三角波，频率 1-5000Hz 可编程 |
| **数据采集** | ADC 实现 1000Hz 采样率，128 点数据采集 |
| **频谱分析** | FFT 峰值频率检测，频域分辨率 7.8Hz |
| **多任务架构** | FreeRTOS 采集/处理/串口命令 3 个独立任务 |
| **串口命令** | 实时切换波形类型和频率 |

### 🌐 物联网功能

| 功能 | 说明 |
|------|------|
| **WiFi 连接** | 自动连接，支持断线重连 |
| **MQTT 上报** | 峰值频率上报至 EMQX 公共服务器 |
| **Web 监控** | 内置 HTTP 服务器，浏览器实时查看频谱数据 |

---

## 🛠️ 技术栈

| 层级 | 技术 |
|------|------|
| 芯片 | ESP32 |
| RTOS | FreeRTOS |
| 驱动开发 | DAC / ADC |
| 算法 | FFT 峰值检测 |
| 通信协议 | WiFi / MQTT / HTTP |
| 开发框架 | ESP-IDF v5.3.1 |

---

## 📁 代码结构

```
main/
├── main.c           # 主函数、任务创建、串口命令
├── dac_wave.c/h     # DAC 波形生成（正弦/方波/三角波）
├── adc_sample.c/h   # ADC 数据采集
├── fft_process.c/h  # FFT 峰值频率检测
├── mqtt_report.c/h  # WiFi + MQTT 云端上报
├── webserver.c/h    # HTTP Web 服务器
└── CMakeLists.txt   # 构建配置
```

---

## 🔌 硬件连接

| 引脚 | 功能 |
|------|------|
| GPIO25 | DAC 输出（波形生成）|
| GPIO35 | ADC 输入（数据采集）|

**连接方式**：使用杜邦线将 GPIO25 连接到 GPIO35

```
ESP32 GPIO25 ──────── 杜邦线 ──────── ESP32 GPIO35
```

---

## 🚀 快速开始

### 1. 环境准备

安装 ESP-IDF v5.3.1 或更高版本

### 2. 克隆项目

```bash
git clone https://github.com/WuQinghui-00/ESP32-Signal-Processing-System.git
cd ESP32-Signal-Processing-System
```

### 3. 配置 WiFi

修改 `main/mqtt_report.c` 中的 WiFi 配置：

```c
#define WIFI_SSID "你的WiFi名称"
#define WIFI_PASS "你的WiFi密码"
```

### 4. 编译烧录

```bash
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

### 5. 串口命令

烧录后在串口监视器中输入命令：

| 命令 | 说明 |
|------|------|
| `SINE 1000` | 输出 1000Hz 正弦波 |
| `SQUARE 500` | 输出 500Hz 方波 |
| `TRIANGLE 2000` | 输出 2000Hz 三角波 |
| `STOP` | 停止输出 |
| `START` | 开始输出 |

---

## 📊 串口输出示例

```
I (xxx) MAIN: =========================================
I (xxx) MAIN: Signal Processing System
I (xxx) MAIN: =========================================
I (xxx) WIFI: Got IP: 172.20.10.2
I (xxx) MQTT: MQTT connected
I (xxx) MAIN: Peak frequency: 1000 Hz
I (xxx) MQTT: Published: {"peak_frequency":1000}
```
<img width="1106" height="306" alt="image" src="https://github.com/user-attachments/assets/1726d148-c676-4b1e-9142-a4f1d81576bc" />

---

## 🌐 Web 监控界面

1. 烧录后串口会显示 ESP32 的 IP 地址
2. 浏览器访问 `http://<ESP32_IP>/`
3. 实时显示峰值频率和频谱数据（每 2 秒自动刷新）
📺 演示视频：[点击观看 Web 监控效果](https://www.bilibili.com/video/BV1zZXfBAE6A?vd_source=5f8e03673d870706c690f30b18ceac8c)

**界面预览**：
```
┌─────────────────────────────────┐
│   📊 ESP32 Spectrum Monitor     │
│                                 │
│      🎵 Peak Frequency: 734 Hz  │
│                                 │
│   Raw Data:                     │
│   {"peak_freq":734,"x":[...]}   │
│                                 │
│   Auto-refresh every 2 seconds  │
└─────────────────────────────────┘

```
<img width="1826" height="865" alt="image" src="https://github.com/user-attachments/assets/a23ff22f-46c5-4f19-a5fa-8764f753b6cb" />


---

## 📈 项目亮点

| 亮点 | 说明 |
|------|------|
| **边缘计算** | 设备端完成 FFT 分析，仅上报关键结果，降低带宽消耗 |
| **实时系统** | FreeRTOS 多任务架构，确保数据采集实时性 |
| **全栈能力** | 从驱动开发到云端上报到 Web 监控的完整链路 |
| **工程化** | 模块化代码结构，开源 GitHub，技术博客记录 |

---

## 📝 技术博客

- [ESP32 + FreeRTOS 智能光照监测系统](https://blog.csdn.net/2501_92470428/article/details/159498961)
- [ESP32 MQTT 上云实战](https://blog.csdn.net/2501_92470428/article/details/159562399)

---

## 📄 许可证

本项目基于 MIT 许可证开源。详见 [LICENSE](LICENSE) 文件。

---

## 🤝 贡献

欢迎提交 Issue 和 Pull Request。

---

## 📧 联系

- **作者**：吴青慧
- **邮箱**：1263105429@qq.com
- **GitHub**：[WuQinghui-00](https://github.com/WuQinghui-00)
- **CSDN**：[ooxiaosui](https://blog.csdn.net/2501_92470428)

---

<div align="center">
⭐ 如果这个项目对你有帮助，欢迎 Star！
</div>
```

---

