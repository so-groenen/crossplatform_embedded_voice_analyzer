#!/usr/bin/env python3

import serial
import numpy as np
import matplotlib.pyplot as plt
from scipy import signal
from collections import deque
import time

# ------------------------------------------------------------
# Configuration
# ------------------------------------------------------------

PORT = "/dev/ttyACM1"
BAUDRATE = 921600

SAMPLE_RATE = 16_000

PACKET_SAMPLES = 64
PACKET_SIZE = 2 + PACKET_SAMPLES * 4

HEADER = b"\xAA\xBB"

# Time-domain display
PLOT_SAMPLES = 1600       # 100 ms @ 16 kHz

# FFT size
FFT_SAMPLES = 2048        # 128 ms @ 16 kHz

# GUI refresh rate
PLOT_RATE = 30            # Hz

RESOLUTION = SAMPLE_RATE / FFT_SAMPLES
MIN_FREQ = 50
MAX_FREQ = 1000
MIN_INDX = int(MIN_FREQ/RESOLUTION)
MAX_INDX = int(MAX_FREQ/RESOLUTION)

# ------------------------------------------------------------
# Serial
# ------------------------------------------------------------

ser = serial.Serial(
    PORT,
    BAUDRATE,
    timeout=0.01,
)

rx_buffer = bytearray()

audio = deque(
    np.zeros(PLOT_SAMPLES, dtype=np.float32),
    maxlen=PLOT_SAMPLES
)


# ------------------------------------------------------------
# FFT
# ------------------------------------------------------------

# Separate buffer for FFT
fft_buffer = deque(
    np.zeros(FFT_SAMPLES, dtype=np.float32),
    maxlen=FFT_SAMPLES
)

# Hann window
window = signal.windows.hann(FFT_SAMPLES)
# Frequency axis
freq = np.fft.rfftfreq(
    FFT_SAMPLES,
    d=1.0 / SAMPLE_RATE
)


# ------------------------------------------------------------
# Matplotlib
# ------------------------------------------------------------

plt.ion()

fig, (ax_time, ax_fft) = plt.subplots(
    2,
    1,
    figsize=(10, 7)
)

# ------------------------------------------------------------
# Time domain
# ------------------------------------------------------------

time_axis = np.arange(PLOT_SAMPLES) / SAMPLE_RATE

time_line, = ax_time.plot(
    time_axis,
    np.zeros(PLOT_SAMPLES)
)

ax_time.set_xlim(
    0,
    PLOT_SAMPLES / SAMPLE_RATE
)

ax_time.set_ylim(
    -1.05,
    1.05
)

ax_time.set_xlabel("Time [s]")
ax_time.set_ylabel("Amplitude")

ax_time.set_title("Microphone waveform")

ax_time.grid(True)


# ------------------------------------------------------------
# FFT
# ------------------------------------------------------------

fft_line, = ax_fft.plot(
    freq,
    np.zeros(len(freq))
)

ax_fft.set_xlim(
    0,
    1000 #SAMPLE_RATE / 2
)

ax_fft.set_ylim(
    -100,
    0
)

ax_fft.set_xlabel("Frequency [Hz]")
ax_fft.set_ylabel("Magnitude [dB]")

ax_fft.set_title(
    f"FFT ({FFT_SAMPLES} samples, "
    f"{SAMPLE_RATE / FFT_SAMPLES:.2f} Hz/bin)"
)

ax_fft.grid(True)


fig.tight_layout()
fig.show()


# ------------------------------------------------------------
# Main loop
# ------------------------------------------------------------

next_plot = time.monotonic()

try:

    while plt.fignum_exists(fig.number):

        # ----------------------------------------------------
        # Read available serial data
        # ----------------------------------------------------

        waiting = ser.in_waiting

        if waiting:
            rx_buffer.extend(
                ser.read(waiting)
            )

        # ----------------------------------------------------
        # Extract complete packets
        # ----------------------------------------------------

        while True:

            header_pos = rx_buffer.find(HEADER)

            if header_pos < 0:

                # Keep possible first header byte
                if len(rx_buffer) > 1:
                    rx_buffer[:] = rx_buffer[-1:]

                break

            # Remove garbage before header
            if header_pos:
                del rx_buffer[:header_pos]

            # Wait for complete packet
            if len(rx_buffer) < PACKET_SIZE:
                break

            # ------------------------------------------------
            # Extract packet
            # ------------------------------------------------

            payload = rx_buffer[
                2 : PACKET_SIZE
            ]

            packet = np.frombuffer(
                payload,
                dtype="<f4"
            )

            # Remove packet from RX buffer
            del rx_buffer[:PACKET_SIZE]

            # ------------------------------------------------
            # Store samples
            # ------------------------------------------------

            audio.extend(packet)
            fft_buffer.extend(packet)

        # ----------------------------------------------------
        # Update plots at fixed rate
        # ----------------------------------------------------

        now = time.monotonic()

        if now >= next_plot:

            # ================================================
            # Time domain
            # ================================================

            time_data = np.asarray(audio)

            time_line.set_ydata(
                time_data
            )

            # ================================================
            # FFT
            # ================================================

            fft_data = np.asarray(
                fft_buffer
            )

            # Apply Hann window
            windowed = fft_data #* window

            # Real FFT
            spectrum = np.fft.rfft(
                windowed
            )

            # Magnitude
            magnitude = np.abs(spectrum)
            peak_index = np.argmax(magnitude[MIN_INDX:MAX_INDX]) + 1
            peak_frequency = freq[peak_index]

            print(f"Peak: {peak_frequency:.1f} Hz")

            # Normalize approximately to signal amplitude
            magnitude /= np.sum(window) / 2

            # Avoid log(0)
            magnitude = np.maximum(
                magnitude,
                1e-12
            )

            # Convert to dB
            magnitude_db = 20 * np.log10(
                magnitude
            )


            fft_line.set_ydata(
                magnitude_db
            )

            # ================================================
            # Redraw
            # ================================================

            fig.canvas.draw_idle()
            fig.canvas.flush_events()

            next_plot = (
                now + 1.0 / PLOT_RATE
            )

        # Give CPU some breathing room
        time.sleep(0.001)


except KeyboardInterrupt:

    print("\nStopping...")


finally:

    ser.close()
    plt.close(fig)