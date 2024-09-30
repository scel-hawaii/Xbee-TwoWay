import serial
from xbee import ZigBee
import time

SERIAL_PORT = 'COM8'  # Change this to match your system
BAUD_RATE = 9600

try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE)
    print(f"Opened serial port: {SERIAL_PORT} at {BAUD_RATE} baud")
except serial.SerialException as e:
    print(f"Error opening serial port: {e}")
    exit(1)

xbee = ZigBee(ser, escaped=True)  # Set escaped=True for API mode 2
print("XBee object created")

# Dictionary to store known router addresses
known_routers = {}


def send_message_to_router(router_address, message):
    print(f"Attempting to send to Router {router_address.hex()}: {message}")
    xbee.tx(dest_addr_long=router_address, dest_addr=b'\xFF\xFE', data=message.encode('utf-8'))
    print(f"Sent to Router {router_address.hex()}: {message}")


def receive_data():
    try:
        frame = xbee.wait_read_frame(timeout=1)
        print(f"Received frame: {frame}")
        if frame['id'] == 'rx':
            source_addr = frame['source_addr_long']
            message = frame['rf_data'].decode('utf-8').strip()
            print(f"Received from Router ({source_addr.hex()}): {message}")

            # Store or update the router address
            known_routers[source_addr] = time.time()

            response = f"Coordinator received: {message}"
            send_message_to_router(source_addr, response)
        elif frame['id'] == 'tx_status':
            print(f"Transmission status: {frame['deliver_status'].hex()}")
        else:
            print(f"Received frame type: {frame['id']}")
    except Exception as e:
        pass  # Timeout or other error, just continue


try:
    print("XBee Coordinator running. Waiting for router messages...")
    last_send_time = 0
    while True:
        receive_data()
        current_time = time.time()

        # Send a message to all known routers every 15 seconds
        if current_time - last_send_time > 15:
            for router_address in list(known_routers.keys()):
                # Remove routers we haven't heard from in 5 minutes
                if current_time - known_routers[router_address] > 300:
                    del known_routers[router_address]
                    print(f"Removed inactive router: {router_address.hex()}")
                else:
                    send_message_to_router(router_address, "Hello from Coordinator!")
            last_send_time = current_time

        time.sleep(0.1)

except KeyboardInterrupt:
    print("\nKeyboard interrupt received. Exiting...")
except Exception as e:
    print(f"An error occurred: {e}")
finally:
    xbee.halt()
    ser.close()
    print("Serial port closed.")
