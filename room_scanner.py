import requests
import time
import threading
import json

BASE_URL = "http://192.168.100.236"
MOVE_COMMANDS = {
    "forward": "/forward",
    "left": "/left",
    "right": "/right",
    "back": "/back"
}
SCAN_PATH = "/"
MIN_DISTANCE_CM = 100  # minimum distance to consider safe to move
STEP_DISTANCE_M = 1  # 1m per move

class Scanner:
    def __init__(self):
        self.running = False
        self.area_covered = 1  # starts at 1m²
        self.visited = set()

    def get_scan_data(self):
        try:
            print("[INFO] Sending scan request...")
            response = requests.get(BASE_URL + SCAN_PATH, timeout=15)
            response.raise_for_status()
            print(f"[DEBUG] Raw scan response: {response.text}")
            data = response.json()
            return data.get("distance_measurements", [])
        except Exception as e:
            print(f"[ERROR] {e}")
            return []

    def move(self, direction):
        print(f"[MOVE] Attempting to move {direction}")
        try:
            response = requests.get(BASE_URL + MOVE_COMMANDS[direction], timeout=17)
            time.sleep(1)
        except Exception as e:
            print(f"[ERROR] Move failed: {e}")

    def run_scan(self):
        print("[INFO] Starting room scan...")

        directions_map = {
            10: "right",
            40: "right",
            70: "forward",
            100: "forward",
            130: "left",
            160: "left"
        }

        while self.running:
            scan = self.get_scan_data()

            best_move = None
            max_distance = -1

            for point in scan:
                angle = point.get("angle")
                dist = point.get("distance")
                direction = directions_map.get(angle)

                if direction and dist >= MIN_DISTANCE_CM and dist > max_distance:
                    best_move = direction
                    max_distance = dist

            if not best_move:
                print("[INFO] No clear direction. Stopping scan.")
                break

            self.move(best_move)
            print(f"[MOVE] Moving {best_move}, distance: {max_distance} cm")
            self.area_covered += STEP_DISTANCE_M

        print("\n[END] Scan complete.")
        print(f"[RESULT] Estimated square area: {self.area_covered} m²")

    def start(self):
        if not self.running:
            self.running = True
            self.run_scan()

    def stop(self):
        self.running = False
        print("[ABORT] Stopping scan...")

def main():
    scanner = Scanner()
    print("🚗 Arduino Room Scanner CLI")

    while True:
        command = input("Type 'start' to begin scan, 's' to stop, 'exit' to quit: ").strip().lower()

        if command == "start":
            scanner = Scanner()
            scanner_thread = threading.Thread(target=scanner.start)
            scanner_thread.start()
        elif command == "s":
            scanner.stop()
        elif command == "exit":
            scanner.stop()
            break
        else:
            print("[WARN] Unknown command.")

if __name__ == "__main__":
    main()
