# Development Roadmap

## Project Phases

The project evolves from the current working prototype (Arduino + Python wall-follower) through increasingly capable versions, each adding a new layer of functionality.

---

## Phase 0 — Current State (DONE)
**Goal**: Working room scanner with area estimation

- [x] Arduino Nano R4 WiFi HTTP server
- [x] Motor control (forward/back/left/right)
- [x] Ultrasonic sensor on servo (180° sweep)
- [x] Python wall-following algorithm
- [x] Shoelace formula area calculation
- [x] Timestamped logging system
- [x] Thread-safe stop control

**Known issues**:
- Area estimate drifts (dead reckoning only)
- Slow scan (servo sweep ~4 s)
- No map persistence

---

## Phase 1 — Better Room Measurement (Arduino only, no new hardware)
**Goal**: Improve accuracy without buying anything new

- [ ] 3-sample median for ultrasonic sensor (reduces noise)
- [ ] Finer servo sweep (15° steps instead of 30°)
- [ ] Calibrate `STEP_DISTANCE_M` against real floor measurement
- [ ] Serial communication mode (USB Arduino ↔ Pi) as alternative to WiFi
- [ ] JSON map export (save explored path + measurements to file)
- [ ] Web dashboard (simple Flask app showing robot path visualization)

**Estimated effort**: 2–4 days
**Expected result**: Area accuracy ±20%, more reliable navigation

---

## Phase 2 — Co-processor Addition (Raspberry Pi 5 acquired)
**Goal**: Run AI features, establish the two-tier architecture

### 2a — Infrastructure
- [ ] Set up Pi 5 with Ubuntu 24.04 or Raspberry Pi OS
- [ ] Serial link Arduino ↔ Pi (USB)
- [ ] Python serial protocol layer on Pi (replaces HTTP requests for autonomous mode)
- [ ] ROS2 Jazzy installation on Pi
- [ ] Basic ROS2 node for Arduino motor control

### 2b — Voice Assistant (English)
- [ ] USB microphone + speaker working
- [ ] Wake word detection (Porcupine)
- [ ] Whisper STT (faster-whisper, `base` model)
- [ ] Claude API integration (claude-opus-4-8 for conversation)
- [ ] gTTS text-to-speech
- [ ] Simple conversation loop

### 2c — Chinese Language Support
- [ ] Whisper auto-detects language (English/Chinese)
- [ ] gTTS Chinese TTS (zh-CN)
- [ ] Claude responds in detected language
- [ ] Test Chinese conversation quality

**Estimated effort**: 1–2 weeks
**Expected result**: Talking robot in English and Chinese

---

## Phase 3 — SLAM Room Mapping (RPLIDAR A1 acquired)
**Goal**: Accurate 2D room map, area from map geometry

- [ ] RPLIDAR A1 mounted and connected to Pi
- [ ] `rplidar_ros` ROS2 package working
- [ ] `slam_toolbox` producing occupancy grid
- [ ] MPU-6050 IMU added (orientation correction)
- [ ] `robot_localization` EKF fusing IMU + odometry
- [ ] Map saving/loading between runs
- [ ] Area calculation from occupancy grid
- [ ] Map visualization web interface
- [ ] Multi-room detection (doorway detection algorithm)

**Estimated effort**: 2–3 weeks
**Expected result**: ±3% area accuracy, persistent floor plan

---

## Phase 4 — Camera and Vision (Pi Camera Module 3)
**Goal**: See the world, enable baby tracking and security

### 4a — Baby Companion
- [ ] Camera stream working (picamera2 + OpenCV)
- [ ] YOLOv8 Nano person detection
- [ ] Baby vs. adult classification (bounding box size heuristic)
- [ ] Visual follow (pan/tilt or full robot turn to track)
- [ ] Cry detection (audio classifier)
- [ ] Telegram alert on cry
- [ ] Lullaby library (English + Chinese MP3s)
- [ ] Fall detection (MediaPipe pose)

