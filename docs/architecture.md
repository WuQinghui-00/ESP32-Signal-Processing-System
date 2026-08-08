# ESP32 信号处理系统 系统框图与数据流图

## 1. 系统架构框图

```mermaid
flowchart TB
    subgraph HW["硬件层"]
        DAC["DAC 输出 GPIO25"]
        ADC["ADC 输入 GPIO35"]
        SIG["外部信号发生器"]
    end

    subgraph DRV["驱动与采集"]
        DACDRV["dac_wave（esp_timer 定时驱动）"]
        ADCDRV["adc_sample（1kS/s 绝对时间戳调度）"]
    end

    subgraph DSP["信号处理"]
        DC["去直流（128 点均值）"]
        WIN["Hann 窗"]
        FFT["128 点 radix-2 FFT（ESP-DSP）"]
        PEAK["峰值检测（排除 DC bin）"]
    end

    subgraph NET["网络与展示"]
        WEB["HTTP Server（Canvas 频谱 + 诊断 API）"]
        MQTT["MQTT 上报（QoS1）"]
    end

    subgraph REL["可靠性/可观测性"]
        WIFI["WiFi 指数退避重连"]
        STATS["MQTT QoS1/栈/堆/RSSI 统计"]
        CACHE["断线缓存补发"]
    end

    SIG --> ADC
    DAC --> ADC
    ADC --> ADCDRV --> DC --> WIN --> FFT --> PEAK
    PEAK --> WEB
    PEAK --> MQTT
    PEAK --> STATS
    MQTT --> CACHE
    WIFI --> STATS
```

## 2. 信号数据流图

```mermaid
flowchart LR
    SRC["信号源（DAC 或信号发生器）"]
    A["ADC 采样 128 点 @1kS/s"]
    B["去直流：减去均值"]
    C["Hann 窗加权"]
    D["128 点 FFT"]
    E["幅值谱 0~64 bin"]
    F["峰值 bin → 频率 = bin×7.8125Hz"]
    G1["Web JSON（x=Hz, y=幅值）+ Canvas"]
    G2["MQTT：peak_frequency/amplitude/sample_rate"]

    SRC --> A --> B --> C --> D --> E --> F
    F --> G1
    F --> G2
```

## 3. 任务与并发模型

```mermaid
flowchart LR
    subgraph Tasks["任务"]
        Main["app_main 主循环（采集→FFT→上报）"]
        Uart["uart_task（串口命令）"]
        Httpd["httpd（Web 请求）"]
        MqttTask["esp-mqtt 任务"]
        WifiTask["esp_wifi 任务"]
        Timer["esp_timer（采样调度 + DAC 波形）"]
    end

    subgraph Shared["共享资源"]
        Spectrum["频谱快照（互斥锁）"]
        Stats["统计计数（WiFi/MQTT）"]
    end

    Main --> Spectrum
    Httpd --> Spectrum
    Main --> Stats
    MqttTask --> Stats
    Uart --> Timer
    Main --> Timer
```

## 4. 可靠性机制时序（断网→恢复）

```mermaid
sequenceDiagram
    participant W as WiFi 事件
    participant T as 重连定时器
    participant M as MQTT
    participant C as 缓存
    W->>T: 断线，按退避延时调度（1s→2s→4s…）
    T->>W: 定时到点，发起重连
    W->>M: 断线期间，每 2s 缓存最新状态
    W->>W: 热点恢复，重连成功（Got IP）
    W->>M: MQTT 重连
    M->>M: 补发缓存的最后一条（Resent cached state）
    M->>M: 恢复正常周期上报
```

## 5. 说明

- 全部共享数据（频谱快照、统计计数）通过互斥锁保护，避免跨任务数据竞争。
- Web 频谱与诊断接口（`/api/status`、`/api/metrics`）复用同一锁。
- DAC 波形由 esp_timer 驱动（真机测试修复），不占用任务 CPU。
