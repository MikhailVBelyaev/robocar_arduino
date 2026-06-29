# Development Guide

## Environment Setup

### Arduino Development

**Required tools**:
```bash
# Arduino CLI (recommended over IDE for command-line workflow)
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# Install board support for Arduino Nano R4
arduino-cli core update-index
arduino-cli core install arduino:renesas_uno

# Install Servo library
arduino-cli lib install Servo

# Install WiFiS3 (bundled with renesas_uno core)
```

**Compile and flash**:
```bash
# Compile
arduino-cli compile --fqbn arduino:renesas_uno:unor4wifi Arduino/WiFiCar

# Find port
arduino-cli board list

# Upload
arduino-cli upload --fqbn arduino:renesas_uno:unor4wifi -p /dev/ttyACM0 Arduino/WiFiCar

# Monitor serial output
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=9600
```

**Configure WiFi credentials** (before flashing):
Edit `Arduino/WiFiCar/WiFiCar.ino` lines 7–8:
```cpp
const char* ssid = "YourWiFiName";
const char* password = "YourPassword";
```

---

### Python Development

**Requirements**:
```bash
# Create virtual environment
python3 -m venv .venv
source .venv/bin/activate

# Install dependencies
pip install requests

# Future dependencies (Phase 2+)
# pip install anthropic faster-whisper gTTS pyaudio porcupine face-recognition ultralytics
```

**Run the room scanner**:
```bash
python3 room_scanner_06_07_25.py
# Commands:
#   start  — begin scan
#   s      — stop
#   exit   — quit
```

**Logs**: Written to `logs/scan_<timestamp>.log`. Keep these for calibration analysis.

---

### Jetson Orin Nano Setup (Phase 2+)

**OS**: Ubuntu 22.04 (bundled with JetPack 6.2.1 — already flashed).
See `mb_orin_nano` repo for flash commands and full setup scripts.

**Initial setup on Jetson**:
```bash
# Update
sudo apt update && sudo apt upgrade -y

# Python deps
sudo apt install python3-pip python3-venv -y

# Serial port access (for Arduino USB)
sudo usermod -aG dialout $USER

# Audio (PulseAudio for USB mic + speaker)
sudo apt install alsa-utils pulseaudio pavucontrol -y

# CUDA check (should show Ampere GPU)
nvidia-smi
nvcc --version
```

**ROS2 Jazzy installation** (Ubuntu 22.04 compatible):
```bash
sudo apt install software-properties-common -y
sudo add-apt-repository universe
sudo apt update && sudo apt install curl -y
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
  http://packages.ros.org/ros2/ubuntu jammy main" \
  | sudo tee /etc/apt/sources.list.d/ros2.list
sudo apt update
sudo apt install ros-jazzy-desktop -y
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
```

**RPLIDAR setup**:
```bash
sudo apt install ros-jazzy-rplidar-ros -y

# Test
ros2 launch rplidar_ros rplidar_a1_launch.py

# Verify scan topic
ros2 topic echo /scan
```

**SLAM toolbox**:
```bash
sudo apt install ros-jazzy-slam-toolbox -y
ros2 launch slam_toolbox online_async_launch.py
```

---

## Serial Protocol (Arduino ↔ Pi)

For Phase 2+ when Pi drives the robot autonomously via USB serial instead of WiFi HTTP.

**Arduino additions to WiFiCar.ino**:
```cpp
void handleSerial() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        String scanResult = getScanJSON();
        
        if (cmd == "F") { _mForward(); }
        else if (cmd == "B") { _mBack(); }
        else if (cmd == "L") { _mLeft(); }
        else if (cmd == "R") { _mRight(); }
        else if (cmd == "S") { _mStop(); }
        
        Serial.println(scanResult);
    }
}
```

**Python serial client**:
```python
import serial
import json

class ArduinoSerial:
    def __init__(self, port="/dev/ttyACM0", baud=9600):
        self.ser = serial.Serial(port, baud, timeout=5)
    
    def send(self, command: str) -> dict:
        self.ser.write(f"{command}\n".encode())
        response = self.ser.readline().decode().strip()
        return json.loads(response)
    
    def forward(self): return self.send("F")
    def back(self):    return self.send("B")
    def left(self):    return self.send("L")
    def right(self):   return self.send("R")
    def stop(self):    return self.send("S")
    def scan(self):    return self.send("SCAN")
```

---

## Debugging

### Arduino

**Serial monitor** shows all movement commands and IP address:
```
Connecting to WiFi: Mishawifi
.....
Connected!
IP Address: 192.168.100.236
IP Address: 192.168.100.236
Forward
Stop
```

**Common issues**:
- Robot doesn't move: check ENA/ENB are HIGH in motor functions
- WiFi doesn't connect: verify SSID/password, check 2.4 GHz (Nano R4 doesn't support 5 GHz)
- Ultrasonic returns 0: check Echo/Trig wiring, ensure 5V supply is adequate

### Python Scanner

**Enable debug logging**: the scanner already logs at DEBUG level — check `logs/scan_<timestamp>.log`.

**Test connectivity**:
```python
import requests
r = requests.get("http://192.168.100.236/", timeout=10)
print(r.json())
# Should show: {"distance_measurements": [...]}
```

**Simulate scan without hardware**:
```python
# Mock scan data for testing navigation logic
MOCK_SCAN = [
    {"angle": 10, "distance": 30},
    {"angle": 40, "distance": 35},
    {"angle": 70, "distance": 150},
    {"angle": 100, "distance": 200},
    {"angle": 130, "distance": 80},
    {"angle": 160, "distance": 60},
]
```

---

## Testing Checklist

### Before each scan session
- [ ] Battery is charged (motors move at full speed)
- [ ] Floor is clear of obstacles below 10 cm (ultrasonic can't see very low objects)
- [ ] Room is at least 2 × 2 m (smaller rooms confuse wall-following)
- [ ] Robot is placed at least 50 cm from any wall at start
- [ ] WiFi connection confirmed: `ping 192.168.100.236`

### After scan
- [ ] Check log for collision events (`forward_distance < 60 cm` before forward move)
- [ ] Check path makes geometric sense (no teleportation)
- [ ] Area result is within ±50% of expected room size
- [ ] Log saved in `logs/` directory

---

## Code Style

- Python: PEP 8, type hints for function signatures
- Arduino: Arduino style (camelCase functions, UPPER_CASE constants)
- All new constants go at the top of the file with a comment explaining units
- Log every decision in the scanner with the measured values that led to it
- Never `time.sleep()` inside a loop that holds `self.lock`

---

## Git Workflow

```bash
# Feature branch
git checkout -b feature/imu-integration

# Commit format (match existing log style)
git commit -m "Add MPU-6050 IMU orientation correction

Replaces fixed 90° turn assumption with gyro-measured actual angle.
Reduces orientation error from ±15° to ±2° in testing."

# Keep DEVELOPMENT_LOG.md updated with dated entries
```
