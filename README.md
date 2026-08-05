# 🌊 ESP32-C6 Smart Water Tank Level Monitor

An industrial-grade, multi-cloud **Smart Water Tank Level Monitoring System** built on **ESP32-C6** using ESP-IDF 6.x and PlatformIO.

It features **Dual Wi-Fi Router Auto-Failover**, **BLE Mobile Provisioning**, real-time **Blynk Cloud IoT monitoring**, **Google Home & Google Assistant integration** via Sinric Pro, **Scheduled Night Deep Sleep**, and **bilingual voice announcements in English & Telugu** played through a MAX98357A I2S Class-D amplifier.

---

## 🌟 Key Features

- **🔔 12V Active Buzzer Module (PN2222A Transistor Switch)**: Non-blocking, non-interfering alarm sound patterns via **GPIO21** executed after voice announcements finish:
  - **Tank Empty (0%)**: Continuous **5-second alarm sound**.
  - **Level Low (22%)**: **Two beeps** (300 ms ON, 300 ms OFF).
  - **Level Medium (61%)**: Continuous **3-second alarm sound**.
  - **Tank Full (100%)**: **One confirmation beep** (500 ms ON).
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
| **12V Buzzer Transistor Switch** | `GPIO21` | Digital Output (PN2222A Base via 1kΩ Resistor) |
| **MAX98357A BCLK** | `GPIO19` | I2S Bit Clock |
| **MAX98357A LRC / WS** | `GPIO18` | I2S Word Select / Left-Right Clock |
| **MAX98357A DIN** | `GPIO20` | I2S Data Input |
| **Console UART0 TX** | `GPIO16` (UART0) | Serial Monitor (115200 baud) |
| **Console UART0 RX** | `GPIO17` (UART0) | Serial Input |

---

## ⚡ 12V Active Buzzer Hardware Schematic & Electronics Theory

### Circuit Diagram

```text
ESP32-C6 GPIO21 (3.3V) ────[ 1 kΩ Resistor ]──── Base (B)
                                                 PN2222A NPN
                                             Emitter (E) ──── Common GND
                                             Collector (C) ── Buzzer Negative (-)
                                                              Buzzer Positive (+) ── +12V Adapter (+)
                                                              12V Adapter GND ───── Common GND
```

### Hardware Rationale

1. **Why Transistor Switch is Required**:
   - ESP32-C6 GPIO pins output **3.3V DC** with a maximum safe current limit of **28 mA**.
   - A 12V active buzzer requires **12V DC** and draws 30–100 mA of current.
   - Connecting a 12V power source directly to an ESP32 GPIO pin would instantly destroy the microcontroller SoC due to overvoltage and overcurrent. The PN2222A NPN transistor acts as a high-voltage low-side electronic switch.

2. **Why Common Ground (GND) is Required**:
   - The transistor base-emitter forward voltage ($V_{BE} \approx 0.7\text{V}$) is referenced directly to the Emitter terminal (GND).
   - Linking the 12V DC power supply ground and the ESP32 ground provides a common reference point for base current $I_B$ to return to the ESP32 power supply, enabling proper saturation of the PN2222A.

3. **Current & Power Flow**:
   - **Control Circuit**: `GPIO21 HIGH` (3.3V) $\rightarrow$ 1 kΩ Base Resistor $\rightarrow$ $I_B \approx \frac{3.3\text{V} - 0.7\text{V}}{1000\ \Omega} = 2.6\text{ mA}$ into PN2222A Base $\rightarrow$ Emitter $\rightarrow$ Common GND.
   - **Load Circuit**: `+12V Adapter (+)` $\rightarrow$ `Buzzer (+)` $\rightarrow$ `Buzzer (-)` $\rightarrow$ `Collector (C)` $\rightarrow$ `Emitter (E)` $\rightarrow$ `Common GND`.

---

## 🔔 Alarm Pattern & Voice Announcement Table

| Water Level | Voice Announcement | Buzzer Alarm Pattern |
| :--- | :--- | :--- |
| **Tank Empty (0%)** | *"Tank Empty. Please turn on the motor."* (5x) | **Continuous Buzzer for 5 seconds** |
| **Level Low (22%)** | *"Water Level Low."* (2x) | **Two Beeps** (300 ms ON, 300 ms OFF) |
| **Level Medium (61%)** | *"Water Level Sixty One Percent."* (3x) | **Continuous Buzzer for 3 seconds** |
| **Tank Full (100%)** | *"Tank Full. Please turn off the motor."* (5x) | **One Confirmation Beep** (500 ms ON) |

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
