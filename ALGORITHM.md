# Robocar Room Scanning Algorithm

## Objective
The goal is to calculate the square area of a room by having a robocar trace its perimeter without colliding with walls or obstacles. The robocar uses an ultrasonic sensor with a 180-degree field of view, providing 5-6 distance measurements at angles (e.g., 10°, 40°, 70°, 100°, 130°, 160°), and supports commands: `scan`, `forward`, `back`, `left`, and `right`. The algorithm ensures systematic coverage, collision avoidance, and accurate area estimation.

## Strategy
The algorithm employs a **right-hand wall-following** approach, inspired by the Bug2 algorithm in robotics, to trace the room’s perimeter while maintaining a safe distance from walls (~25 cm). Key principles include:
- **Initial Wall Detection**: Perform an initial scan to orient the car toward the closest wall.
- **Wall-Following**: Keep the wall on the right at a target distance (20-50 cm), moving forward when safe, turning left when too close to a wall ahead, or turning right when too far from the right wall.
- **Collision Avoidance**: Prevent forward moves when the distance ahead is below a safety threshold (20 cm).
- **Area Calculation**: Track the car’s path and use the shoelace formula to compute the enclosed area.
- **Outlier Handling**: Filter invalid sensor readings (e.g., >1000 cm) to ensure reliable decisions.
- **Stopping Condition**: Stop when the car returns to its starting position with a similar orientation or after a maximum number of steps.

This strategy addresses issues observed in logs (e.g., moving toward walls with 16-19 cm distances, incomplete loops leading to a 2.50 m² estimate).

## Mathematical Foundation

### Position Tracking
The car’s position \((x, y)\) is updated based on its orientation \(\theta\) (0°: north, 90°: east, 180°: south, 270°: west) and step distance (\(d = 0.5 \, \text{m}\)):
\[
x_{\text{new}} = x + d \cdot \cos(\theta), \quad y_{\text{new}} = y + d \cdot \sin(\theta)
\]
For discrete 90° orientations:
- \(\theta = 0^\circ\): \((x, y + d)\)
- \(\theta = 90^\circ\): \((x + d, y)\)
- \(\theta = 180^\circ\): \((x, y - d)\)
- \(\theta = 270^\circ\): \((x - d, y)\)

### Area Calculation (Shoelace Formula)
Given path points \((x_0, y_0), (x_1, y_1), \ldots, (x_{n-1}, y_{n-1})\), the area is:
\[
A = \frac{1}{2} \left| \sum_{i=0}^{n-1} (x_i y_{i+1} - x_{i+1} y_i) \right|
\]
where \(i+1\) wraps to 0 for a closed polygon.

### Sensor Data
- Angles (10°, 40°, 70°, 100°, 130°, 160°) are mapped to directions:
  - Right: 10°, 40°
  - Forward: 70°, 100°
  - Left: 130°, 160°
- Distances >1000 cm are filtered as outliers.
- Minimum distances per direction guide movement decisions.

### Collision Avoidance
- If `forward_distance < 20 cm`, turn left.
- Maintain `right_distance ≈ 25 cm`, turning right if >50 cm or left if <25 cm.

## Algorithm Steps
1. **Initialize**:
   - Set \((x, y) = (0, 0)\), \(\theta = 0^\circ\), `path = [(0, 0)]`.
   - Perform initial `scan` to get distance measurements.
   - Create a unique log file for the session.

2. **Phase 1: Find a Wall**:
   - **Loop**: While the path ahead is clear (`forward_distance > MAX_WALL_DISTANCE_CM`):
     - Issue a `forward` command to move towards a wall.
     - Get new sensor data from the move.
     - If no wall is found after several steps, stop to avoid getting lost.
   - This phase ensures the robot is close to a perimeter wall before starting to trace it.

3. **Phase 2: Align and Follow Wall**:
   - Once a wall is detected ahead, issue a `left` turn to align the robot, placing the wall on its right side. This positions the robot to begin right-hand wall-following.
   - **Wall-Following Loop** (while `running` and `step_count < MAX_STEPS`):
     - **Sense**: Analyze the current sensor data.
     - **Decide** the next action based on the right-hand rule:
       - If `forward_distance < MIN_DISTANCE_CM` (e.g., 20 cm) -> Turn `left` (to avoid collision).
       - Else if `right_distance > MAX_WALL_DISTANCE_CM` (e.g., 50 cm) -> Turn `right` (to follow an outer corner or find a lost wall).
       - Else if `right_distance < TARGET_WALL_DISTANCE_CM` (e.g., 25 cm) -> Turn `left` (to move away from an inner corner or wall).
       - Else -> Move `forward` (ideal distance from the wall).
     - **Act**: Execute the decided move and get new sensor data for the next iteration.
     - Update position for `forward` moves and orientation for turns.
     - Append the new position to the `path`.
     - Check stopping condition: stop if `step_count > 10` and the car is near the start position with a similar orientation.

4. **Calculate Area**: Apply the shoelace formula to `path`.

5. **Stop**: Allow user to stop with `s` using thread-safe synchronization.

## Implementation Notes
- **Language**: Python, using `requests` for HTTP communication with the Arduino at `http://192.168.100.236`.
- **Logging**: A new log file is created in a `logs/` directory for each run, capturing all events for later analysis.
- **Commands**: `scan`, `forward`, `back`, `left`, `right` return `distance_measurements` in JSON format.
- **Parameters**:
  - `MIN_DISTANCE_CM = 20`
  - `TARGET_WALL_DISTANCE_CM = 25`
  - `MAX_WALL_DISTANCE_CM = 50`
  - `STEP_DISTANCE_M = 0.5`
  - `MAX_VALID_DISTANCE_CM = 1000`
  - `MAX_STEPS = 100`
- **Threading**: Uses `threading.Lock` for responsive stopping.
- **Logging**: Includes timestamps, levels (INFO, DEBUG, WARN, ERROR), and diagnostics (e.g., position, orientation, distances).

## Diagnostic Framework for Future Improvements
- **Log Analysis**:
  - Check for collision risks (forward moves when `forward_distance < 20 cm`).
  - Verify wall-following (right_distance ≈ 25 cm).
  - Monitor outliers (>1000 cm) and their frequency.
  - Confirm closed loop (path returns to start).
- **Issues from Log (2025-07-06 16:05:57)**:
  - Forward moves at 16-19 cm (e.g., 16:06:35) risked collisions.
  - Incomplete loop led to low area (2.50 m²).
  - Repeated `s`/`exit` ignored due to threading issues.
- **Metrics to Track**:
  - Number of steps to complete a loop.
  - Area estimate vs. actual room size.
  - Frequency of turns vs. forward moves.
  - Outlier rate (distances >1000 cm).
- **Potential Improvements**:
  - Add obstacle avoidance for tables (back up if all directions <20 cm).
  - Detect doorways (large distances, e.g., >500 cm) for multi-room scanning.
  - Calibrate `STEP_DISTANCE_M` against actual movement.
  - Adjust `TARGET_WALL_DISTANCE_CM` based on room size.