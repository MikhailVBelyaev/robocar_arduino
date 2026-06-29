# Baby Companion Robot

## Vision

Transform the robocar into a mobile baby companion that can:
- **Follow the baby** visually around the room
- **Talk and sing** to the baby in English and Mandarin Chinese
- **Detect distress** (crying) and alert parents
- **Teach language** through interactive conversation and songs
- **Monitor safety** (fall detection, crib proximity, room temperature)

This is the most emotionally compelling use case for this robot. A child can grow up with a friend that speaks two languages naturally.

---

## Feature 1: Visual Baby Tracking

### How it works
Camera stream → YOLO v8 Nano (person detection) → estimate baby position in frame → compute angular offset → send turn/forward commands to Arduino.

```python
# conceptual tracking loop (runs on Pi 5)
import cv2
from ultralytics import YOLO

model = YOLO("yolov8n.pt")  # nano model, fast on Pi 5

def track_baby(frame):
    results = model(frame, classes=[0])  # class 0 = person
    if not results[0].boxes:
        return None, None
    
    # Take the lowest bounding box (baby on floor vs. standing adult)
    boxes = results[0].boxes.xyxy.cpu().numpy()
    lowest = max(boxes, key=lambda b: b[3])  # max y2 = closest to floor
    
    cx = (lowest[0] + lowest[2]) / 2  # center x
    cy = (lowest[1] + lowest[3]) / 2  # center y
    return cx, cy

def compute_movement(cx, frame_width, target_distance_pixels=100):
    center = frame_width / 2
    offset = cx - center
    
    if abs(offset) > 50:       # turn threshold
        return "right" if offset > 0 else "left"
    elif offset < -target_distance_pixels:
        return "forward"       # baby is far, approach
    return None                # stay still
```

### Hardware needed
- Pi Camera Module 3 Wide (170° FOV for broader room coverage)
- Raspberry Pi 5 (runs YOLOv8 Nano at ~10–15 FPS)

