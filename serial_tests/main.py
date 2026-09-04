import serial
import struct
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation

# --- CONFIGURATION ---
SERIAL_PORT  = '/dev/ttyACM1'  # Update to your Picoprobe UART COM port
BAUD_RATE    = 921_600
FLOATS_COUNT = 64

PAYLOAD_SIZE = FLOATS_COUNT * 4  # 16 bytes

# Total audio window length to display (must be a power of 2 for clean FFT visualization)
BUFFER_SIZE = FLOATS_COUNT ##512 
# Estimated sample rate of your Pico loop (adjust to match your Pico's physical loop speed)
SAMPLE_RATE = 8000  

# Global audio buffer to store incoming microphone samples
audio_buffer = np.zeros(BUFFER_SIZE)

# Open serial connection
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.01)
    ser.reset_input_buffer()
except Exception as e:
    print(f"Error opening serial port: {e}")
    exit()

def calculate_checksum(raw_floats_bytes):
    return sum(raw_floats_bytes) & 0xFF

def parse_serial_data():
    """Reads all available packets from the UART queue and updates the global audio buffer."""
    global audio_buffer
    new_samples = []

    # Read as many packets as available in the serial buffer to keep up with raw audio rates
    while ser.in_waiting >= (PAYLOAD_SIZE + 3):
        b1 = ser.read(1)
        if not b1 or b1[0] != 0xAA:
            continue

        b2 = ser.read(1)
        if not b2 or b2[0] != 0xBB:
            continue


        print("Received")
        payload = ser.read(PAYLOAD_SIZE)
        if len(payload) != PAYLOAD_SIZE:
            print("bad payload")
            continue

        # checksum_byte = ser.read(1)
        # if not checksum_byte or checksum_byte != calculate_checksum(payload):
        #     continue
        
        # Unpack the 4 audio floats
        floats = struct.unpack(f'<{FLOATS_COUNT}f', payload)
        new_samples.extend(floats)
        # break

    if new_samples:
        # Shift the old data out and append the new incoming audio blocks
        # audio_buffer[:] = new_samples
        audio_buffer = np.roll(audio_buffer, -len(new_samples))
        audio_buffer[-len(new_samples):] = new_samples

# --- PLOT SETUP (2 Subplots: Waveform & FFT) ---
fig, ax1 = plt.subplots(1, 1, figsize=(10, 6))
fig.suptitle("Real-Time I2S Microphone Audio Monitor", fontsize=14)

# 1. Waveform Plot
x_time = np.arange(BUFFER_SIZE)
line_wave, = ax1.plot(x_time, audio_buffer, color='dodgerblue')
ax1.set_title("Audio Oscilloscope Waveform")
ax1.set_xlim(0, BUFFER_SIZE)
ax1.set_ylim(-1.1, 1.1)  # Locked to fixed microphone scale bounds
ax1.set_ylabel("Amplitude")
ax1.grid(True)

# 2. FFT Spectrum Plot
# xf = np.fft.rfftfreq(BUFFER_SIZE, d=1/SAMPLE_RATE)
# line_fft, = ax2.plot(xf, np.zeros(len(xf)), color='crimson')
# ax2.set_title("Frequency Spectrum (FFT)")
# ax2.set_xlim(0, SAMPLE_RATE / 2)
# ax2.set_ylim(0, 20)  # Initial magnitude limit, will scale or can be tweaked
# ax2.set_xlabel("Frequency (Hz)")
# ax2.set_ylabel("Magnitude")
# ax2.grid(True)

plt.tight_layout()

def update_plot(frame):
    """Callback function executed by Matplotlib to refresh audio frames."""
    # Step 1: Drain serial data queue and refresh global buffer
    parse_serial_data()
    
    # Step 2: Update the raw time-domain line data
    line_wave.set_ydata(audio_buffer)
    
    # # Step 3: Compute fast fourier transform for frequency visibility
    # fft_data = np.abs(np.fft.rfft(audio_buffer))
    # line_fft.set_ydata(fft_data)
    
    # # Dynamically scale FFT peak height so it doesn't clip visually
    # max_fft = np.max(fft_data)
    # if max_fft > ax2.get_ylim()[1]:
    #     ax2.set_ylim(0, max_fft * 1.2)
        
    return [line_wave]#, line_fft

def main():
    print(f"Streaming audio visualizations from {SERIAL_PORT}. Close window to stop.")
    
    # Run the visualization framework loop
    ani = animation.FuncAnimation(fig, update_plot, interval=500, blit=True, cache_frame_data=False)
    plt.show()
    
    ser.close()
    print("Serial port closed safely.")

if __name__ == "__main__":
    main()
