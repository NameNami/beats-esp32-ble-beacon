# BEATS ESP32 BLE Beacon Firmware

## Overview
This repository contains the firmware for the **BEATS (BLE-based Gamified University Attendance System)** beacons. The system uses ESP32-based hardware to act as iBeacons, providing a secure and dynamic way to track student attendance in university classrooms.

This firmware works in tandem with the [BEATS VILT Backend](https://github.com/NameNami/beats-vilt-backend), which provides an administrative dashboard. Through the dashboard, admins can perform CRUD operations on beacons, assign them to specific rooms, and monitor their real-time connection status (driven by the heartbeats sent from this firmware).

## System Flow
1. **Registration:** An administrator notes the physical MAC address of the ESP32 beacon and registers it via the backend dashboard, assigning it to a specific classroom.
2. **Identification:** Upon boot, the beacon connects to Wi-Fi and sends a heartbeat to the server containing its **automatically retrieved MAC address**.
3. **Dynamic Configuration:** The server responds with a unique **UUID**. This UUID is dynamic and is rotated every 5 minutes by a Laravel cron job to prevent attendance spoofing or "beacon cloning."
4. **Advertising:** The firmware parses the UUID and immediately begins (or updates) BLE advertising as **"BEATS_Beacon"** using the iBeacon protocol.

## Hardware Indicators (LEDs)
The firmware uses on-board LEDs to communicate its current status:
*   **RED LED (GPIO 4):** System Power/Startup. Stays ON to indicate the BLE stack is active.
*   **BLUE LED (GPIO 2):**
    *   **Single Blink:** Successful heartbeat sent to server (Status 200).
    *   **Double Blink:** Successful UUID update/rotation applied.
    *   **Steady ON:** Wi-Fi or Server Connection failed.

## Features
*   **Automatic Identity:** Automatically retrieves the factory MAC address from eFuse; no manual ID configuration needed in code.
*   **Dynamic UUID Rotation:** Updates the iBeacon UUID in real-time without restarting the device.
*   **Proximity-Based Security (0dBm):** Radio TX power is set to 0dBm (Neutral) to provide a stable signal within the classroom while minimizing leakage to adjacent rooms.
*   **Precision Calibration:** Calibrated to -56dBm (at 1m) based on empirical testing to ensure accurate "Immediate/Near" detection on student devices.
*   **Multi-WiFi Fallback:** Supports a list of prioritized Wi-Fi networks for maximum uptime.
*   **Encrypted Heartbeats:** Uses HTTPS/SSL with certificate validation for secure server communication.
*   **Event-Driven Networking:** Uses a robust HTTP event handler to correctly process chunked server responses.
*   **NimBLE Stack:** Built using the efficient NimBLE host stack for optimized power and memory usage.

## Technical Requirements
*   **ESP-IDF v6.0+**
*   **Managed Components:** `espressif/cjson` (automatically handled by Component Manager via `idf_component.yml`).

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
*   `main/main.c`: Core logic, Wi-Fi management, HTTP event handling, and heartbeat task.
*   `main/src/gap.c`: BLE/NimBLE configuration, dynamic iBeacon advertising, and TX power management.
*   `main/include/common.h`: Global definitions (Device Name, Tags).
*   `main/idf_component.yml`: Managed dependency configuration for cJSON.
