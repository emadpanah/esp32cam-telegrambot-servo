

# ESP32-CAM Telegram Bot

A **stable security monitoring system** using **ESP32-CAM (AI Thinker)** that captures images based on **motion detection** or **time intervals**, streams live video via a web interface, and sends alerts/photos to **Telegram** with full remote control.

---

## Features

* Real-time camera streaming via web interface
* Motion detection with adjustable sensitivity
* Time-based automated image captures
* Telegram integration for alerts and photo delivery
* **Telegram bot commands for full remote control**
* Web-based configuration panel (no reboot required)
* Statistics and activity logging
* Automatic Telegram update offset tracking (no duplicate commands)
* **Tehran time support (UTC +3:30 via NTP)**
* Flash-safe EEPROM persistence (throttled writes)
* Stable long-running design (no reboot loops)

---

## Hardware Requirements

* ESP32-CAM module (**AI Thinker**)
* FTDI programmer (USB to Serial)
* 5V power supply (**minimum 2A recommended**)
* OV2640 camera module
* MicroSD card (**optional**, not required)

---

## Telegram Bot Commands pack

### Basic Commands

* `/start` or `/help` – Show available commands
* `/status` or `/info` – Camera status (IP, uptime, WiFi, stats)
* `/settings` – View current configuration

### Capture Commands

* `/capture` or `/photo` or `/pic` – Take immediate photo
* `/test` – Test Telegram connection (text + photo)

### Control Commands

* `/stream` – Get live web stream URL
* `/motion_on` – Enable motion detection
* `/motion_off` – Disable motion detection
* `/reboot` or `/restart` – Safe reboot (no restart loop)

### Debug Commands

* `/debug` – Memory usage, PSRAM, reset reason, uptime

---

## Web Interface

Available at:

```
http://<ESP32-IP>/
```

Web UI features:

* Live camera stream
* Manual photo capture
* Telegram connection test
* Change capture mode (Motion / Time / Mixed)
* Adjust time interval and motion sensitivity
* Live statistics and logs
* `/debug` endpoint for system diagnostics

---

## Installation

### 1. Clone the repository

```bash
git clone https://github.com/emadpanah/esp32cam-telegrambot.git
cd esp32cam-telegrambot
```

### 2. Configure

* Copy `config.example.h` → `config.h`
* Set WiFi credentials and Telegram bot info
* **Do NOT commit real tokens**

### 3. Upload Firmware

Recommended Arduino IDE settings:

* Board: **ESP32 Wrover Module**
* Partition Scheme: **Huge APP**
* PSRAM: **Enabled**
* Flash Frequency: **40MHz**

---

## Notes

* Time is synchronized via NTP and displayed in **Tehran local time**
* EEPROM writes are throttled to prevent flash wear
* Telegram photo uploads use streaming (low memory usage)
* Designed for 24/7 continuous operation

---

## Author

**Emad Panah**
ESP32 • Embedded Systems • Security Automation

GitHub: [https://github.com/emadpanah](https://github.com/emadpanah)

---
