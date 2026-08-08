# ESP32 实时信号处理与物联网系统

<div align="center">

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)](https://www.espressif.com/)
[![Framework](https://img.shields.io/badge/framework-ESP--IDF-red.svg)](https://docs.espressif.com/projects/esp-idf/)
[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))

**基于 ESP32 的边缘计算信号处理系统 | 波形生成 · 1kS/s 采集 · 128 点 FFT 频谱分析 · MQTT 云端上报 · Web Canvas 实时监控**

</div>

---

## 📖 项目简介

一套完整的嵌入式信号处理系统，从底层驱动到上层应用，覆盖**波形生成、ADC 采样校准、去直流、Hann 窗、128 点 FFT、峰值检测、WiFi/MQTT 上报、Web 实时频谱监控**的全链路闭环。基于 FreeRTOS 多任务 + ESP-IDF v5.3.1，并经过信号发生器 + 示波器真机验证，体现了嵌入式“采集 → 算法 → 网络 → 可观测性”的全栈能力。

---

## ✨ 功能特性

### 🔬 信号链路（第 2 阶段）

| 功能 | 说明 |
|------|------|
| **波形生成** | DAC 输出正弦/方波/三角波，频率可编程（默认 1kHz，验证用 ≤400Hz） |
| **采样校准** | 绝对时间戳调度，实测采样率约 1007 S/s（误差 <1%） |
| **去直流** | FFT 前减去 128 点均值，消除 DAC/信号源直流偏置对 0Hz bin 的淹没 |
| **真实 FFT** | 基于 ESP-DSP：Hann 窗 + 128 点 radix-2 FFT，频率分辨率 7.8125Hz |
| **峰值检测** | 排除 DC bin 后找最大幅值 bin，输出峰值频率与幅值 |
| **Web 频谱图** | HTML5 Canvas 实时绘制频谱曲线 + 峰值标记（每 2 秒刷新） |
| **并发安全** | Web 频谱数据由互斥锁保护，消除跨任务数据竞争 |

### 🛡️ 可靠性与可观测性（第 3 阶段）

| 功能 | 说明 |
|------|------|
| **WiFi 指数退避重连** | 断线后 1s→2s→4s…→30s 封顶，±20% 随机抖动，防重连风暴 |
| **MQTT 状态与 QoS1 统计** | 在线标志 + publish/ack/disconnect/error 计数，丢包可见 |
| **断线缓存补发** | 断线期间缓存最新状态，重连后自动补发一次 |
| **栈水位监控** | 每 60s 报告 main/uart_cmd 任务最小剩余栈（真机发现 main 栈偏紧） |
| **堆内存监控** | 当前空闲堆 + 开机以来最低空闲堆 |
| **WiFi 信号统计** | RSSI、累计断线次数、重连次数、断线 reason 码 |
| **诊断接口** | `/api/status`（健康摘要）、`/api/metrics`（全量计数 + 栈水位） |

---

## 🛠️ 技术栈

| 层级 | 技术 |
|------|------|
| 芯片 | ESP32（双核 Xtensa LX6） |
| RTOS | FreeRTOS |
| 驱动 | DAC（GPIO25）/ ADC（GPIO35，12bit，12dB 衰减） |
| 算法 | ESP-DSP：Hann 窗 + 128 点 radix-2 FFT |
| 定时器 | esp_timer（采样调度、DAC 波形输出） |
| 通信 | WiFi（WPA3 兼容）/ MQTT（QoS1）/ HTTP Web Server |
| 开发框架 | ESP-IDF v5.3.1 |

---

## 📁 代码结构

```
main/
├── main.c           # 主循环：采集→去直流→FFT→上报，栈/堆/信号监控
├── dac_wave.c/h     # DAC 波形生成（esp_timer 定时驱动）
├── adc_sample.c/h   # 1kS/s 绝对时间戳采样
├── fft_process.c/h  # Hann 窗 + 128 点 FFT + 峰值检测
├── mqtt_report.c/h  # WiFi/MQTT：退避重连、QoS1 统计、断线缓存补发
├── webserver.c/h    # Web 频谱 Canvas + /api/status + /api/metrics
├── idf_component.yml# 依赖：espressif/esp-dsp
└── CMakeLists.txt   # 构建配置
```

---

## 🔌 硬件连接

| 引脚 | 功能 |
|------|------|
| GPIO25 | DAC 输出（板载波形） |
| GPIO35 | ADC 输入（信号采集） |

**自测回路**：GPIO25 用杜邦线接到 GPIO35（内部 DAC → ADC）。

**外部信号发生器验证**：信号发生器输出接 GPIO35（0~3.3V 以内，建议正弦 Vpp≤2V、偏置约 1.65V，负载选 High-Z），发生器 GND 与板子 GND 共地。

> ⚠️ ADC 量程 0~3.3V：波形峰值**绝不能超过 3.3V**；采样率 1kS/s（奈奎斯特 500Hz），验证频率请用 **50~400Hz**，1kHz 会被混叠。

---

## 🚀 快速开始

### 1. 环境准备

安装 ESP-IDF v5.3.1（Windows 用户先执行 `export.ps1`）。

### 2. 克隆项目

```bash
git clone https://github.com/WuQinghui-00/ESP32-Signal-Processing-System.git
cd ESP32-Signal-Processing-System
```

### 3. 配置 WiFi 与 MQTT

修改 `main/mqtt_report.c`：

```c
#define WIFI_SSID "你的WiFi名称"
#define WIFI_PASS "你的WiFi密码"
#define MQTT_BROKER "mqtt://broker.emqx.io"
```

### 4. 编译烧录

```bash
idf.py set-target esp32
idf.py build
idf.py -p COM3 flash monitor
```

（首次构建会联网下载 esp-dsp 组件。）

### 5. 串口命令（可选）

| 命令 | 说明 |
|------|------|
| `SINE 200` | 输出 200Hz 正弦波（验证用 ≤400Hz） |
| `SQUARE 100` / `TRIANGLE 100` | 方波 / 三角波 |
| `STOP` / `START` | 停止 / 启动输出 |

---

## 📊 真机测试结果（第 4 阶段，信号发生器 + 示波器）

### FFT 频率验证（分辨率 7.8125Hz，全部 ±1 bin 内）

| 发生器设定 (Hz) | FFT 报频 (Hz) | 偏差 |
|---|---|---|
| 50 | 47 | -3 |
| 100 | 102 | +2 |
| 200 | 203 | +3 |
| 300 | 297 | -3 |
| 400 | 398 | -2 |

### 幅度线性（200Hz）

| 设定 (Vpp) | FFT amp | 比值 |
|---|---|---|
| 0.5 | ~8,800 | 1.00 |
| 1 | ~17,500 | 1.99 |
| 2 | ~35,700 | 4.06 |

### 可靠性验证

- WiFi 断网 → 指数退避重连（1s→2s→4s…）→ 自动恢复 → MQTT 缓存补发，全程无人工干预。
- MQTT QoS1：长时间运行 2400+ 条，除断线瞬间外 100% ACK。
- 连续运行 90+ 分钟无异常，堆余量约 170KB。

### 真机发现并修复的问题

1. `/spectrum/data` 接口卡死：httpd 任务默认栈 4096 被频谱 handler 撑爆 → `config.stack_size = 8192`。
2. Web 页面曲线不显示：C 字符串拼接的 HTML 单行传输，JS `//` 注释吞掉整段脚本 → 改用 `/* */` 块注释。
3. 任务看门狗报警：DAC 输出任务忙等占死 CPU → 改用 esp_timer 定时驱动。

---

## 🌐 Web 监控

1. 串口日志查看设备 IP（如 `172.20.10.2`）。
2. 浏览器访问 `http://<IP>/`：Canvas 频谱曲线 + 峰值频率 + 红色峰值标记。
3. 诊断接口：
   - `http://<IP>/api/status`：运行时长、WiFi/MQTT 在线、RSSI、堆水位
   - `http://<IP>/api/metrics`：全部计数（发布/ACK/断线/错误）+ 栈水位

## 📡 MQTT

- Broker：`broker.emqx.io`
- 上报主题：`/esp32/signal/freq`，QoS1，每 2 秒一条
- Payload：`{"peak_frequency":203,"peak_amplitude":35000,"sample_rate":1007}`

---

## 📈 项目亮点

| 亮点 | 说明 |
|------|------|
| **真实频谱链路** | 去直流 → Hann 窗 → 128 点 FFT → 峰值检测，仪器级交叉验证 |
| **可靠性设计** | 指数退避、QoS1 统计、断线缓存补发，AP 侧故障也能自动恢复 |
| **可观测性** | 栈/堆/信号监控 + `/api/status`、`/api/metrics` 诊断接口 |
| **真机测试闭环** | 发现并修复 3 个编译期发现不了的硬件/运行时问题 |
| **全栈能力** | 驱动 → 算法 → WiFi/MQTT → Web，FreeRTOS 多任务 |

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
- **CSDN**：[oxiaosui](https://blog.csdn.net/2501_92470428)

---

<div align="center">
⭐ 如果这个项目对你有帮助，欢迎 Star！
</div>
