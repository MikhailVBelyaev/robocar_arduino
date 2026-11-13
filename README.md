# Arduino Robocar with HTTP Control Interface

## 🚗 Project Overview

This project involves an **Arduino Nano R4**-based robotic car that can be controlled via a **local HTTP web interface**. The car is powered by a **battery pack** and uses an **ultrasonic sensor** for scanning and obstacle detection. The primary goal of the project is to scan rooms by navigating the car, identify clear paths, and move accordingly.

## 🎛️ Interface Description

The Robocar is controlled via HTTP requests sent to the Arduino's IP address. The available commands are:

- **HTTP Requests**: Control the Robocar using HTTP requests to the following endpoints:
  - **`/forward`**: Move forward
  - **`/back`**: Move backward
  - **`/left`**: Turn left
  - **`/right`**: Turn right
  - **``**: Start room scanning (first-time scan only, to map the room)
  
  - **`/look?angle=N`**, **`/nod`**, **`/status`**: head (servo) commands for the Nanny robot, answer immediately; see [docs/head.md](docs/head.md)

  Example Command:
  - `http://192.168.100.236/forward` – Move the car forward.

## 🧰 Technologies Used

- **Arduino Nano R4 with Wi-Fi**
- **Ultrasonic sensor (for distance measurement)**
- **Battery pack (for power supply)**
- **HTTP web interface** for local network control
- **Python (for room scanning logic)**

## 📡 Communication

- The Robocar uses **Wi-Fi** to communicate with the control interface over the local network. There is no need for a serial connection; all commands are sent via HTTP requests to the Arduino board's IP address.

## 🔧 Hardware Setup

- **Motor Driver**: L298N (for motor control)
- **DC Motors**: Two motors for movement control
- **Power Source**: Battery pack (no external power required)
- **Ultrasonic Sensor**: Used for room scanning and obstacle detection
- **Arduino Nano R4 with Wi-Fi**: Controller for the Robocar

## 🧪 Current Status

The Robocar is currently functional and can move in all directions (forward, backward, left, and right) via HTTP commands. Room scanning logic is in place, but the robot's area estimation needs improvement.

## 📅 Planned Features

- **Room Scanning Logic**: Using the ultrasonic sensor, the car scans the room and tries to navigate clear paths.
- **Area Estimation Improvement**: Refine the logic for a more accurate area estimate.
- **Room Mapping**: Store and analyze the map of the room.
- **Machine Learning Integration**: Potential future enhancements like automated movement learning and room analysis.
