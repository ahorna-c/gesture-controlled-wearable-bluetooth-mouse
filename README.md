# Gesture-Controlled Wearable Bluetooth Mouse

This project was developed as part of the SYDE 361 (Systems Design Methods 1: Needs Analysis and Prototyping) course.

## Problem Statement
Standard digital interfaces (such as traditional mouse and trackpad setups) require fine motor control, often excluding individuals with mobility impairments. This project bridges that gap by providing a wireless, wearable, and affordable Bluetooth glove that translates natural hand gestures into precise computer commands.

## Key Features
- Accessible Computing: Enables full cursor control and scrolling for users with limited dexterity or stationary setups.
- Low-Latency Performance: Achieves a response time of 75–80ms using asynchronous Python processing.
- Cost-Efficient: Engineered with a Bill of Materials (BOM) under $20.
- Long-Range Connectivity: Stable Bluetooth transmission up to 15 meters.
- Wearable Ergonomics: Compact, lightweight 3D-printed enclosure mounted on breathable polyester.

## System Architecture & Hardware
- ESP32 Microcontroller: Handles sensor data processing and BLE advertising.
- MPU6050 Accelerometer: Tracks hand tilt (Pitch/Roll) for cursor and scroll movement.
- Tactile Buttons: Facilitates Left/Right clicks and toggles between Cursor/Scroll modes.

<img width="405" height="443" alt="image" src="https://github.com/user-attachments/assets/978679a1-1e40-4373-8b39-9e6fe517447e" />

## Software Logic
The project is split into two primary environments:

1. Firmware (`firmware.ino`)
This code runs on the ESP32 and performs:
- Data Aggregation: Collects IMU data and debounces button inputs.
- BLE Notification: Packs pitch, roll, and button states into a bitmask for wireless transmission every 10ms.

2. Controller Application (`cursor_controller.py`)
This Python-based service acts as the BLE client and performs:
- Data Parsing: Decodes incoming byte arrays from the ESP32.
- Action Mapping: Uses the `pyautogui` library to translate hand tilts into relative cursor motion or vertical scrolling.
- Dead-Zone Calibration: Implements a 3.0° dead zone to filter out mechanical noise and natural hand tremors.

## Final Prototype
<img width="635" height="443" alt="image" src="https://github.com/user-attachments/assets/412852b4-6e57-4bbe-aa0c-aafcaed6b186" />

## How to Use
1. Flash `firmware.ino` to your ESP32 via the Arduino IDE
2. Install the required Python libraries:
```
pip install bleak pyautogui asyncio
```
3. Run the controller script on your host computer:
```
python3 cursor_controller.py
```
The device will broadcast as NanoESP32-BLE and should pair in 1-3 minutes.

4. Use hand gestures and clicks to control your computer screen!

