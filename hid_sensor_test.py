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

    crc = crc16_modbus(packet[:62])

    packet[62] = crc & 0xFF
    packet[63] = (crc >> 8) & 0xFF

    return packet


def decode_sensor(response):

    # Temperature
    temperature_raw = (response[11] << 8) | response[12]
    temperature = temperature_raw / 10.0

    # Fields 1-6
    values = {}

    position = 14

    for field_id in range(1, 7):

        value = (response[position + 4] << 8) | response[position + 5]

        values[field_id] = value

        position += 7

    # Decode remaining parameters
    humidity = values[1] / 10.0
    conductivity = values[2]
    ph = values[3] / 10.0
    nitrogen = values[4]
    phosphorus = values[5]
    potassium = values[6]

    return (
        temperature,
        humidity,
        conductivity,
        ph,
        nitrogen,
        phosphorus,
        potassium
    )


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
    print("Starting sensor monitoring...")
    print("Press Ctrl+C to stop.\n")

    while True:

        realtime_command = make_command(0x22)

        packet = bytes([0x00]) + realtime_command

        print("=" * 60)
        print("Sending real-time command...")

        written = device.write(packet)

        print("Bytes written:", written)

        response = device.read(64, timeout_ms=3000)

        if not response:
            print("NO RESPONSE")
            time.sleep(2)
            continue

        response = bytes(response)

        if len(response) < 56:
            print("Invalid response length:", len(response))
            continue

        (
            temperature,
            humidity,
            conductivity,
            ph,
            nitrogen,
            phosphorus,
            potassium
        ) = decode_sensor(response)

        print("\n============================================================")
        print("SOIL SENSOR READING")
        print("============================================================")

        print(f"Temperature    : {temperature:.1f} °C")
        print(f"Humidity       : {humidity:.1f} %")
        print(f"Conductivity   : {conductivity} µS/cm")
        print(f"pH             : {ph:.1f}")
        print(f"Nitrogen       : {nitrogen} mg/kg")
        print(f"Phosphorus     : {phosphorus} mg/kg")
        print(f"Potassium      : {potassium} mg/kg")

        print("============================================================\n")

        time.sleep(2)

except KeyboardInterrupt:

    print("\nStopping...")

finally:

    device.close()

    print("Device closed.")
