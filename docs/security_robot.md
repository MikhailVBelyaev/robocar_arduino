# Home Security Robot

## Vision

An autonomous patrol robot that monitors the home, detects unknown people, alerts the owner via Telegram, and deters intruders — all while navigating using the SLAM map built during room scanning.

---

## Feature 1: Autonomous Patrol

Once the room map is built (via SLAM), the robot follows a preset patrol route through key waypoints (doorways, hallways, entry points).

```python
# Patrol waypoints in map coordinates (meters from origin)
PATROL_ROUTE = [
    (0.0, 0.0),   # dock/home
    (2.0, 0.0),   # hallway
    (2.0, 3.0),   # living room far corner
    (4.0, 3.0),   # near front door
    (4.0, 1.5),   # kitchen
    (0.0, 0.0),   # return to dock
]

def run_patrol(nav_client, route):
    for x, y in route:
        goal = NavigateToPose.Goal()
        goal.pose.pose.position.x = x
        goal.pose.pose.position.y = y
        nav_client.send_goal(goal)
        nav_client.wait_for_result()
        time.sleep(2)  # pause at each waypoint to scan
```

With ROS2 Nav2, this is straightforward once the map is built.

**Patrol schedule**: Trigger patrol on a cron job or when PIR motion sensor wakes the robot.

---

## Feature 2: Face Recognition

Distinguish **known people** (family members) from **unknown people** (intruders).

### Step 1: Enroll family members
```python
import face_recognition

def enroll_person(name: str, photo_path: str):
    image = face_recognition.load_image_file(photo_path)
    encodings = face_recognition.face_encodings(image)
    if not encodings:
        raise ValueError("No face found in photo")
    save_encoding(name, encodings[0])  # store in JSON file
```

### Step 2: Recognize during patrol
```python
def identify_person(frame) -> str:
    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    face_locations = face_recognition.face_locations(rgb_frame)
    face_encodings = face_recognition.face_encodings(rgb_frame, face_locations)
    
    for encoding in face_encodings:
        matches = face_recognition.compare_faces(known_encodings, encoding, tolerance=0.5)
        if any(matches):
            idx = matches.index(True)
            return known_names[idx]
    return "UNKNOWN"
```

### Step 3: React
```python
def handle_detection(person_name: str, frame):
    if person_name == "UNKNOWN":
        send_telegram_alert(frame, message="Unknown person detected!")
        robot_speak("Hello, who are you? I'm calling the owner.")
        save_snapshot(frame)
    else:
        robot_speak(f"Hello, {person_name}! Welcome home.")
```

**Note**: `face_recognition` library uses `dlib` — runs on Pi 5 at ~2–5 FPS. Acceptable for stationary detection; consider YOLO face detection for faster initial detection.

---

## Feature 3: Motion-Triggered Wake

Use HC-SR501 PIR sensor connected to Pi GPIO:

```python
import RPi.GPIO as GPIO

PIR_PIN = 17

GPIO.setmode(GPIO.BCM)
GPIO.setup(PIR_PIN, GPIO.IN)

def on_motion_detected(channel):
    print("Motion detected! Starting patrol...")
    start_patrol()

GPIO.add_event_detect(PIR_PIN, GPIO.RISING, callback=on_motion_detected, bouncetime=3000)
```

The robot sleeps (motors off, low power) until PIR detects motion, then begins a patrol scan.

---

## Feature 4: Telegram Bot Interface

Full two-way control via Telegram:

