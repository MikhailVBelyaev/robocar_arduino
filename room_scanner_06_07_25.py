import requests
import time
import threading
import json
import os
import math
from datetime import datetime

BASE_URL = "http://192.168.100.236"
MOVE_COMMANDS = {
    "forward": "/forward",
    "left": "/left",
    "right": "/right",
    "back": "/back"
}
SCAN_PATH = "/"
MIN_DISTANCE_CM = 60  # Minimum safe distance, MUST be > STEP_DISTANCE_M * 100
TARGET_WALL_DISTANCE_CM = 25  # Ideal distance to maintain from right wall
MAX_WALL_DISTANCE_CM = 50  # Maximum distance to consider wall on right
STEP_DISTANCE_M = 0.5  # Distance per move (0.5m)
RETRY_ATTEMPTS = 3  # Number of retries for failed requests
MAX_STEPS = 100  # Maximum steps to prevent infinite loops
MAX_VALID_DISTANCE_CM = 1000  # Maximum valid sensor distance

class Scanner:
    def __init__(self):
        self.running = False
        self.path = [(0, 0)]  # List of (x, y) coordinates for area calculation
        self.position = [0, 0]  # Current position [x, y] in meters
        self.orientation = 0  # Orientation in degrees (0: north, 90: east, 180: south, 270: west)
        self.step_count = 0  # Track number of moves
        self.start_position = [0, 0]  # Starting position
        self.start_orientation = 0  # Starting orientation
        self.lock = threading.Lock()  # For thread-safe stopping
        self.outlier_count = 0  # Track outliers for diagnostics
        self.log_file = None # Path to the log file for the current run

    def log(self, level, message):
        """Log messages with timestamp and level."""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_message = f"[{timestamp}] [{level}] {message}"
        print(log_message)
        if self.log_file:
            try:
                with open(self.log_file, "a") as f:
                    f.write(log_message + "\n")
            except Exception as e:
                print(f"Failed to write to log file: {e}")

    def get_scan_data(self, path="/"):
        """Fetch and validate scan data from the Arduino."""
        for attempt in range(RETRY_ATTEMPTS):
            try:
                self.log("INFO", f"Sending request to {path}...")
                response = requests.get(BASE_URL + path, timeout=15)
                response.raise_for_status()
                data = response.json()
                measurements = data.get("distance_measurements", [])
                # Validate and filter measurements
                valid_measurements = []
                for point in measurements:
                    if not isinstance(point.get("angle"), (int, float)) or not isinstance(point.get("distance"), (int, float)):
                        raise ValueError("Invalid measurement format")
                    if point.get("distance") < 0 or point.get("distance") > MAX_VALID_DISTANCE_CM:
                        self.outlier_count += 1
                        self.log("WARN", f"Ignoring invalid distance: {point.get('distance')} cm at angle {point.get('angle')}")
                        continue
                    valid_measurements.append(point)
                self.log("DEBUG", f"Raw response: {data}, Valid measurements: {valid_measurements}")
                return valid_measurements
            except Exception as e:
                self.log("ERROR", f"Request to {path} failed (attempt {attempt + 1}/{RETRY_ATTEMPTS}): {e}")
                if attempt < RETRY_ATTEMPTS - 1:
                    time.sleep(2)
                else:
                    return []
        return []

    def move(self, direction):
        """Send move command and update position/orientation."""
        with self.lock:
            if not self.running:
                return []
            try:
                response = self.get_scan_data(MOVE_COMMANDS[direction])
                if not response:
                    raise ValueError("No valid data from move command")

                # Update position and orientation
                if direction == "forward":
                    if self.orientation == 0:
                        self.position[1] += STEP_DISTANCE_M  # Move north
                    elif self.orientation == 90:
                        self.position[0] += STEP_DISTANCE_M  # Move east
                    elif self.orientation == 180:
                        self.position[1] -= STEP_DISTANCE_M  # Move south
                    elif self.orientation == 270:
                        self.position[0] -= STEP_DISTANCE_M  # Move west
                    self.path.append(tuple(self.position))
                elif direction == "left":
                    self.orientation = (self.orientation - 90) % 360
                elif direction == "right":
                    self.orientation = (self.orientation + 90) % 360
                elif direction == "back":
                    self.orientation = (self.orientation + 180) % 360

                self.step_count += 1
                self.log("INFO", f"Moved {direction}. Position: {self.position}, Orientation: {self.orientation}°, Path length: {len(self.path)}")
                return response
            except Exception as e:
                self.log("ERROR", f"Move {direction} failed: {e}")
                return []

    def calculate_area(self):
        """Calculate enclosed area using the shoelace formula."""
        if len(self.path) < 3:
            self.log("WARN", "Insufficient path points for area calculation")
            return 0.0
        area = 0.0
        n = len(self.path)
        for i in range(n):
            j = (i + 1) % n
            area += self.path[i][0] * self.path[j][1]
            area -= self.path[j][0] * self.path[i][1]
        area = abs(area) / 2.0
        return area

    def _parse_distances(self, scan_data):
        """Helper to parse min distances for right, forward, and left."""
        if not scan_data:
            return float('inf'), float('inf'), float('inf')

        directions_map = {
            10: "right", 40: "right",
            70: "forward", 100: "forward",
            130: "left", 160: "left"
        }
        right_distances, forward_distances, left_distances = [], [], []
        for point in scan_data:
            direction = directions_map.get(point.get("angle"))
            if direction == "right":
                right_distances.append(point.get("distance"))
            elif direction == "forward":
                forward_distances.append(point.get("distance"))
            elif direction == "left":
                left_distances.append(point.get("distance"))

        right = min(right_distances) if right_distances else float('inf')
        forward = min(forward_distances) if forward_distances else float('inf')
        left = min(left_distances) if left_distances else float('inf')
        return right, forward, left

    def run_scan(self):
        """Run room scan using a two-phase wall-seeking and wall-following algorithm."""
        self.log("INFO", "Starting room scan...")
        scan = self.get_scan_data(SCAN_PATH)
        if not scan:
            self.log("ERROR", "Initial scan failed. Stopping.")
            self.running = False
            return

        # --- Phase 1: Find a Wall ---
        self.log("INFO", "Phase 1: Seeking a wall...")
        right_distance, forward_distance, left_distance = self._parse_distances(scan)
        seek_steps = 0
        # Move forward until a wall is within MAX_WALL_DISTANCE_CM or we hit a step limit
        while forward_distance > MAX_WALL_DISTANCE_CM and self.running and seek_steps < 10:
            self.log("INFO", f"Path ahead is clear (dist: {forward_distance:.0f} cm). Moving forward to find wall.")
            scan = self.move("forward")
            if not scan:
                self.log("ERROR", "Lost sensor data while seeking wall. Stopping.")
                break
            right_distance, forward_distance, left_distance = self._parse_distances(scan)
            seek_steps += 1

        if not self.running or seek_steps >= 10:
            if seek_steps >= 10:
                self.log("WARN", "Could not find a wall within 10 steps. Stopping.")
            self.stop()
            return

        # --- Phase 2: Align and Follow Wall ---
        self.log("INFO", "Phase 2: Wall detected. Aligning for wall-following (turning left).")
        scan = self.move("left") # Turn left to place the wall on the right

        # Main wall-following loop
        while self.running and self.step_count < MAX_STEPS:
            if not scan:
                self.log("ERROR", "No valid sensor data. Stopping scan.")
                break

            right_distance, forward_distance, left_distance = self._parse_distances(scan)
            self.log("DEBUG", f"Sensor data: Right={right_distance} cm, Forward={forward_distance} cm, Left={left_distance} cm")

            # Wall-following logic (Sense -> Decide -> Act)
            next_action = "forward"  # Default action
            if forward_distance < MIN_DISTANCE_CM:
                next_action = "left"
                self.log("INFO", f"Decision: Turn left (forward too close: {forward_distance} cm < {MIN_DISTANCE_CM} cm)")
            elif right_distance > MAX_WALL_DISTANCE_CM:
                next_action = "right"
                self.log("INFO", f"Decision: Turn right (right too far: {right_distance} cm > {MAX_WALL_DISTANCE_CM} cm)")
            elif right_distance < TARGET_WALL_DISTANCE_CM:
                next_action = "left"
                self.log("INFO", f"Decision: Turn left (right too close: {right_distance} cm < {TARGET_WALL_DISTANCE_CM} cm)")
            else:
                self.log("INFO", f"Decision: Continue forward (right distance: {right_distance} cm)")

            # Execute the decided move. The result is the scan data for the next iteration.
            scan = self.move(next_action)

            # Check stopping condition
            distance_to_start = math.sqrt((self.position[0] - self.start_position[0])**2 + (self.position[1] - self.start_position[1])**2)
            if self.step_count > 10 and distance_to_start < STEP_DISTANCE_M and abs(self.orientation - self.start_orientation) < 45:
                self.log("INFO", f"Returned to starting position (distance: {distance_to_start:.2f} m). Stopping scan.")
                break

        with self.lock:
            self.running = False
        area = self.calculate_area()
        self.log("INFO", f"Scan complete. Steps: {self.step_count}, Outliers: {self.outlier_count}")
        self.log("RESULT", f"Estimated room area: {area:.2f} m²")

    def start(self):
        """Start the scan in a separate thread."""
        with self.lock:
            if self.running:
                self.log("WARN", "Scan is already in progress.")
                return
            
            # Setup logging
            log_dir = "logs"
            os.makedirs(log_dir, exist_ok=True)
            timestamp = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
            self.log_file = os.path.join(log_dir, f"scan_{timestamp}.log")

            # Reset state and start
            self.running = True
            self.path = [(0, 0)]
            self.position = [0, 0]
            self.start_position = [0, 0]
            self.start_orientation = 0
            self.orientation = 0
            self.step_count = 0
            self.outlier_count = 0
            threading.Thread(target=self.run_scan).start()

    def stop(self):
        """Stop the scan."""
        with self.lock:
            if self.running:
                self.running = False
                self.log("INFO", "Stopping scan...")

def main():
    scanner = Scanner()
    print("🚗 Arduino Room Scanner CLI")

    while True:
        command = input("Type 'start' to begin scan, 's' to stop, 'exit' to quit: ").strip().lower()
        if command == "start":
            scanner.start()
        elif command == "s":
            scanner.stop()
        elif command == "exit":
            scanner.stop()
            break
        else:
            scanner.log("WARN", "Unknown command.")

if __name__ == "__main__":
    main()
