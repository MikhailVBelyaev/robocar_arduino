# Hardware Guide

## Current Hardware (Phase 1 — Working)

### Arduino Nano R4 WiFi
- **Role**: Low-level controller, HTTP server
- **Pins used**:
  - `D3` — Servo signal
  - `D5` (ENA), `D6` (IN1), `D7` (IN2) — Left motor via L298N
  - `D8` (IN3), `D9` (IN4), `D11` (ENB) — Right motor via L298N
  - `A4` (Echo), `A5` (Trig) — HC-SR04 ultrasonic
- **WiFi**: Connects to home SSID, gets IP via DHCP (assign static lease on router)

### L298N Motor Driver
- Dual H-bridge, handles 2 DC motors
- ENA/ENB control speed (HIGH = full speed, can PWM for variable speed)
- IN1/IN2 and IN3/IN4 control direction

### HC-SR04 Ultrasonic Sensor
- Range: 2–400 cm, practical indoor: 2–200 cm
- Mounted on servo for 180° sweep
- **Limitation**: Single-point measurement; sweep takes ~4 s for 6 angles
- **Blind spot**: objects < 2 cm or > 400 cm are undetected

### Standard 180° Servo
- Pin 3, sweeps 10° → 180° in 30° increments
- Holds ultrasonic sensor for room scanning

### Battery Pack
- Powers the whole robot (motors + Arduino)
- Motor behavior changes as battery drains — calibrate `STEP_DISTANCE_M` when freshly charged

---

## Phase 2 Hardware — Room Mapping Upgrade

### RPLIDAR A1 (recommended) or A2
- **Why**: 360° simultaneous laser scan at ~5–10 Hz, 0.15–6 m range
- **Connection**: USB → Raspberry Pi (USB-to-serial via `/dev/ttyUSB0`)
- **ROS2 package**: `rplidar_ros`
- **Cost**: A1 ~$100, A2 ~$150
- **Alternative**: YDLIDAR X4 (~$80) — lower range but budget-friendly
- **Replaces**: Ultrasonic servo sweep for mapping purposes (keep ultrasonic for close obstacle detection)

### MPU-6050 IMU (gyro + accelerometer)
- **Why**: Correct orientation drift from dead reckoning
- **Connection**: I2C to Arduino (SDA=A4, SCL=A5) — **note**: conflicts with current HC-SR04 pins; reassign ultrasonic to D2/D3
- **Alternative on Pi**: Connect MPU-6050 directly to Pi's I2C bus
- **ROS2 package**: `imu_tools`, `robot_localization` (EKF fusion)
- **Cost**: ~$3

### Wheel Encoders
- **Why**: Measure actual distance traveled (replaces timing-based dead reckoning)
- **Type**: Hall-effect or IR encoder discs on motor shafts
- **Connection**: Interrupt pins on Arduino (D2, D3 preferred)
- **Cost**: ~$5–10 per pair
- **Resolution target**: ≥ 20 pulses/revolution

### Revised Pin Assignment for Phase 2

| Pin | Function |
|-----|----------|
| D2 | Left encoder interrupt |
| D3 | Right encoder interrupt |
| D4 | HC-SR04 Trig |
| D5 (ENA) | Left motor speed (PWM) |
| D6 (IN1) | Left motor dir A |
| D7 (IN2) | Left motor dir B |
| D8 (IN3) | Right motor dir A |
| D9 (IN4) | Right motor dir B |
| D10 | Servo signal |
| D11 (ENB) | Right motor speed (PWM) |
| D12 | HC-SR04 Echo |
| A4 (SDA) | I2C data (MPU-6050) |
| A5 (SCL) | I2C clock (MPU-6050) |

---

## Phase 3 Hardware — AI Features

### Camera
- **For Jetson Orin Nano**: IMX219 CSI camera (the official Jetson camera, plug-and-play with JetPack)
  - Connects via CSI ribbon cable to Jetson Orin Nano's 15-pin MIPI CSI port
  - Supported natively by JetPack — no driver work needed
  - `nvarguscamerasrc` in GStreamer pipeline, or `cv2.VideoCapture("nvarguscamerasrc ... ! appsink")` in OpenCV
- **Alternative**: Any USB UVC webcam (Logitech C920/C270) — plug-and-play, lower latency wiring
- **For baby tracking**: Wide-angle lens (≥ 90° FOV)
- **For night**: IMX219-160IR (infrared capable) + IR LED ring

