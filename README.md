# ESP32-CAM Telegram Bot

A security monitoring system using ESP32-CAM that sends images to Telegram based on motion detection or time intervals, with remote control via Telegram commands.

## Features
- Real-time video streaming via web interface
- Motion detection with adjustable sensitivity
- Time-based automated captures
- Telegram integration for alerts and photo sharing
- **Telegram bot commands for remote control**
- Web-based configuration interface
- Statistics and logging
- Automatic message offset tracking

## Hardware Requirements
- ESP32-CAM module (AI Thinker)
- FTDI programmer (for USB to Serial)
- 5V power supply (minimum 2A recommended)
- OV2640 camera module
- MicroSD card (optional, for logging)

## Telegram Bot Commands

### Basic Commands
- `/start` or `/help` - Show available commands
- `/status` or `/info` - Get camera status (IP, uptime, stats)
- `/settings` - View current configuration

### Capture Commands
- `/capture` or `/photo` - Take immediate photo
- `/test` - Test Telegram connection with photo

### Control Commands
- `/stream` - Get live stream URL
- `/reboot` or `/restart` - Restart the ESP32
- `/motion_on` - Enable motion detection
- `/motion_off` - Disable motion detection

## Installation

### 1. Clone the repository
```bash
git clone https://github.com/emadpanah/esp32cam-telegrambot.git
cd esp32cam-telegrambot
