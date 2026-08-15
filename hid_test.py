import hid
import time

VID = 0x0487
PID = 0x0007

devices = hid.enumerate(VID, PID)

if not devices:
    print("Device not found")
    exit()

device_info = devices[0]

print("Device found:")
print("Manufacturer:", device_info["manufacturer_string"])
print("Product:", device_info["product_string"])
print("Serial:", device_info["serial_number"])
print("Path:", device_info["path"])

device = hid.device()

device.open_path(device_info["path"])

print("\nConnected!")

device.set_nonblocking(1)

print("Listening for HID reports...")
print("Press Ctrl+C to stop.\n")

try:
    while True:
        data = device.read(64)

        if data:
            print("RX:", " ".join(f"{x:02X}" for x in data))
            print("Length:", len(data))
            print()

        time.sleep(0.01)

except KeyboardInterrupt:
    print("\nStopping...")

finally:
    device.close()import hid
import time

VID = 0x0487
PID = 0x0007

print("Opening sensor...")

device = hid.device()
device.open(VID, PID)

print("Device opened")
print("Manufacturer:", device.get_manufacturer_string())
print("Product:", device.get_product_string())
print("Serial:", device.get_serial_number_string())

device.set_nonblocking(True)

print("\nWaiting for HID reports...")

while True:
    data = device.read(64)

    if data:
        print("RX:", " ".join(f"{x:02X}" for x in data))

    time.sleep(0.1)