### Microphone
- **Type**: USB omnidirectional microphone
- **Why USB**: Avoids noise from motor PWM coupling into analog mic; Jetson Orin Nano has USB 3.0 × 4
- **Recommended**: ReSpeaker 4-mic USB array — supports far-field voice pickup, wake word detection
- **Wake word**: Use `porcupine` (Picovoice) for "Hey Robot" offline detection on Jetson GPU

### Speaker
- **Option A**: USB speaker or USB DAC + 3.5 mm speaker — simplest, plug-and-play
- **Option B**: I2S amplifier (MAX98357A) + small speaker — higher quality, lower cost
- **Volume**: 3–5 W is sufficient for a room with a baby

### IR Blaster (TV Remote)
- **Hardware**: IR LED + 38 kHz carrier driver circuit (transistor + resistor)
- **Connection**: GPIO pin on Pi → IR LED → TV
- **Library**: `LIRC` (Linux Infrared Remote Control) or `irdb` for IR codes
- **Alternative**: Broadlink RM4 mini (standalone WiFi IR blaster, no wiring) + Python `broadlink` library

### PIR Motion Sensor (Security)
- **Type**: HC-SR501 passive infrared
- **Connection**: GPIO on Pi or Arduino
- **Use**: Wake robot from sleep/dock when motion detected in room
- **Range**: 3–7 m, 120° cone

### Temperature / Air Quality (Baby Safety)
- **DHT22** — temperature + humidity (I2C or 1-wire, ~$3)
- **MQ-135** — air quality / CO2 level (~$5)
- **Connection**: Direct to Arduino (analog/digital), report to Pi via serial

### Battery Monitoring
- **INA219** — current + voltage sensor on I2C
- **Purpose**: Know remaining battery %, trigger auto-return to dock before power dies
- **Connection**: I2C on Arduino

---

## Wiring Diagram Sketches

### Current Setup (Phase 1)
```
Battery Pack (+) ──► L298N (12V in) ──► L298N (5V out) ──► Arduino Vin
                          │
                     IN1-IN4, ENA, ENB ◄── Arduino D5-D9, D11

Arduino A4 ──► HC-SR04 Echo
Arduino A5 ──► HC-SR04 Trig
HC-SR04 VCC ──► Arduino 5V
HC-SR04 GND ──► Arduino GND

Arduino D3 ──► Servo Signal
Servo VCC ──► Arduino 5V (or separate 5V)
Servo GND ──► Arduino GND
```

### Phase 2 Addition (RPLIDAR + IMU + Encoders)
```
Raspberry Pi USB ──► RPLIDAR USB adapter (UART) ──► RPLIDAR motor

Arduino I2C (A4/A5) ──► MPU-6050

Motor shaft ──► Encoder disc ──► IR/Hall sensor ──► Arduino D2 / D3

Arduino USB ──► Raspberry Pi USB (Serial communication)
```

### Phase 3 Addition (Camera + Audio + IR)
```
Jetson Orin Nano CSI ──► IMX219 camera (or USB webcam)

Jetson Orin Nano USB 3.0 ──► ReSpeaker 4-mic USB
Jetson Orin Nano GPIO ──► IR LED driver (3.3V logic) ──► IR LED → TV

I2S or USB DAC ──► MAX98357A ──► Speaker
```

---

## Bill of Materials (full Phase 3 build)

| Component | Purpose | Est. Cost |
|-----------|---------|-----------|
| Arduino Nano R4 WiFi | Already have | — |
| L298N motor driver | Already have | — |
| HC-SR04 + servo | Already have | — |
| Battery pack | Already have | — |
| Jetson Orin Nano 8 GB | Already have | — |
| IMX219 CSI camera (wide) | Vision | $25 |
| RPLIDAR A1 | SLAM mapping | $100 |
| MPU-6050 | IMU | $3 |
| ReSpeaker 4-mic USB array | Voice input | $25 |
| USB speaker or MAX98357A + 5W | Audio output | $10–$15 |
| HC-SR501 PIR | Security wake | $3 |
| IR LED + NPN driver circuit | TV control | $2 |
| INA219 | Battery monitor | $4 |
| DHT22 | Environment | $3 |
| NVMe SSD 256 GB (M.2 2230) | Jetson storage | $30 |
| DC 5V 4A power supply (for Jetson) | Power | $15 |
| **Total additional cost** | | **~$225** |

> **Budget path**: Skip RPLIDAR ($100), use only ultrasonic for now. Add LiDAR later.
> **Minimum viable build** (voice + vision): IMX219 + mic + speaker = **~$65** additional.
