import socket
import time
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# NETWORK SETTINGS
UDP_IP = "0.0.0.0"  # Listen on all available network interfaces
UDP_PORT = 12345  # Must match the port configured in Vitis
FFT_LENGTH = 1024
PACKET_SIZE = FFT_LENGTH * 4  # 1024 words * 4 bytes = 4096 bytes

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))
sock.setblocking(False)  # Set to non-blocking mode

print("=" * 50)
print(f"[*] Server started. Listening on UDP port {UDP_PORT}...")
print(f"[*] Expected packet size: {PACKET_SIZE} bytes.")
print("=" * 50)

# PLOT CONFIGURATION
fig, ax = plt.subplots(figsize=(10, 6))
fig.canvas.manager.set_window_title('Zynq Real-Time Spectrum')
x_data = np.arange(FFT_LENGTH)
line, = ax.plot(x_data, np.zeros(FFT_LENGTH), color='cyan')

ax.set_title("Real-Time FFT Spectrum", color='white')
ax.set_facecolor('#1e1e1e')
fig.patch.set_facecolor('#121212')
ax.grid(color='gray', linestyle='--', linewidth=0.5, alpha=0.5)
ax.set_xlim(0, FFT_LENGTH)
ax.set_ylim(0, 5000)
ax.tick_params(colors='white')

# DEBUG VARIABLES
latest_magnitude = np.zeros(FFT_LENGTH)
first_packet_received = False
total_packets_received = 0
last_print_time = time.time()


def update(frame):
    global latest_magnitude, first_packet_received, total_packets_received, last_print_time

    packets_in_this_loop = 0

    try:
        # Attempt to drain ALL packets currently in the OS buffer
        while True:
            data, addr = sock.recvfrom(65535)

            # Catch the very first packet to confirm connection
            if not first_packet_received:
                print(f"\n[+] SUCCESS! Connection established! First packet received from: {addr[0]}:{addr[1]}")
                first_packet_received = True

            # Verify packet size consistency
            if len(data) == PACKET_SIZE:
                packets_in_this_loop += 1
                total_packets_received += 1

                # Parse data: extract real and imaginary parts from 32-bit words
                raw_data = np.frombuffer(data, dtype=np.uint32)
                real_part = np.int16(raw_data & 0xFFFF)
                imag_part = np.int16((raw_data >> 16) & 0xFFFF)

                # Calculate magnitude: sqrt(I^2 + Q^2)
                latest_magnitude = np.sqrt(real_part.astype(float) ** 2 + imag_part.astype(float) ** 2)
            else:
                print(f"[!] WARNING: Received packet of unexpected size: {len(data)} bytes (discarding)")

    except BlockingIOError:
        pass

    # DEBUG OUTPUT (ONCE PER SECOND)
    current_time = time.time()
    if current_time - last_print_time >= 1.0:
        if first_packet_received:
            print(
                f"[*] Status: Data flowing. Total packets: {total_packets_received} | Packet rate: {packets_in_this_loop} pkt/frame")
        else:
            print("[...] Waiting for Zynq data... (Check cable, IP addresses, and Windows Firewall)")
        last_print_time = current_time

    # Update the plot line with the latest magnitude data
    line.set_ydata(latest_magnitude)
    return line,


# Start the animation loop (approx. 30ms interval)
ani = FuncAnimation(fig, update, interval=30, blit=True, cache_frame_data=False)
plt.show()