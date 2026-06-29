# AI Features — Integration Guide

## Overview

All AI features run on the Raspberry Pi 5 or Jetson Orin Nano co-processor. The Arduino handles only real-time motor/sensor control. AI features are connected via the Anthropic Claude API and other open-source models.

---

## Claude API Integration

### Why Claude for this robot

- **Claude Opus 4.8** is the best model for nuanced, safe, context-aware conversation with children.
- Claude's safety training makes it naturally suitable for a baby companion — it will not generate harmful, scary, or age-inappropriate content.
- Claude handles **multilingual** responses (English and Chinese) without any special prompting.
- Claude can act as the **robot's brain** for decision-making beyond simple rules.

### Setup

```python
import anthropic

client = anthropic.Anthropic(api_key="ANTHROPIC_API_KEY")
# Store API key in environment variable, not in code
```

### Model selection

| Model | Use case | Notes |
|-------|---------|-------|
| `claude-opus-4-8` | Baby conversation, security decisions | Highest quality, slower |
| `claude-sonnet-4-6` | Real-time interaction, faster responses | Good quality, faster |
| `claude-haiku-4-5-20251001` | Quick commands, classification | Fastest, minimal quality |

For baby conversation: use `claude-opus-4-8`.
For security detection narration: use `claude-sonnet-4-6`.
For quick classification (is this command "stop" or "go"?): use `claude-haiku-4-5-20251001`.

### Core conversation wrapper

```python
from anthropic import Anthropic

client = Anthropic()

class RobotBrain:
    def __init__(self, mode: str = "baby_companion"):
        self.mode = mode
        self.conversation_history = []
        self.system_prompts = {
            "baby_companion": BABY_SYSTEM_PROMPT,
            "security": SECURITY_SYSTEM_PROMPT,
            "general": GENERAL_SYSTEM_PROMPT,
        }
    
    def chat(self, user_message: str) -> str:
        self.conversation_history.append({
            "role": "user",
            "content": user_message
        })
        
        response = client.messages.create(
            model="claude-opus-4-8",
            max_tokens=300,
            system=self.system_prompts[self.mode],
            messages=self.conversation_history
        )
        
        assistant_message = response.content[0].text
        self.conversation_history.append({
            "role": "assistant",
            "content": assistant_message
        })
        
        # Keep conversation history manageable (last 10 exchanges)
        if len(self.conversation_history) > 20:
            self.conversation_history = self.conversation_history[-20:]
        
        return assistant_message
    
    def reset(self):
        self.conversation_history = []
```

---

## Claude as Robot Decision Engine

Beyond conversation, Claude can make complex decisions:

```python
NAVIGATION_DECISION_PROMPT = """
You are a robot navigation assistant. Given sensor data, decide the next action.

Sensor data: {sensor_json}
Current position: {position}
Goal: {goal}
Obstacles: {obstacles}

Respond with ONLY one of: "forward", "left", "right", "back", "stop"
"""

def decide_action(sensor_data: dict, position: tuple, goal: tuple) -> str:
    prompt = NAVIGATION_DECISION_PROMPT.format(
        sensor_json=json.dumps(sensor_data),
        position=position,
        goal=goal,
        obstacles=sensor_data.get("obstacles", [])
    )
    
    response = client.messages.create(
        model="claude-haiku-4-5-20251001",  # fast for real-time decisions
        max_tokens=10,
        messages=[{"role": "user", "content": prompt}]
    )
    return response.content[0].text.strip().lower()
```

---

## Vision AI (YOLO)

### Installation on Pi 5

```bash
pip install ultralytics
# Downloads ~6 MB model
```

### Models by speed/accuracy trade-off

| Model | Size | Pi 5 FPS | Accuracy |
|-------|------|----------|----------|
| yolov8n (nano) | 6 MB | 10–15 FPS | Good |
| yolov8s (small) | 22 MB | 5–8 FPS | Better |
| yolov8m (medium) | 50 MB | 2–4 FPS | Best for Pi |

For real-time tracking: use `yolov8n`.

### Baby vs. adult detection

```python
from ultralytics import YOLO
import cv2

model = YOLO("yolov8n.pt")

def detect_people(frame):
    results = model(frame, classes=[0], conf=0.5)  # class 0 = person
    
    people = []
    for box in results[0].boxes:
        x1, y1, x2, y2 = box.xyxy[0].cpu().numpy()
        height = y2 - y1
        width = x2 - x1
        
        # Classify as baby if bounding box height is < 40% of frame height
        is_baby = height < (frame.shape[0] * 0.4)
        
        people.append({
            "bbox": (x1, y1, x2, y2),
            "is_baby": is_baby,
            "confidence": float(box.conf[0])
        })
    
    return people
```

---

## Speech Pipeline

### Complete pipeline on Pi 5

```
[Porcupine wake word] → [record audio] → [Whisper STT] → [Claude] → [gTTS/ElevenLabs] → [speaker]
```

### Wake word + record

