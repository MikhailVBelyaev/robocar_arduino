# Room Measurement — Algorithms and Improvement Plan

## Current State and Problems

The current scanner (`room_scanner_06_07_25.py`) uses **dead reckoning + shoelace formula**:
- Assume 0.5 m per forward move.
- Track (x, y) with 90° orientation steps.
- Compute polygon area when the loop closes.

**Observed problems** (from `DEVELOPMENT_LOG.md`):
1. **Drift** — actual distance per move varies with battery voltage; 0.5 m is a rough estimate.
2. **Slow scan** — servo sweep takes ~4 s; during that time the robot drifts if on a slope.
3. **No map persistence** — every run starts from scratch.
4. **Area underestimate** — incomplete loops produce small polygons (observed: 2.5 m² for a larger room).
5. **Sensor noise** — single ultrasonic reading per angle; objects near walls cause spurious readings.

---

## Improvement Path

### Level 1: Calibration (no new hardware)

**Goal**: Make dead reckoning more accurate without adding sensors.

1. **Calibrate `STEP_DISTANCE_M`**: Mark 1 m on the floor, run `/forward`, measure actual distance. Update constant. Do this when battery is fresh.

2. **Multi-sample ultrasonic**: Take 3 readings per angle and use the median. Reduces noise significantly.
   ```python
   def distance_median(n=3):
       samples = [Distance_test() for _ in range(n)]
       return sorted(samples)[n // 2]
   ```

3. **Finer servo sweep**: Change sweep from 30° steps to 15° steps → 12 readings instead of 6. Costs ~8 s but gives a much clearer picture of the room geometry.

4. **Voltage-corrected step distance**: Read battery voltage via ADC on Arduino, scale `STEP_DISTANCE_M` accordingly (higher voltage → longer actual step).

**Expected improvement**: area error from ±50% → ±20%.

---

### Level 2: IMU Integration (MPU-6050, ~$3)

**Goal**: Correct orientation drift. The biggest source of error is not knowing the exact turning angle.

The robot currently assumes exactly 90° turns, but:
- Motor timing is inconsistent (floor friction, battery level).
- Actual turn can be 75°–105°.

**With MPU-6050**:
- Read gyroscope during turn → integrate angular velocity → know actual heading.
- Update `self.orientation` with measured angle instead of assumed 90°.

```python
# pseudocode for IMU-corrected turn
def turn_left_imu():
    start_heading = imu.get_heading()
    send_command("left")
    while imu.get_heading() - start_heading < 90:
        time.sleep(0.01)
    send_command("stop")
    actual_angle = imu.get_heading() - start_heading
    self.orientation = (self.orientation - actual_angle) % 360
```

**Expected improvement**: orientation error from ±15° → ±2°, area error from ±20% → ±8%.

---

### Level 3: Wheel Encoders (Hall-effect, ~$10)

**Goal**: Know exact distance traveled per forward move.

Replace fixed `STEP_DISTANCE_M = 0.5` with encoder-measured distance:
- Count encoder pulses during forward move.
- Known: wheel circumference × (pulses / PPR) = distance.

```cpp
// Arduino: count encoder pulses in ISR
volatile long left_pulses = 0;
void IRAM_ATTR left_encoder_isr() { left_pulses++; }

// Report in JSON response
jsonResponse += "\"odometry\": {\"left\": " + String(left_pulses) + ", \"right\": " + String(right_pulses) + "}";
```

**Expected improvement**: distance error from ±20% → ±3%.

---

### Level 4: SLAM with RPLIDAR (RPLIDAR A1, ~$100) — Most Accurate

**Goal**: Build a 2D map of the room with centimeter accuracy. Measure area from the map, not from robot path.

**Stack**:
- RPLIDAR A1 → USB → Raspberry Pi
- ROS2 (Humble or Jazzy) on Pi
- `rplidar_ros` package for scan data
- `slam_toolbox` for SLAM
- `robot_localization` for EKF (fuse IMU + odometry + lidar)

**Algorithm overview**:
```
RPLIDAR scan → /scan topic
IMU data     → /imu topic       →  EKF  → /odom_corrected
Odometry     → /odom topic   ↗

slam_toolbox reads /scan + /odom_corrected → builds OccupancyGrid
OccupancyGrid → count free cells × cell_size² → room area
```

**Area calculation from occupancy grid**:
```python
import numpy as np
from nav_msgs.msg import OccupancyGrid

def calculate_area_from_map(occupancy_grid: OccupancyGrid) -> float:
    data = np.array(occupancy_grid.data).reshape(
        occupancy_grid.info.height, occupancy_grid.info.width
    )
    resolution = occupancy_grid.info.resolution  # meters per cell
    free_cells = np.sum(data == 0)  # 0 = free, 100 = occupied, -1 = unknown
    return free_cells * (resolution ** 2)
```

**Expected improvement**: area error ±2–3% (limited by sensor noise and SLAM drift).

---

### Level 5: Full SLAM + Room Segmentation (advanced)

**Goal**: Identify individual rooms, label them, and export a floor plan.

**Additional tools**:
- `map_server` — save/load maps between runs
- Room segmentation: flood-fill free space from doorways → label connected regions
- Export to SVG/PDF floor plan

**Libraries**:
- `scikit-image` — morphological operations to segment rooms
- `shapely` — compute polygon areas for each labeled region
- `matplotlib` — render the map

---

## Room Area from SLAM Map — Step by Step (once Pi 5 is available)

```bash
# 1. Install ROS2 and packages (on Pi 5)
sudo apt install ros-jazzy-slam-toolbox ros-jazzy-rplidar-ros ros-jazzy-robot-localization

# 2. Launch RPLIDAR
ros2 launch rplidar_ros rplidar_a1_launch.py

# 3. Launch SLAM
ros2 launch slam_toolbox online_async_launch.py

# 4. Drive the robot (teleoperate or autonomous)
ros2 run teleop_twist_keyboard teleop_twist_keyboard

# 5. Save the map
ros2 run nav2_map_server map_saver_cli -f ~/maps/my_room

# 6. Compute area
python3 src/area_from_map.py ~/maps/my_room.pgm ~/maps/my_room.yaml
```

---

## Accuracy Comparison

| Method | Hardware | Area Error | Map Quality |
|--------|----------|-----------|-------------|
| Current (dead reckoning) | Arduino only | ±50% | None |
| + Calibration | Arduino only | ±20% | None |
| + IMU | Arduino + MPU-6050 | ±8% | None |
| + Encoders + IMU | Arduino + extras | ±5% | Path only |
| SLAM (RPLIDAR) | + Pi + LiDAR | ±2–3% | Full 2D map |

---

## Immediate Next Step (no new hardware)

Replace single-sample ultrasonic with 3-sample median in `WiFiCar.ino`:

```cpp
int Distance_test_median() {
    int samples[3];
    for (int i = 0; i < 3; i++) {
        digitalWrite(Trig, LOW);
        delayMicroseconds(2);
        digitalWrite(Trig, HIGH);
        delayMicroseconds(20);
        digitalWrite(Trig, LOW);
        samples[i] = (int)(pulseIn(Echo, HIGH) / 58.0);
        delay(30);
    }
    // Sort and return median
    if (samples[0] > samples[1]) { int t = samples[0]; samples[0] = samples[1]; samples[1] = t; }
    if (samples[1] > samples[2]) { int t = samples[1]; samples[1] = samples[2]; samples[2] = t; }
    if (samples[0] > samples[1]) { int t = samples[0]; samples[0] = samples[1]; samples[1] = t; }
    return samples[1];
}
```

This single change will reduce spurious wall detections and improve navigation consistency.
