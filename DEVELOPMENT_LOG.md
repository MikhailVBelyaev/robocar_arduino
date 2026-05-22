# Arduino Robocar Development Log

## 📅 2025-05-01

### ✅ Tasks Completed
- Set up Arduino Nano R4 with Wi-Fi for local network communication
- Configured HTTP endpoints for movement control:
  - **`/forward`** – Move forward
  - **`/back`** – Move backward
  - **`/left`** – Turn left
  - **`/right`** – Turn right
  - **`/scan`** – Initial room scan
- Created Python script (`room_scanner.py`) to handle HTTP requests and room scanning logic
- The car can move based on simple HTTP commands sent via the local network
- Implemented basic room scanning logic, using the ultrasonic sensor for distance measurements
- Estimated area after initial scan: 52m² (which is incorrect due to poor estimation logic)

### 🧪 Results
- The car successfully responds to movement commands via the HTTP interface.
- The scan is being performed, but the area calculation needs to be more accurate. The current logic uses a rough estimation which overestimates the room size.
- The car is able to navigate the room but needs refinement in its decision-making and obstacle avoidance.

### 🧭 Plan for 2025-05-02
- Improve room area estimation logic to match the actual room size more accurately (e.g., account for obstacles like tables and machines).
- Add functionality to move the car between rooms or stop when the scan completes.
- Continue working on fine-tuning the scanning logic (e.g., handle various room configurations).
- Integrate potential improvements for error handling in the room scanning process.
- Review the Python script and identify areas for optimization.

### 🤖 Prompt for Next Session
> Improve room scanning logic and refine area estimation to more accurately represent the physical space. Develop error handling and further optimize the movement logic. Ensure that the car stops at the end of its scan and doesn't overestimate the room size.

## 📅 2025-07-06

### ✅ Major Improvements
- Reworked navigation system from simple direction selection to wall-following autonomous navigation
- Added coordinate tracking system:
  - robot position (x, y)
  - orientation tracking (0°, 90°, 180°, 270°)
  - movement path history
- Implemented polygon-based room area calculation using the shoelace formula
- Added two-phase navigation logic:
  - wall seeking
  - wall following
- Added sensor validation and outlier filtering
- Added retry logic for unstable HTTP/network communication
- Added detailed logging system with timestamped log files
- Added thread-safe control using locks
- Added stopping conditions:
  - maximum step protection
  - return-to-origin detection
- Improved obstacle avoidance and wall distance control

### 🧪 Results
- Navigation became significantly more stable
- Area estimation is now geometry-based instead of move-count estimation
- Robot can maintain approximate wall distance during exploration
- Logs now provide detailed diagnostics for debugging and future ML analysis
- Sensor outliers are now filtered correctly

### ⚠️ Current Limitations
- Position estimation still relies on dead reckoning and accumulates drift
- No real SLAM or map correction yet
- Ultrasonic sensor noise still affects navigation precision
- Orientation changes are approximate
- No probabilistic obstacle model yet

### 🧭 Next Planned Improvements
- Add occupancy grid mapping
- Add LiDAR support
- Add IMU/gyro integration
- Implement SLAM algorithms
- Add visualization of explored path
- Export map data to JSON/CSV
- Add autonomous room-to-room exploration
- Add battery monitoring and auto-return logic
