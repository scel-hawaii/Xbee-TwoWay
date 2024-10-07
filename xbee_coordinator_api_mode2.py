import serial
from xbee import ZigBee
from xbee.backend.base import TimeoutException
import time
import csv
import msvcrt
import threading
import traceback

SERIAL_PORT = 'COM8'  # Change this to match your system
BAUD_RATE = 9600
CSV_FILE = r'router_data.csv'

try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE)
    print(f"Opened serial port: {SERIAL_PORT} at {BAUD_RATE} baud")
except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
    exit(1)

xbee = ZigBee(ser, escaped=True)  # Set escaped=True for API mode 2
print("XBee object created")

router_address = None
data_collection_active = False

HEARTBEAT_INTERVAL = 30  # seconds
STATUS_UPDATE_INTERVAL = 5  # seconds
last_heartbeat_time = time.time()
last_status_update = time.time()

def send_message_to_router(message):
    if router_address:
        print(f"Sending to Router {router_address.hex()}: {message}")
        try:
            xbee.tx(dest_addr_long=router_address, data=message.encode('utf-8'))
            print("Message sent successfully")
        except Exception as e:
            print(f"Error sending message: {e}")
    else:
        print("Router address not yet known. Cannot send message.")

def receive_data():
    global router_address, data_collection_active, last_status_update
    try:
        frame = xbee.wait_read_frame(timeout=1)
        if frame['id'] == 'rx':
            source_addr = frame['source_addr_long']
            message = frame['rf_data'].decode('utf-8').strip()
            print(f"Received from Router ({source_addr.hex()}): {message}")

            if not router_address:
                router_address = source_addr
                print(f"Initial contact established with Router: {router_address.hex()}")
                send_message_to_router("Contact confirmed")
            elif message == "data" and data_collection_active:
                print("Received data message. Saving to CSV...")
                save_to_csv(message)
            elif message == "heartbeat_ack":
                print("Received heartbeat acknowledgment.")
            else:
                print(f"Received message: {message}")
        elif frame['id'] != 'rx':
            print(f"Received non-rx frame: {frame['id']}")
    except TimeoutException:
        # This is expected when no data is received
        current_time = time.time()
        if current_time - last_status_update >= STATUS_UPDATE_INTERVAL:
            print("Waiting for router contact...")
            last_status_update = current_time
    except Exception as e:
        print(f"Error in receive_data: {e}")
        print(traceback.format_exc())

def save_to_csv(data):
    try:
        with open(CSV_FILE, 'a', newline='') as file:
            writer = csv.writer(file)
            writer.writerow([time.strftime("%Y-%m-%d %H:%M:%S"), data])
        print(f"Data saved to {CSV_FILE}")
    except Exception as e:
        print(f"Error saving to CSV: {e}")

def process_command(command):
    global data_collection_active
    if command.lower() == "pause":
        data_collection_active = False
        send_message_to_router("pause")
        print("Paused data collection")
    elif command.lower() == "start":
        data_collection_active = True
        send_message_to_router("start")
        print("Started data collection")
    else:
        print("Unknown command. Available commands: 'pause', 'start'")

def send_heartbeat():
    global last_heartbeat_time
    current_time = time.time()
    if current_time - last_heartbeat_time >= HEARTBEAT_INTERVAL:
        send_message_to_router("heartbeat")
        print("Heartbeat sent")
        last_heartbeat_time = current_time

def check_for_input():
    if msvcrt.kbhit():
        return input().strip()
    return None

try:
    print("XBee Coordinator running. Waiting for initial router contact...")
    while True:
        receive_data()
        if router_address:
            break
        time.sleep(0.1)

    print("Coordinator ready. Enter 'pause' or 'start' to control data collection.")
    while True:
        receive_data()
        send_heartbeat()

        input_command = check_for_input()
        if input_command:
            process_command(input_command)

        time.sleep(0.1)  # Small delay to prevent CPU overuse

except KeyboardInterrupt:
    print("\nKeyboard interrupt received. Exiting...")
except Exception as e:
    print(f"An error occurred: {e}")
    print(traceback.format_exc())
finally:
    xbee.halt()
    ser.close()
    print("Serial port closed.")
