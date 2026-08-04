# 🌊 ESP32-C6 Smart Water Tank Level Monitor

An industrial-grade, IoT-enabled **Smart Water Tank Level Monitoring System** built on **ESP32-C6** (ESP-IDF 6.x / PlatformIO). 

It features real-time **Blynk Cloud IoT monitoring**, instant mobile push notifications, 24/7 online heartbeat telemetry, and **bilingual voice announcements in English & Telugu** played through a MAX98357A I2S Class-D amplifier.

---

## 🌟 Key Features

- **📶 Blynk IoT Cloud Integration**: Monitor water percentage, tank status, and probe states anywhere in the world via Blynk Mobile App & Web Console.
- **🔊 Bilingual Voice Announcements**: Real-time audio alerts in **English** and **Telugu** (*"Tank Empty / ట్యాంక్ ఖాళీగా ఉంది"*, *"Tank Full / ట్యాంక్ నిండిపోయింది"*).
- **🔊 MAX98357A I2S Audio**: 16000 Hz, 16-bit Mono PCM audio with **+12 dB 4.0x software digital gain boost** and dynamic audio normalization (`dynaudnorm`).
- **📱 Instant Mobile Push Notifications**: Sends immediate push alerts to iOS/Android smartphones on water level state changes.
- **💚 24/7 Telemetry & Heartbeat**: Periodic 15-second heartbeat sync ensures the device remains marked **ONLINE (Green Dot)** continuously.
- **🛡️ Fault Detection**: Debounced GPIO sensor polling with automatic sensor fault detection for improper probe contact.
- **⚡ Custom Flash Partitioning**: Custom 3 MB factory app partition for Wi-Fi + TLS + mbedTLS certificate bundle + embedded PCM voice arrays.

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

## 🗣️ Bilingual Audio Announcements

| Water Level | English Announcement | Telugu Announcement |
| :--- | :--- | :--- |
| **Tank Empty (0%)** | *"Tank Empty. Please turn on the motor."* | *"ట్యాంక్ ఖాళీగా ఉంది. దయచేసి నీటి పంపు ఆన్ చేయండి."* |
| **Level Low (22%)** | *"Water Level Low."* | *"నీటి మట్టం తక్కువగా ఉంది. ఇరవై రెండు శాతం."* |
| **Level Medium (61%)** | *"Water Level Sixty One Percent."* | *"నీటి మట్టం అరవై ఒక్క శాతం ఉంది."* |
| **Tank Full (100%)** | *"Tank Full. Please turn off the motor."* | *"ట్యాంక్ నిండిపోయింది. దయచేసి నీటి పంపు ఆఫ్ చేయండి."* |

---

## 🚀 Building & Flashing

### Prerequisites
- [PlatformIO Core](https://platformio.org/) or PlatformIO extension in VS Code / Anti-Gravity IDE.
- ESP32-C6 DevKitC-1 connected via UART bridge USB port.

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

## 📄 License
Distributed under the MIT License. See `LICENSE` for more information.