### 4b — Home Security
- [ ] Face enrollment (family members)
- [ ] Face recognition during patrol (face_recognition library)
- [ ] Telegram bot: /status /patrol /stop /photo commands
- [ ] Unknown person alert with snapshot
- [ ] PIR sensor → wake from dock → begin patrol
- [ ] Patrol route management (waypoints in map)

**Estimated effort**: 3–4 weeks
**Expected result**: Full baby companion + basic home security

---

## Phase 5 — Smart Home Integration
**Goal**: Robot as a hub for home automation

- [ ] IR blaster (TV remote control)
- [ ] Voice-controlled TV (Claude maps voice → IR command)
- [ ] Home Assistant MQTT integration
- [ ] Battery monitoring (INA219) + auto-return to dock
- [ ] Dock station with auto-charging (IR homing beacon)
- [ ] Mobile web dashboard (React or HTMX)
- [ ] OTA firmware updates for Arduino

**Estimated effort**: 2–4 weeks
**Expected result**: Full home robot assistant

---

## Phase 6 — Advanced AI (Jetson Orin Nano, optional upgrade)
**Goal**: Run large AI models locally, no internet dependency

- [ ] Migrate from Pi 5 to Jetson Orin Nano (or add alongside)
- [ ] Local Whisper medium model (5x better than base, GPU-accelerated)
- [ ] Local LLM (LLaMA 3 8B via Ollama) for offline baby companion
- [ ] Real-time YOLO v8 at 30 FPS (GPU accelerated)
- [ ] Local Chinese TTS (VITS or Coqui)
- [ ] Behavior learning (log interesting interactions, fine-tune conversation)

**Estimated effort**: 3–5 weeks
**Note**: Only worth doing once Phase 4 is complete and API costs become significant

---

## Milestone Summary Table

| Milestone | Key Deliverable | Hardware Needed | Timeline |
|-----------|----------------|-----------------|---------|
| Phase 0 | Working scanner | Current | Done |
| Phase 1 | Better accuracy | Current | 2–4 days |
| Phase 2 | Talking robot (EN + ZH) | + Raspberry Pi 5 | 1–2 weeks |
| Phase 3 | Accurate floor plan | + RPLIDAR A1 | 2–3 weeks |
| Phase 4 | Baby companion + security | + Camera Module 3 | 3–4 weeks |
| Phase 5 | Smart home hub | + IR blaster + INA219 | 2–4 weeks |
| Phase 6 | Local AI (offline) | + Jetson Orin Nano | 3–5 weeks |

---

## Recommended First Purchase

**If budget ≤ $150**: Raspberry Pi 5 (4 GB, $60) + USB webcam ($25) + USB mic ($15) + speaker ($15) = ~$115  
Start immediately with Phase 2 (voice assistant) while saving for RPLIDAR.

**If budget ≤ $300**: Add RPLIDAR A1 ($100) = full Phase 3 capability.

**If budget ≤ $500**: Add Pi Camera Module 3 Wide ($35) + RPLIDAR A1 = full Phase 4 baby companion.

---

## Repository Structure for Future Phases

```
robocar_arduino/
├── Arduino/
│   └── WiFiCar/WiFiCar.ino          # Phase 1+: add encoder support, IMU I2C
├── pi/                               # Phase 2+: Pi code
│   ├── ros2_ws/                      # ROS2 workspace
│   │   └── src/
│   │       ├── robocar_bringup/      # launch files
│   │       ├── robocar_navigation/   # Nav2 config
│   │       └── robocar_arduino_bridge/ # Serial ↔ ROS2 node
│   ├── ai/
│   │   ├── brain.py                  # Claude API wrapper
│   │   ├── vision.py                 # YOLO + face recognition
│   │   ├── speech.py                 # Whisper + TTS pipeline
│   │   └── security.py              # Telegram bot + alerts
│   ├── baby_companion.py             # Baby companion main loop
│   ├── security_patrol.py           # Security patrol main loop
│   └── tv_remote.py                 # IR blaster control
├── room_scanner_06_07_25.py         # Phase 1: standalone Python scanner
├── docs/                            # This directory
├── CLAUDE.md
└── ALGORITHM.md
```