```python
import pvporcupine
import pyaudio
import wave

def listen_for_command(porcupine, pa):
    # Listen for wake word
    audio_stream = pa.open(
        rate=porcupine.sample_rate,
        channels=1,
        format=pyaudio.paInt16,
        input=True,
        frames_per_buffer=porcupine.frame_length
    )
    
    print("Listening for wake word...")
    while True:
        pcm = audio_stream.read(porcupine.frame_length)
        pcm = struct.unpack_from("h" * porcupine.frame_length, pcm)
        keyword_index = porcupine.process(pcm)
        
        if keyword_index >= 0:
            print("Wake word detected! Recording...")
            # Record 3 seconds of command
            return record_audio(audio_stream, duration=3)
```

### Whisper STT (local, supports Chinese)

```python
from faster_whisper import WhisperModel

# Load once at startup
whisper_model = WhisperModel("base", device="cpu", compute_type="int8")
# "base" model: ~140 MB, runs at ~2x realtime on Pi 5

def transcribe(audio_path: str) -> tuple[str, str]:
    segments, info = whisper_model.transcribe(audio_path, beam_size=1)
    text = " ".join(s.text for s in segments)
    language = info.language  # "en", "zh", "ru", etc.
    return text, language
```

### TTS with Chinese support

```python
from gtts import gTTS
import subprocess

def speak(text: str, language: str = "en"):
    lang_map = {
        "en": "en",
        "zh": "zh-CN",
        "ru": "ru",
    }
    lang_code = lang_map.get(language, "en")
    
    tts = gTTS(text=text, lang=lang_code, slow=False)
    tts.save("/tmp/robot_speech.mp3")
    subprocess.run(["mpg321", "-q", "/tmp/robot_speech.mp3"])
```

---

## TV Remote Control (IR Blaster)

### Hardware setup
```
Pi GPIO17 → 1kΩ resistor → NPN transistor (BC547) base
                            collector → IR LED anode
                            emitter → GND
IR LED cathode → GND
```

### LIRC configuration
```bash
# /etc/lirc/lirc_options.conf
driver = default
device = /dev/lirc0

# /boot/config.txt (Pi)
dtoverlay=gpio-ir-tx,gpio_pin=17
```

### Control TV
```python
import subprocess

TV_CODES = {
    "power": "KEY_POWER",
    "volume_up": "KEY_VOLUMEUP",
    "volume_down": "KEY_VOLUMEDOWN",
    "mute": "KEY_MUTE",
    "channel_up": "KEY_CHANNELUP",
    "channel_down": "KEY_CHANNELDOWN",
    "ok": "KEY_OK",
    "back": "KEY_BACK",
}

def send_ir_command(command: str, remote: str = "samsung_tv"):
    code = TV_CODES.get(command)
    if code:
        subprocess.run(["irsend", "SEND_ONCE", remote, code])

# Or use python-broadlink for Broadlink IR blaster (simpler, no wiring)
import broadlink
devices = broadlink.discover()
device = devices[0]
device.auth()
device.send_data(IR_CODE_BYTES)  # recorded codes
```

### Voice-controlled TV
```python
def handle_tv_command(text: str, language: str):
    # Use Claude to parse natural language TV command
    prompt = f"""
    User said: "{text}" (language: {language})
    Map this to one TV command: power, volume_up, volume_down, mute, channel_up, channel_down, ok, back
    If none matches, respond with "unknown".
    Respond with ONLY the command name.
    """
    
    response = client.messages.create(
        model="claude-haiku-4-5-20251001",
        max_tokens=20,
        messages=[{"role": "user", "content": prompt}]
    )
    
    command = response.content[0].text.strip()
    if command != "unknown":
        send_ir_command(command)
        speak(f"OK, {command.replace('_', ' ')}", language)
```

---

## Home Assistant Integration

Expose the robot as a Home Assistant entity:

```python
import paho.mqtt.client as mqtt

HA_MQTT_BROKER = "192.168.1.x"
HA_TOPIC_PREFIX = "homeassistant/sensor/robocar"

def publish_state(client, state: dict):
    # Battery level
    client.publish(f"{HA_TOPIC_PREFIX}/battery/state", state["battery"])
    # Position
    client.publish(f"{HA_TOPIC_PREFIX}/position/state", 
                   json.dumps({"x": state["x"], "y": state["y"]}))
    # Mode
    client.publish(f"{HA_TOPIC_PREFIX}/mode/state", state["mode"])
    # Last detection
    client.publish(f"{HA_TOPIC_PREFIX}/detection/state", state["last_detection"])
```

Home Assistant automations can then trigger robot patrol when the front door opens, or pause the robot when someone is sleeping.

---

## AI Cost Estimate (Claude API)

Assuming moderate daily use:

| Use case | Tokens/interaction | Interactions/day | Cost/day |
|----------|------------------|--------------------|---------|
| Baby chat (Opus 4.8) | ~500 in + 200 out | 20 | ~$0.40 |
| Security narration (Sonnet 4.6) | ~300 in + 100 out | 5 | ~$0.02 |
| TV commands (Haiku 4.5) | ~100 in + 20 out | 10 | ~$0.001 |
| **Total** | | | **~$0.42/day** |

Monthly: ~$12–15/month. Very affordable for a household AI assistant.

> Reduce cost by using `claude-sonnet-4-6` for baby chat during active periods and `claude-haiku-4-5-20251001` for simple yes/no decisions.
