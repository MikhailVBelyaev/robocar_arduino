# RoboCar Arduino — Project Guide for Claude

## What This Project Is

An Arduino Nano R4 WiFi-based robotic car that scans rooms using an ultrasonic sensor on a servo, navigates autonomously using wall-following, and estimates room area via the shoelace formula. The project is evolving toward a multi-purpose AI-powered home robot combining room mapping, baby companion functions, home security, and language learning features.

## Repository Layout

```
robocar_arduino/
├── Arduino/
│   └── WiFiCar/WiFiCar.ino     # Arduino firmware (motor control + HTTP server + servo scan)
├── docs/                        # Architecture, hardware, and feature docs
├── room_scanner.py              # Original simple scanner (historical reference)
├── room_scanner_06_07_25.py     # Current scanner — wall-following + shoelace area
├── ALGORITHM.md                 # Right-hand wall-following algorithm spec
├── DEVELOPMENT_LOG.md           # Dated log of milestones and known issues
└── README.md                    # Quick-start overview
```

## Current Hardware

| Component | Model | Notes |
|-----------|-------|-------|
| Microcontroller | Arduino Nano R4 WiFi | HTTP server on port 80 |
| Motor driver | L298N | Pins ENA=5, ENB=11, IN1-IN4=6-9 |
| DC motors | x2 | Forward/back 500 ms pulse, left/right 250 ms pulse |
| Ultrasonic sensor | HC-SR04 | Echo=A4, Trig=A5, mounted on servo |
| Servo | Standard 180° | Pin 3, sweeps 10°–180° in 30° steps |
| Power | Battery pack | Self-contained |

## Current Arduino API (HTTP)

All requests return JSON `{"distance_measurements": [{"angle": N, "distance": N}, ...]}`.

| Endpoint | Action |
|----------|--------|
| `GET /` | Scan only (sweep servo, return distances) |
| `GET /forward` | Move forward 500 ms, then return scan |
| `GET /back` | Move back 500 ms, then return scan |
| `GET /left` | Turn left 250 ms, then return scan |
| `GET /right` | Turn right 250 ms, then return scan |

Servo angles → directions: 10°/40° = right, 70°/100° = forward, 130°/160° = left.

## Current Python Scanner (`room_scanner_06_07_25.py`)

Two-phase algorithm:
1. **Wall-seek** — drive forward until something is ≤ 50 cm ahead.
2. **Wall-follow** — right-hand rule loop: turn left if forward < 60 cm, turn right if right > 50 cm, else forward. Stops when back near origin or after 100 steps.

Position tracked via dead reckoning (0.5 m per move, 90° turns). Area computed with shoelace formula on the path polygon.

**Key constants** (all in `room_scanner_06_07_25.py` top section):
- `MIN_DISTANCE_CM = 60` — minimum safe forward distance
- `TARGET_WALL_DISTANCE_CM = 25` — ideal right-wall gap
- `MAX_WALL_DISTANCE_CM = 50` — outer right-wall threshold
- `STEP_DISTANCE_M = 0.5` — assumed meters per move
- `MAX_STEPS = 100`

## Known Limitations

- Dead reckoning drift — no IMU or wheel odometry to correct position.
- Motor timing is fixed (500/250 ms) — actual distance varies with battery level.
- One ultrasonic sensor — no simultaneous multi-angle reading; servo sweep takes ~4 s.
- No map storage — each run starts fresh.

## Vision: Where This Is Going

This project is expanding toward a **multi-purpose home AI robot** running on a Raspberry Pi 5 or Jetson Orin Nano co-processor. Planned capability pillars:

1. **Accurate Room Mapping** — SLAM with RPLIDAR, occupancy grid, JSON map export.
2. **Baby Companion** — camera tracking, Claude API voice chat, English/Chinese TTS.
3. **Home Security** — face recognition, intruder alerts via Telegram, autonomous patrol.
4. **Smart Home Control** — IR blaster for TV, voice commands, Home Assistant bridge.

See `docs/` for full architecture and per-feature plans.

## Development Conventions

- Arduino firmware lives in `Arduino/WiFiCar/WiFiCar.ino`. Flash via Arduino IDE or `arduino-cli`.
- Python scripts connect to `http://192.168.100.236` (static IP assigned to the Arduino on the home router).
- Run logs are written to `logs/scan_<timestamp>.log` — keep them for SLAM calibration data.
- All new Python modules go in the repo root until a `src/` layout is warranted.
- Use `threading.Lock` for any background scanner thread — already established pattern.

## Quick Start

```bash
# 1. Flash Arduino firmware
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi Arduino/WiFiCar
arduino-cli upload  --fqbn arduino:renesas_uno:unor4wifi -p /dev/ttyACM0 Arduino/WiFiCar

# 2. Run the scanner
python3 room_scanner_06_07_25.py
# > start   — begin scan
# > s       — stop
# > exit    — quit
```

## File-Level Context for Claude

- **WiFiCar.ino** — all hardware logic lives here; motor timing constants are hardcoded delays, not encoder-based.
- **room_scanner_06_07_25.py** — the `Scanner` class is self-contained; `run_scan()` is the main loop, `move()` updates position state, `calculate_area()` runs shoelace.
- **ALGORITHM.md** — detailed algorithm spec including the math; update this whenever the algorithm changes.
- **DEVELOPMENT_LOG.md** — append a dated entry for every significant session; it is the project memory.