### Safety considerations
- Set a minimum distance stop (never approach closer than 30 cm)
- Stop tracking if no person detected for 5 seconds (don't chase shadows)
- Deactivate motors when speaker is speaking (avoid motor noise in audio)

---

## Feature 2: Voice Conversation in English and Mandarin

### Architecture
```
Microphone → Whisper (STT) → Claude API → TTS → Speaker
              ↑ detect wake word "Hey Robot" (Porcupine)
```

### Wake word detection (offline)
Use Picovoice Porcupine — runs on Pi without internet, very low latency:
```python
import pvporcupine
import pyaudio

porcupine = pvporcupine.create(
    access_key="YOUR_KEY",
    keywords=["hey robot", "baby robot"]
)
```

### Speech-to-text
Two options:
- **Cloud**: OpenAI Whisper API — excellent Mandarin recognition, requires internet
- **Local on Pi 5**: `faster-whisper` (CTranslate2 optimized) — runs `tiny` or `base` model at ~2–4× realtime on Pi 5

```python
from faster_whisper import WhisperModel

model = WhisperModel("base", device="cpu", compute_type="int8")

def transcribe(audio_path, language="zh"):  # zh for Chinese, en for English
    segments, info = model.transcribe(audio_path, language=language)
    return " ".join(s.text for s in segments)
```

### Language detection
Automatically detect if the baby is speaking English or Chinese:
```python
# Whisper auto-detects language
segments, info = model.transcribe(audio_path)
detected_language = info.language  # "en" or "zh"
```

### Conversation with Claude
```python
import anthropic

client = anthropic.Anthropic()

BABY_SYSTEM_PROMPT = """
You are a friendly, gentle robot companion for a young child (age 1-4).

Rules:
- Use very simple words and short sentences
- Be encouraging, warm, and playful
- When responding in Chinese, use Mandarin (Simplified Chinese characters)
- When responding in English, use clear, simple vocabulary
- Respond in the SAME language the child spoke
- If asked, you can sing simple songs or rhymes
- Never be scary or use complex concepts
- Topics: colors, animals, numbers 1-10, body parts, simple emotions
"""

def chat_with_baby(user_text: str, language: str) -> str:
    response = client.messages.create(
        model="claude-opus-4-8",  # most capable for nuanced child-safe responses
        max_tokens=200,
        system=BABY_SYSTEM_PROMPT,
        messages=[{"role": "user", "content": user_text}]
    )
    return response.content[0].text
```

### Text-to-speech
```python
# Option A: gTTS (Google, requires internet, excellent Chinese support)
from gtts import gTTS
import os

def speak(text, language="en"):
    lang_code = "zh-CN" if language == "zh" else "en"
    tts = gTTS(text=text, lang=lang_code, slow=False)
    tts.save("/tmp/response.mp3")
    os.system("mpg321 /tmp/response.mp3")

# Option B: pyttsx3 (offline, English only, lower quality)
import pyttsx3
engine = pyttsx3.init()
engine.say(text)
engine.runAndWait()

# Option C: ElevenLabs API (best quality voices, paid)
# Good for Chinese with natural-sounding voices
```

### Interactive language learning mode
```python
LANGUAGE_LESSON_PROMPTS = {
    "colors_en": "Let's learn colors! Can you point to something RED?",
    "colors_zh": "我们来学颜色！你能指出红色的东西吗？",
    "numbers_en": "Let's count together! One... two... three...",
    "numbers_zh": "我们一起数数！一... 二... 三...",
    "animals_en": "What sound does a cat make? Meow!",
    "animals_zh": "猫咪发出什么声音？喵喵！",
}
```

---

## Feature 3: Cry Detection

Detect baby crying and send alert to parent's phone.

```python
import librosa
import numpy as np

def is_baby_crying(audio_data, sample_rate=16000):
    # Baby cry frequency: 250–600 Hz fundamental, harmonics up to 2000 Hz
    # Extract MFCC features and compare to trained classifier
    mfccs = librosa.feature.mfcc(y=audio_data, sr=sample_rate, n_mfcc=13)
    # Simple rule: energy in 200-700 Hz band sustained > 2s
    freq_bins = librosa.fft_frequencies(sr=sample_rate)
    stft = np.abs(librosa.stft(audio_data))
    cry_band = stft[(freq_bins >= 200) & (freq_bins <= 700), :]
    energy = cry_band.mean()
    return energy > CRY_THRESHOLD
```

**Telegram alert**:
```python
import requests

def send_alert(photo_path=None):
    bot_token = "YOUR_BOT_TOKEN"
    chat_id = "YOUR_CHAT_ID"
    
    if photo_path:
        with open(photo_path, "rb") as photo:
            requests.post(
                f"https://api.telegram.org/bot{bot_token}/sendPhoto",
                data={"chat_id": chat_id, "caption": "Baby needs attention!"},
                files={"photo": photo}
            )
    else:
        requests.post(
            f"https://api.telegram.org/bot{bot_token}/sendMessage",
            data={"chat_id": chat_id, "text": "Baby is crying!"}
        )
```

---

## Feature 4: Fall Detection

Use camera + MediaPipe pose estimation to detect if a baby has fallen:

```python
import mediapipe as mp

pose = mp.solutions.pose.Pose()

def detect_fall(frame):
    results = pose.process(frame)
    if not results.pose_landmarks:
        return False
    
    # If head landmark is close to the floor (low y value in image = high y in real world)
    head = results.pose_landmarks.landmark[mp.solutions.pose.PoseLandmark.NOSE]
    hip = results.pose_landmarks.landmark[mp.solutions.pose.PoseLandmark.LEFT_HIP]
    
    # If head y ≈ hip y → person is horizontal → possible fall
    if abs(head.y - hip.y) < 0.1:  # normalized coordinates
        return True
    return False
```

---

## Feature 5: Lullaby and Song Mode

Pre-load MP3 songs (lullabies in English and Chinese) on the Pi:

```
/home/pi/songs/
├── en/
│   ├── twinkle_twinkle.mp3
│   ├── row_your_boat.mp3
│   └── wheels_on_the_bus.mp3
└── zh/
    ├── xiao_xing_xing.mp3   # 小星星 (Twinkle Twinkle in Chinese)
    ├── liang_zhi_hu_die.mp3  # 两只蝴蝶
    └── shui_ya_shui.mp3      # 睡呀睡
```

Claude can pick the right song based on context:
```python
def choose_song(context: str, language: str) -> str:
    prompt = f"Baby context: {context}. Language: {language}. Which song filename should I play? Return only the filename."
    response = client.messages.create(...)
    return response.content[0].text.strip()
```

---

## Sample Interaction Flow

```
[Wake word detected]
Robot: "Hello! 你好！"

Baby: "狗狗" (dog dog)

[Whisper detects "zh" language]

Claude (system: baby companion, simple Chinese):
→ "对！狗狗！汪汪汪！你喜欢狗狗吗？"
  (Yes! Dog dog! Woof woof! Do you like dogs?)

[gTTS speaks in Mandarin]

Baby: [cries]

[Cry detection triggers]
→ Robot plays soft lullaby
→ Telegram notification sent to parents
→ Robot slowly approaches baby position
```

---

## Development Phases for Baby Companion

| Phase | Feature | Hardware Required | Estimated Time |
|-------|---------|-------------------|----------------|
| 1 | Voice chat (English only) | Pi 5 + USB mic + speaker | 2–3 days |
| 2 | Chinese language support | Same (cloud Whisper + gTTS) | 1 day |
| 3 | Cry detection | Same | 1–2 days |
| 4 | Baby tracking (camera follow) | + Pi Camera Module 3 | 3–5 days |
| 5 | Fall detection | Same camera | 2–3 days |
| 6 | Song/lullaby library | Same audio hardware | 1 day |
| 7 | Telegram alerts with photo | Same | 1 day |
