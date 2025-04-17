import serial
import time

# Adjust COM port for your board (Windows: 'COMx', Linux: '/dev/ttyUSB0')
PORT = 'COM3'    # change this to your actual COM port
BAUD = 9600

ser = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(2)  # give STM32 time to reset

def calc_checksum(data):
    cs = sum(data)
    return [(cs >> 8) & 0xFF, cs & 0xFF]

def send_packet(packet):
    ser.write(bytearray(packet))
    print(f"Sent: {[hex(b) for b in packet]}")

def read_response(expected_len):
    resp = ser.read(expected_len)
    print(f"Received: {[hex(b) for b in resp]}")
    return resp

# === Write Test ===
# Write 3 bytes (0xDE, 0xAD, 0xBE) to address 0x10
data = [0xAA, 0x01, 0x10, 0x03, 0xDE, 0xAD, 0xBE]
checksum = calc_checksum(data)
packet = data + checksum
send_packet(packet)
ack = ser.read(1)
print(f"ACK? {ack.hex() if ack else 'No response'}")

time.sleep(0.5)

# === Read Test ===
# Request 3 bytes from address 0x10
read_cmd = [0xAA, 0x02, 0x10, 0x03]
read_checksum = calc_checksum(read_cmd)
read_packet = read_cmd + read_checksum
send_packet(read_packet)
read_response(5)  # 3 data + 2 checksum

ser.close()
