import hid
import time

VID = 0x0487
PID = 0x0007


def crc16_modbus(data):
    crc = 0xFFFF

    for byte in data:
        crc ^= byte

        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1

    return crc


def make_command(command_byte):
    packet = bytearray(64)

    packet[0] = 0x55
    packet[1] = 0x01
    packet[2] = command_byte

    # CRC over first 62 bytes
    crc = crc16_modbus(packet[:62])

    packet[62] = crc & 0xFF
    packet[63] = (crc >> 8) & 0xFF

    return packet


def print_hex(data):
    print(" ".join(f"{b:02X}" for b in data))


def print_indexed(data):
    print("\nBYTE POSITIONS:")

    for i in range(0, len(data), 8):
        row = []

        for j in range(i, min(i + 8, len(data))):
            row.append(f"[{j:02d}]={data[j]:02X}")

        print("  ".join(row))


print("Searching for sensor...")

devices = hid.enumerate(VID, PID)

if not devices:
    print("Sensor not found.")
    exit()

info = devices[0]

print("Device found:")
print("Manufacturer:", info["manufacturer_string"])
print("Product:", info["product_string"])
print("Serial:", info["serial_number"])
print("Path:", info["path"])

device = hid.device()

try:
    device.open_path(info["path"])

    print("\nConnected!")

    # --------------------------------------------------
    # DEVICE INFO INITIALIZATION
    # --------------------------------------------------

    print("\n" + "=" * 70)
    print("DEVICE INFO INITIALIZATION")
    print("=" * 70)

    device_info_command = make_command(0x00)

    print("\nDevice Info Command")
    print("Length:", len(device_info_command))
    print("HEX:")
    print_hex(device_info_command)

    print("\nSending device-info command...")

    # Report ID 0 + 64-byte command
    packet = bytes([0x00]) + bytes(device_info_command)

    written = device.write(packet)

    print("Bytes written:", written)

    print("Waiting for device-info response...")

    response = device.read(64, timeout_ms=3000)

    if response:

        response = bytes(response)

        print("\nDEVICE INFO RESPONSE")
        print("Length:", len(response))

        print("\nHEX:")
        print_hex(response)

        print_indexed(response)

    else:
        print("\nNO DEVICE INFO RESPONSE")
        exit()

    time.sleep(0.5)

    # --------------------------------------------------
    # REAL-TIME MONITORING
    # --------------------------------------------------

    print("\n" + "=" * 70)
    print("REAL-TIME MONITORING")
    print("=" * 70)

    print("Press Ctrl+C to stop.\n")

    realtime_command = make_command(0x22)

    while True:

        print("=" * 70)
        print("Sending real-time command...")

        print("Command length:", len(realtime_command))

        packet = bytes([0x00]) + bytes(realtime_command)

        written = device.write(packet)

        print("Bytes written:", written)

        print("Waiting for response...")

        response = device.read(64, timeout_ms=3000)

        if not response:
            print("NO RESPONSE")
            time.sleep(2)
            continue

        response = bytes(response)

        print("\nREAL-TIME RESPONSE")
        print("Length:", len(response))

        print("\nHEX:")
        print_hex(response)

        print_indexed(response)

        time.sleep(2)


except KeyboardInterrupt:

    print("\nStopping...")

finally:

    device.close()

    print("\nDevice closed.")
