# 🌊 ESP32-C6 Production Smart Water Tank Monitor Firmware

Enterprise-grade, highly reliable, modular ESP-IDF firmware for the **ESP32-C6 DevKitC-1 v1.2** platform. Integrates MAX98357A I2S audio announcements (Telugu + English), dual-router Wi-Fi auto-failover, Blynk IoT telemetry, Sinric Pro Google Assistant / Home integration, and night sleep power management.

---

## 🏗️ Architecture & Component Layout

```text
esp32c6_max98357a_sine/
├── CMakeLists.txt              # Root build configuration
├── main/
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild       # idf.py menuconfig hardware pin setup
│   ├── app_main.c / .h         # Subsystem orchestrator
├── components/
│   ├── drivers/                # Hardware Abstraction Layer
│   │   ├── water_sensor.c/h    # Probes polling & debouncing (GPIO10, 11, 23)
│   │   ├── audio_player.c/h    # MAX98357A I2S driver (GPIO18, 19, 20)
│   │   ├── gpio_config.c/h     # Pinout configuration
│   │   └── audio/              # PCM Telugu & English voice clips
│   ├── cloud/                  # Network & Cloud Integrations
│   │   ├── wifi_manager.c/h    # Dual-router failover + exponential backoff
│   │   ├── wifi_prov.c/h       # Secure NVS credential manager
│   │   ├── blynk_client.c/h    # Blynk IoT REST API client
│   │   └── sinric_client.c/h   # Sinric Pro WebSocket client (Google Home)
│   └── services/               # System Services & Telemetry
│       ├── app_events.c/h      # Event groups, queues, TLS mutex
│       ├── night_sleep.c/h     # Night sleep power management
│       └── sys_diagnostics.c/h # Health metrics monitor (Heap, RSSI, Uptime)
```

---

## 🔌 Hardware Pinout Table

| Peripheral | Signal | ESP32-C6 GPIO | Description |
|---|---|---|---|
| **MAX98357A** | BCLK | `GPIO19` | Bit Clock |
| **MAX98357A** | LRC / WS | `GPIO18` | Left/Right Word Select |
| **MAX98357A** | DIN | `GPIO20` | Data Input |
| **Probe (Low)** | Level 1 | `GPIO10` | Low Level Probe (Pull-up) |
| **Probe (Med)** | Level 2 | `GPIO11` | Medium Level Probe (Pull-up) |
| **Probe (Full)**| Level 3 | `GPIO23` | Full Level Probe (Pull-up) |

---

## 🚀 Building & Flashing

### Environment Setup
- **PlatformIO**: Core v6.1+ with `espressif32` platform.
- **ESP-IDF**: v6.0.1 (included via PlatformIO framework).

### Flash Firmware over COM6
```powershell
pio run --environment esp32-c6-devkitc-1 --target upload
```

### Serial Monitor
```powershell
pio device monitor --port COM6 --baud 115200 --filter time
```

---

## 🛡️ Production Security & Reliability Features

1. **Task Watchdog Timer (TWDT)**: 15-second hardware panic reset protection.
2. **Exponential Backoff Jitter**: Reconnect delay scaling `2s -> 60s` with ±20% jitter.
3. **Dual-Router Auto-Failover**: Auto-switches between Primary (`railwirefibernet`) and Secondary (`BSNL Fiber`).
4. **TLS Handshake Mutex**: Prevents simultaneous TLS heap spikes across cloud threads.
5. **Memory Sanitization**: Volatile zeroing of transient stack Wi-Fi credentials.
