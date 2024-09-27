import serial
from xbee import ZigBee
import time

# Configure the serial port
SERIAL_PORT = 'COM8'  # Change this to match your system
BAUD_RATE = 9600

# XBee Router address from the image
ROUTER_ADDRESS = b'\x00\x13\xA2\x00\x40\xF1\x5A\x9C'

try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE)
except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
    exit(1)

# Create ZigBee object with escaped=True for API mode 2
xbee = ZigBee(ser, escaped=True)


def send_message_to_router(message):
    xbee.send('tx', dest_addr_long=ROUTER_ADDRESS, dest_addr=b'\xFF\xFE', data=message.encode('utf-8'))
    print(f"Sent to Router: {message}")


def receive_data():
    try:
        frame = xbee.wait_read_frame()
        if frame['id'] == 'rx':
            source_addr = frame['source_addr_long']
            message = frame['rf_data'].decode('utf-8').strip()
            print(f"Received from Router: {message}")

            # Send a response back to the Router
            response = f"Coordinator received: {message}\n"
            send_message_to_router(response)
    except Exception as e:
        print(f"Error processing received data: {e}")


try:
    print("XBee Coordinator running in API mode 2. Waiting for messages from Router...")
    while True:
        receive_data()
        time.sleep(0.1)  # Small delay to prevent CPU hogging

except KeyboardInterrupt:
    print("\nKeyboard interrupt received. Exiting...")
except Exception as e:
    print(f"An error occurred: {e}")
finally:
    xbee.halt()
    ser.close()
    print("Serial port closed.")
