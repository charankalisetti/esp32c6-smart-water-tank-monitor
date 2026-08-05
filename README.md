# 🌊 ESP32-C6 Smart Water Tank Level Monitor

An industrial-grade, multi-cloud **Smart Water Tank Level Monitoring System** built on **ESP32-C6** using ESP-IDF 6.x and PlatformIO.

It features **Dual Wi-Fi Router Auto-Failover**, **BLE Mobile Provisioning**, real-time **Blynk Cloud IoT monitoring**, **Google Home & Google Assistant integration** via Sinric Pro, **Scheduled Night Deep Sleep**, and **bilingual voice announcements in English & Telugu** played through a MAX98357A I2S Class-D amplifier.

---

## 🌟 Key Features

- **🔄 Dual Wi-Fi Router Auto-Failover**: Maintains dual-network router entries. Automatically detects primary router outages and switches seamlessly to the backup router (`railwirefibernet` primary -> `BSNL Fiber` failover).
- **📲 BLE Wi-Fi Provisioning & Persistent NVS**: Wi-Fi credentials stored in persistent NVS Flash. When unconfigured, broadcasts Bluetooth LE setup (`Water-Monitor-Setup`) for mobile app configuration.
- **📶 Blynk IoT Cloud Integration**: Monitor water percentage, tank status, and probe states anywhere in the world via Blynk Mobile App & Web Console with instant push alerts.
- **🏠 Google Home & Google Assistant Support**: Full visual state updates in Google Home App and voice queries via Sinric Pro.
- **🗣️ Bilingual Voice Announcements**: Real-time audio alerts in **English** and **Telugu** with level-specific repetition rules:
  - **Tank Empty (0%)**: Repeats **5 times**.
  - **Level Low (22%)**: Repeats **2 times**.
  - **Level Medium (61%)**: Repeats **3 times**.
  - **Tank Full (100%)**: Repeats **5 times**.
- **⚡ Smart Interrupt Capability**: Immediately cancels ongoing announcement repetitions if the tank level changes mid-alert and plays the new level's announcement.
- **🌙 Scheduled Night Deep Sleep**: Enters ultra-low-power Deep Sleep (<10µA) during night hours (**11:00 PM – 4:00 AM IST**) with 4:00 AM auto-timer wake and emergency probe GPIO wakeup.
- **🔊 MAX98357A I2S Audio**: 16000 Hz, 16-bit Mono PCM audio with software digital gain boost.
- **💚 24/7 Telemetry & Heartbeat**: Periodic 15-second heartbeat sync ensures the device remains marked **ONLINE (Green Dot)** continuously during active hours.

---

## 📌 GPIO Pin Assignment (ESP32-C6 DevKitC-1)

| Hardware Component | ESP32-C6 Pin | Description |
| :--- | :--- | :--- |
| **Low Probe Sensor** | `GPIO10` | Digital Input (Internal Pull-Up enabled) |
| **Medium Probe Sensor** | `GPIO11` | Digital Input (Internal Pull-Up enabled) |
| **Full Probe Sensor** | `GPIO23` | Digital Input (Internal Pull-Up enabled) |
| **MAX98357A BCLK** | `GPIO19` | I2S Bit Clock |
| **MAX98357A LRC / WS** | `GPIO18` | I2S Word Select / Left-Right Clock |
| **MAX98357A DIN** | `GPIO20` | I2S Data Input |
| **Console UART0 TX** | `GPIO16` (UART0) | Serial Monitor (115200 baud) |
| **Console UART0 RX** | `GPIO17` (UART0) | Serial Input |

---

## 🔄 Dual Router Auto-Failover Logic

| Priority | Network SSID | Password | Function |
| :--- | :--- | :--- | :--- |
| **Router #1 (Primary)** | `railwirefibernet` | `Charan@1904` | Default connection |
| **Router #2 (Backup)** | `BSNL Fiber` | `OppoA59@239856` | Automatic failover when Primary is down |

If the primary router loses power or network access for 3 consecutive retries (~6s), the ESP32-C6 automatically reconfigures its station interface and connects to the secondary backup router.

---

## 🚀 Building & Flashing

### Prerequisites
- [PlatformIO Core](https://platformio.org/) or VS Code PlatformIO extension.
- ESP32-C6 DevKitC-1 connected via USB port.

### Build and Upload
```bash
# Clone the repository
git clone https://github.com/charankalisetti/esp32c6-smart-water-tank-monitor.git
cd esp32c6-smart-water-tank-monitor

# Compile firmware
pio run --environment esp32-c6-devkitc-1

# Upload firmware to ESP32-C6
pio run --environment esp32-c6-devkitc-1 --target upload

# Open Serial Monitor
pio device monitor --port COM6 --baud 115200
```

---

## 🏠 Google Home & Google Assistant Integration

The device supports **Google Home App** visual tiles and **Google Assistant** voice queries via [Sinric Pro](https://sinric.pro):

### Voice Commands Supported:
- 🗣️ *"Hey Google, what is the Water Tank Monitor status?"* -> **Google Assistant**: *"Water Tank Monitor level is 61%."*
- 🗣️ *"OK Google, is the water tank full?"* -> **Google Assistant**: *"Water Tank Monitor status is Full."*

---

## 📄 License
Distributed under the MIT License. See `LICENSE` for more information.
