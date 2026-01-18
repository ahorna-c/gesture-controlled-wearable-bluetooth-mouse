import pyautogui
import time
import asyncio
from bleak import BleakClient, BleakScanner

# Configuration Parameters for Cursor Control
SENSITIVITY = 5
DEAD_ZONE_DEGREES = 3.0 # Angle below which no tilt is registered (to avoid noise)
MAX_CURSOR_SPEED = 50
SCROLL_SPEED = 5 # Adjust this value as needed for scrolling sensitivity

# BLE Configuration
SERVICE_UUID = "55925ea2-e795-4816-b8d5-e0dcf7c09b15"
CHARACTERISTIC_UUID = "2823aff6-4924-4f5a-8fe9-fbebe5a97f93"
DEVICE_NAME = "NanoESP32-BLE" # Matches the name in ESP32's BLEDevice::init()

# Global variable to store the latest sensor data received via BLE notification
latest_sensor_data = {
    "pitch": 0.0,
    "roll": 0.0,
    "button_mask": 0,       # Stores the combined button states as a bitmask
    "is_scrolling_mode": False # Stores the mode flag from ESP32
}
data_updated_event = asyncio.Event() # Event to signal when new data has been received

# Global state variables for printing
prev_button_mask = 0
last_printed_tilt_direction = "NONE" # "NONE", "LEFT", "RIGHT", "UP", "DOWN"
last_printed_mode = False # Stores the last printed mode (False for Cursor, True for Scrolling)

# BLE Notification Callback Function
def notification_handler(sender, data: bytearray):
    """
    Handles incoming BLE notifications. Parses the byte array data into
    pitch, roll, button_mask, and is_scrolling_mode, then updates the global variable and signals the main loop.
    """
    global latest_sensor_data
    try:
        received_string = data.decode('utf-8').strip()
        parts = received_string.split(',')
       
        if len(parts) == 4: # Now expecting 4 parts
            pitch = float(parts[0])
            roll = float(parts[1])
            button_mask = int(parts[2])      # Parse button mask
            is_scrolling_mode = bool(int(parts[3])) # Parse mode flag (0 or 1 to bool)

            latest_sensor_data["pitch"] = pitch
            latest_sensor_data["roll"] = roll
            latest_sensor_data["button_mask"] = button_mask
            latest_sensor_data["is_scrolling_mode"] = is_scrolling_mode
           
            data_updated_event.set() # Signal that new data is available
        else:
            print(f"Received malformed data: '{received_string}' (expected 4 parts, got {len(parts)})")
    except (ValueError, IndexError) as e:
        print(f"Error parsing received data '{data.decode('utf-8').strip()}': {e}")
    except Exception as e:
        print(f"Unexpected error in notification_handler: {e}")

# Mapping Function for Tilt to Movement
def map_tilt_to_movement(angle, sensitivity, dead_zone, max_speed):
    """
    Calculates the cursor/scroll movement (in pixels/units) based on a given tilt angle.
    Applies dead zone and maximum speed limits.
    """
    if abs(angle) < dead_zone:
        return 0
    movement = angle * sensitivity
    if movement > max_speed:
        movement = max_speed
    elif movement < -max_speed:
        movement = -max_speed
    return movement