```python
from telegram import Update, Bot
from telegram.ext import Application, CommandHandler, ContextTypes

bot_token = "YOUR_BOT_TOKEN"

async def cmd_status(update: Update, context: ContextTypes.DEFAULT_TYPE):
    status = get_robot_status()
    await update.message.reply_text(
        f"Robot status:\n"
        f"Battery: {status['battery']}%\n"
        f"Position: {status['position']}\n"
        f"Last detection: {status['last_detection']}\n"
        f"Mode: {status['mode']}"
    )

async def cmd_patrol(update: Update, context: ContextTypes.DEFAULT_TYPE):
    start_patrol()
    await update.message.reply_text("Patrol started.")

async def cmd_photo(update: Update, context: ContextTypes.DEFAULT_TYPE):
    frame = capture_frame()
    await update.message.reply_photo(photo=frame, caption="Live view")

async def cmd_stop(update: Update, context: ContextTypes.DEFAULT_TYPE):
    stop_robot()
    await update.message.reply_text("Robot stopped.")

# Commands: /status /patrol /stop /photo /goto <x> <y> /enroll <name>
app = Application.builder().token(bot_token).build()
app.add_handler(CommandHandler("status", cmd_status))
app.add_handler(CommandHandler("patrol", cmd_patrol))
app.add_handler(CommandHandler("photo", cmd_photo))
app.add_handler(CommandHandler("stop", cmd_stop))
```

### Alert message with photo
```python
async def send_alert(bot: Bot, chat_id: str, frame, message: str):
    # Save frame as JPEG
    _, buffer = cv2.imencode('.jpg', frame)
    await bot.send_photo(
        chat_id=chat_id,
        photo=buffer.tobytes(),
        caption=f"ALERT: {message}\nTime: {datetime.now().strftime('%H:%M:%S')}"
    )
```

---

## Feature 5: Night Vision

For nighttime security, use **Pi Camera Module 3 NoIR** (no infrared filter) + **IR LED ring**:

- IR LEDs illuminate the room invisibly to humans
- NoIR camera captures the IR-lit scene clearly
- YOLO can still detect people in IR-lit grayscale frames

Alternative: USB webcam with built-in IR LEDs (many security webcams have this).

---

## Feature 6: Sound Detection

Detect glass breaking, door slamming, or unusual sounds:

```python
import librosa
import sounddevice as sd
import numpy as np

GLASS_BREAK_FREQ_RANGE = (1000, 8000)  # Hz
ALARM_THRESHOLD = 0.8

def monitor_audio():
    while True:
        audio = sd.rec(int(0.5 * 16000), samplerate=16000, channels=1, dtype='float32')
        sd.wait()
        audio = audio.flatten()
        
        # Check for high-frequency burst (glass breaking characteristic)
        stft = np.abs(librosa.stft(audio))
        freqs = librosa.fft_frequencies(sr=16000)
        high_freq_energy = stft[(freqs > 1000) & (freqs < 8000), :].mean()
        
        if high_freq_energy > ALARM_THRESHOLD:
            send_telegram_alert(None, "Unusual sound detected! (possible glass break)")
```

---

## Security Mode State Machine

```
SLEEP ──(PIR motion)──► WAKE ──(no person detected in 30s)──► SLEEP
  ▲                        │
  │                        ▼
  │                     SCANNING
  │                        │
  │              ┌─────────┴─────────┐
  │         (known person)      (unknown person)
  │              │                   │
  │           GREET             ALERT + RECORD
  │              │                   │
  └──────────────┴───────────────────┘
                 │
            (patrol complete)
```

---

## Privacy Considerations

- Face encodings stored **locally** (JSON file on Pi) — no cloud upload of biometric data
- Video stream never leaves local network unless explicitly sent via Telegram
- Telegram alerts contain snapshots, not a live stream
- Consider adding a **privacy mode** (disable camera, cover lens) triggered by Telegram command

---

## Security Feature Priority

| Priority | Feature | Complexity | Value |
|----------|---------|-----------|-------|
| 1 | Telegram alerts + photo | Low | High |
| 2 | Motion-triggered wake | Low | High |
| 3 | Person detection (YOLO) | Medium | High |
| 4 | Face recognition | Medium | Medium |
| 5 | Autonomous patrol with map | High | High |
| 6 | Sound detection | Low | Medium |
| 7 | Night vision (IR) | Low (camera swap) | Medium |
