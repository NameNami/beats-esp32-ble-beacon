# BEATS ESP32 BLE Beacon Firmware

## Overview
This repository contains the firmware for the **BEATS (BLE-based Gamified University Attendance System)** beacons. The system uses ESP32-based hardware to act as iBeacons, providing a secure and dynamic way to track student attendance in university classrooms.

## System Flow
1. **Registration:** An administrator notes the physical MAC address of the ESP32 beacon and assigns it to a specific room/location in the BEATS Laravel backend.
2. **Identification:** Upon boot, the beacon connects to Wi-Fi and sends a heartbeat to the server containing its MAC address.
3. **Dynamic Configuration:** The server responds with a unique **UUID**. This UUID is dynamic and is rotated every 5 minutes by a Laravel cron job to prevent attendance spoofing or "beacon cloning."
4. **Advertising:** The firmware parses the UUID and immediately begins (or updates) BLE advertising using the iBeacon protocol.

## Hardware Indicators (LEDs)
The firmware uses on-board LEDs to communicate its current status:
*   **RED LED (GPIO 4):** System Power/Startup. Stays ON to indicate the BLE stack is active.
*   **BLUE LED (GPIO 2):**
    *   **Single Blink:** Successful heartbeat sent to server (Status 200).
    *   **Double Blink:** Successful UUID update/rotation applied.
    *   **Steady ON:** Wi-Fi or Server Connection failed.

## Features
*   **Dynamic UUID Rotation:** Updates the iBeacon UUID in real-time without restarting the device.
*   **Multi-WiFi Fallback:** Supports a list of prioritized Wi-Fi networks for maximum uptime.
*   **Encrypted Communication:** Uses HTTPS/SSL for server heartbeats.
*   **NimBLE Stack:** Built using the efficient NimBLE host stack for optimized power and memory usage on ESP-IDF v6.0+.

## Requirements
*   **ESP-IDF v6.0+**
*   **Managed Components:** `espressif/cjson` (automatically handled by Component Manager).

## Getting Started

### Set Target
Set the correct chip target (default is esp32):
```shell
idf.py set-target esp32
```

### Configure & Build
1. Update Wi-Fi credentials in `main/main.c`.
2. Update the server URL and root certificate if necessary.
3. Build and flash:
```shell
idf.py build flash monitor
```

## Project Structure
*   `main/main.c`: Core logic, Wi-Fi management, and API heartbeat task.
*   `main/src/gap.c`: BLE/NimBLE configuration and dynamic iBeacon advertising.
*   `main/idf_component.yml`: Dependency management for cJSON.
