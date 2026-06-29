# System Architecture

## Overview

The long-term architecture is a **two-tier embedded system**: a low-level Arduino layer handling real-time motor/sensor control, and a high-level SBC (single-board computer) layer running AI, SLAM, and communication. They talk over USB-Serial (preferred for reliability) or WiFi HTTP (current fallback).

```
┌─────────────────────────────────────────────────────────────────┐
│                    Physical Robot                               │
│                                                                 │
│  ┌───────────────────┐         ┌─────────────────────────────┐  │
│  │  Arduino Nano R4  │ USB/   │   Jetson Orin Nano          │  │
│  │  WiFi             │◄──────►│   JetPack 6.2.1 / Ubuntu 22 │  │
│  │                   │ Serial │                             │  │
│  │ • Motor control   │        │ • SLAM / mapping            │  │
│  │ • Servo sweep     │        │ • Computer vision           │  │
│  │ • Ultrasonic dist │        │ • Speech recognition        │  │
│  │ • HTTP server     │        │ • Text-to-speech            │  │
│  │ • WiFi AP/client  │        │ • Claude API client         │  │
│  └───────────────────┘        │ • Home Assistant bridge     │  │
│                               │ • Telegram bot              │  │
│  ┌─────────────────────────┐  │ • Web dashboard             │  │
│  │  Sensors & Actuators    │  └─────────────────────────────┘  │
│  │                         │                                   │
│  │  HC-SR04 (ultrasonic)   │  ┌─────────────────────────────┐  │
│  │  RPLIDAR A1 (future)    │  │  Peripherals on SBC          │  │
│  │  MPU-6050 IMU (future)  │  │                             │  │
│  │  Camera (Pi/USB)        │  │  • Camera module            │  │
│  │  IR blaster             │  │  • USB microphone           │  │
│  │  Speaker + Mic          │  │  • Speaker (USB/BT/3.5mm)   │  │
│  │  L298N motor driver     │  │  • IR LED blaster           │  │
│  └─────────────────────────┘  └─────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                    │
                    │ WiFi / LAN
                    │
         ┌──────────┴──────────┐
         │    Home Network      │
         │                     │
         │  • Home Assistant   │
         │  • Telegram Cloud   │
         │  • Claude API       │
         │  • Mobile app       │
         └─────────────────────┘
```

## Layer Responsibilities

### Arduino Layer (real-time, < 10 ms latency)
- PWM motor speed control via L298N
- Servo position commands
- Ultrasonic pulse timing (microsecond precision)
- WiFi HTTP server (current control interface)
- Future: encoder pulse counting, IMU reading via I2C, battery ADC

### SBC Layer (high-level, 100 ms – 1 s latency acceptable)
- SLAM: fuse RPLIDAR + IMU + odometry into a 2D occupancy grid
- Computer vision: YOLO/MediaPipe for person/baby/face detection
- Audio pipeline: Whisper STT → Claude API → TTS playback
- Autonomy: path planning (A*), mission management, return-to-dock
- Connectivity: Telegram alerts, Home Assistant MQTT, web dashboard

## Communication Between Layers

### Option A — USB Serial (recommended for production)
```
SBC  ──USB──►  Arduino
      Commands: "F\n", "B\n", "L\n", "R\n", "S\n"
      Responses: JSON distance_measurements line
```
Advantages: no WiFi dependency, lower latency, more reliable, powers Arduino from SBC USB port.

### Option B — WiFi HTTP (current implementation)
```
SBC  ──HTTP GET──►  Arduino HTTP server
```
Advantages: already implemented, no wiring change.  
Disadvantages: ~100–500 ms overhead per request, drops when WiFi is busy.

**Migration path**: keep HTTP for remote manual control; add Serial for autonomous SBC-driven navigation.

## SBC: Jetson Orin Nano (already owned)

**Device**: Jetson Orin Nano 8 GB, flashed with JetPack 6.2.1 (Ubuntu 22.04).
**Repository**: `mb_orin_nano` — companion project that holds all Orin-side code.

| Spec | Value |
|------|-------|
| CPU | ARM Cortex-A78AE ×6 @ 1.5 GHz |
| GPU | 1024 CUDA cores (Ampere architecture) |
| RAM | 8 GB LPDDR5 |
| OS | Ubuntu 22.04 (JetPack 6.2.1) |
| ROS2 | Jazzy (native on Ubuntu 22.04) |
| AI inference | Local YOLO v8 ~30 FPS, Whisper medium locally |

**Why the Orin Nano is better than Pi 5 for this project**:
- GPU-accelerated YOLO → real-time baby/person tracking at 30 FPS vs 10 FPS on Pi
- CUDA-accelerated Whisper → Chinese speech recognition is much more accurate with `medium` model locally
- DLA (Deep Learning Accelerator) for low-power inference during patrol/sleep modes
- 8 GB unified memory → can run SLAM + YOLO + Whisper simultaneously without swapping
- Ubuntu 22.04 → ROS2 Jazzy installs natively, no Docker workarounds needed

## Data Flow: Room Mapping

```
RPLIDAR A1  →  /scan topic (ROS2)  →  slam_toolbox  →  /map (OccupancyGrid)
                                                          │
MPU-6050 IMU  →  /imu topic        →  EKF filter   →  /odom (corrected)
                                                          │
                                               map_saver_cli → room_map.pgm + .yaml
                                                          │
                                               area_calculator.py → m² estimate
```

## Data Flow: Baby Companion

```
Camera  →  YOLO v8 (person detect)  →  servo/motor commands to follow
        →  MediaPipe face mesh      →  face ID (known vs unknown)

Microphone  →  Whisper STT  →  text
                               │
                           Claude API (claude-sonnet-4-6 or opus-4-8)
                               │
                           TTS (pyttsx3 / gTTS / ElevenLabs)
                               │
                           Speaker output (English or Mandarin)
```

## Data Flow: Security

```
Camera  →  YOLO v8 (person detect)  →  known face? ──No──►  Telegram alert + photo
                                                   ──Yes──►  greet / log

Motion sensor (PIR)  →  wake robot from sleep  →  begin patrol
```

## Network Architecture

```
Home Router
    │
    ├── Arduino (192.168.x.x, static DHCP)
    ├── Raspberry Pi / Jetson (static IP)
    └── Mobile phone (Telegram app)
         │
         └── Telegram Bot API ──► Telegram Cloud ──► mobile notifications
```

All AI API calls (Claude, Whisper if cloud) go outbound to the internet. The robot itself does not need to be accessible from the internet.