# Main BLE Connection and Cursor Control Logic
async def run_ble_cursor_control():
    global prev_button_mask, last_printed_tilt_direction, last_printed_mode

    print(f"Searching for BLE device '{DEVICE_NAME}' with Service UUID '{SERVICE_UUID}'...")
    device = None
   
    while device is None:
        devices = await BleakScanner.discover(timeout=5.0)
        for d in devices:
            if d.name == DEVICE_NAME:
                print(f"Found device by name: {d.name} ({d.address})")
                device = d
                break
            elif d.metadata.get('uuids') and SERVICE_UUID in d.metadata['uuids']:
                print(f"Found device by service UUID: {d.name} ({d.address})")
                device = d
                break
        if device is None:
            print("Device not found. Retrying scan...")
            await asyncio.sleep(1)

    async with BleakClient(device.address) as client:
        if not client.is_connected:
            print("Failed to connect to device.")
            return

        print(f"Connected to {device.name} ({device.address})")
       
        print(f"Subscribing to notifications for characteristic {CHARACTERISTIC_UUID}...")
        try:
            await client.start_notify(CHARACTERISTIC_UUID, notification_handler)
            print("Successfully subscribed to notifications. Control active.")
        except Exception as e:
            print(f"Failed to subscribe to notifications: {e}")
            return

        print("\nTilt the glove. Press Ctrl+C to stop.")
        print(f"Config: Dead zone = +/- {DEAD_ZONE_DEGREES}° | Sensitivity = {SENSITIVITY} px/° | Max Speed = {MAX_CURSOR_SPEED} px/step")
        print("Button 1 on ESP32 toggles between CURSOR and SCROLLING modes.\n")
        print("Current Mode: CURSOR") # Initial mode display

        operate = True

        try:
            # Main loop for cursor/scroll control
            while True:
                # Wait until the notification_handler signals that new data has arrived.
                await data_updated_event.wait()
                data_updated_event.clear() # Reset the event for the next data update

                # Get the latest parsed sensor data from the global variable
                current_pitch = latest_sensor_data["pitch"]
                current_roll = latest_sensor_data["roll"]
                current_button_mask = latest_sensor_data["button_mask"]
                if operate:
                    current_is_scrolling_mode = latest_sensor_data["is_scrolling_mode"]

                # Interpret and Print Mode
                if (current_is_scrolling_mode != last_printed_mode):
                    print(f"MODE: {'NOW SCROLLING' if current_is_scrolling_mode else 'CURSOR'}")
                    last_printed_mode = current_is_scrolling_mode

                # Interpret and Print Button Presses
                for i in range(5): # Check for up to 5 buttons
                    # Check if the current button (bit i) is pressed AND it was not pressed in the previous mask
                    if (current_button_mask & (1 << i)) and not (prev_button_mask & (1 << i)):
                        if operate:
                            if i == 0: # Button 1
                                print("--> Button 1 PRESSED (Mode Toggle). No click action in Python.")
                                # No pyautogui action here for Button 1, as requested.
                            elif i == 1: # Button 2
                                pyautogui.rightClick() # Assigned to Right Click
                                print("--> Button 2 PRESSED (Normal Click)!")
                            elif i == 2: # Button 3
                                pyautogui.click() # Assigned to Normal (Left) Click
                                print("--> Button 3 PRESSED (Right Click)!")
                            elif i == 3: # Button 4
                                pyautogui.press('volumemute')
                        if i == 4: # Button 5
                            operate = not operate

                # Update the previous button mask for the next iteration
                prev_button_mask = current_button_mask

                # Interpret and Print Tilt Direction
                current_tilt_direction = "NONE"

                # Roll (physical left/right tilt) now controls UP/DOWN.
                # Pitch (physical forward/backward tilt) now controls LEFT/RIGHT.
                if operate:
                    # Check for roll (vertical movement/scroll)
                    if current_roll > (DEAD_ZONE_DEGREES + 5): # Positive roll means physical right tilt
                        current_tilt_direction = "DOWN" # This maps to logical DOWN for screen content
                    elif current_roll < -(DEAD_ZONE_DEGREES + 5): # Negative roll means physical left tilt
                        current_tilt_direction = "UP"   # This maps to logical UP for screen content
                    # If no significant roll, check for pitch (horizontal movement/scroll)
                    elif current_pitch > (DEAD_ZONE_DEGREES + 5): # Positive pitch means physical forward/down tilt
                        current_tilt_direction = "RIGHT" # This maps to logical RIGHT for screen content
                    elif current_pitch < -(DEAD_ZONE_DEGREES + 5): # Negative pitch means physical backward/up tilt
                        current_tilt_direction = "LEFT"  # This maps to logical LEFT for screen content

                    # Only print if the detected tilt direction has changed
                    if current_tilt_direction != last_printed_tilt_direction:
                        if current_tilt_direction != "NONE":
                            print(f"Tilting {current_tilt_direction}")
                        else:
                            print("Tilt released (no significant tilt)")
                        last_printed_tilt_direction = current_tilt_direction


                    # Apply Cursor/Scroll Movement
                    if not current_is_scrolling_mode: # Cursor Mode
                        # dx (horizontal movement) now comes from PITCH
                        # dy (vertical movement) now comes from ROLL
                        dx = map_tilt_to_movement(current_pitch, SENSITIVITY, DEAD_ZONE_DEGREES, MAX_CURSOR_SPEED)
                        dy = map_tilt_to_movement(current_roll, SENSITIVITY, DEAD_ZONE_DEGREES, MAX_CURSOR_SPEED)
                        if dx != 0 or dy != 0:
                            pyautogui.moveRel(dx, dy, duration=0)
                    else: # Scrolling Mode
                        # scroll_y (vertical scroll) comes from ROLL
                        scroll_y = map_tilt_to_movement(current_roll, SCROLL_SPEED, DEAD_ZONE_DEGREES, MAX_CURSOR_SPEED)
                    
                        if scroll_y != 0:
                            # If current_roll < 0 (physical "up" tilt) -> scroll_y negative.
                            # We want it to scroll UP, so need a positive amount. -> -negative = positive.
                            # If current_roll > 0 (physical "down" tilt) -> scroll_y positive.
                            # We want it to scroll DOWN, so need a negative amount. -> -positive = negative.
                            pyautogui.scroll(-int(round(scroll_y)))
                        # Horizontal scrolling is disabled for now.
                        # if scroll_x != 0:
                        #     pyautogui.hscroll(scroll_x)
                
                # Small sleep to yield CPU, even though updates are event-driven
                await asyncio.sleep(0.001)

        except KeyboardInterrupt:
            print("\nControl stopped by user (Ctrl+C detected).")
        except Exception as e:
            print(f"An unexpected error occurred: {e}")
        finally:
            print("Stopping BLE notifications and disconnecting...")
            try:
                await client.stop_notify(CHARACTERISTIC_UUID)
            except Exception as e:
                print(f"Error stopping notifications: {e}")

# Script Execution
if __name__ == "__main__":
    print("Starting BLE gesture control in 3 seconds. Switch to your target application now...")
    time.sleep(3)
   
    asyncio.run(run_ble_cursor_control())

