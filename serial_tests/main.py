import struct
import sys
from collections import deque
import matplotlib.pyplot as plt
import numpy as np
import serial
from matplotlib.animation import FuncAnimation
from scipy.interpolate import interp1d

# Configuration
PORT          = "/dev/ttyACM1"
BAUDRATE      = 921_600
SAMPLE_RATE   = 20_000
FFT_SIZE      = 1024
MAG_SIZE      = FFT_SIZE // 2

# Scrolling display buffer width (e.g., 240 pixels for SPI LCD frame buffers)
DISPLAY_WIDTH = 240  

# Frequency & Amplitude Constraints
MIN_FREQ = 50
MAX_FREQ = 450
MAX_AMP  = 0.01

# Dynamic index mapping
full_freq_axis   = np.fft.rfftfreq(FFT_SIZE, d=1.0 / SAMPLE_RATE)
K_MIN            = np.searchsorted(full_freq_axis, MIN_FREQ)
K_MAX            = np.searchsorted(full_freq_axis, MAX_FREQ)
freq_axis_sliced = full_freq_axis[K_MIN:K_MAX]

INTERP_SAMPLES   = 100  # Vertical resolution for spectrogram
freq_axis_interp = np.linspace(freq_axis_sliced[0], freq_axis_sliced[-1], INTERP_SAMPLES)

# Updated Header Definitions
HEADER_PEAK  = b"\xab\xbc\xcd\xde"  # Single float peak frequency packet
HEADER_MAG   = b"\xff\xee\xdd\xcc"  # Full FFT magnitude packet
HEADER_BYTES = 4

SIZE_PEAK = 4
SIZE_MAG  = MAG_SIZE * 4


class UARTAudioParser:
    def __init__(self, port, baudrate):
        self.ser = serial.Serial(port, baudrate, timeout=1)
        self.mag_data = np.zeros(MAG_SIZE, dtype=np.float32)
        self.peak_freq = None
        self.active_mode = None  # Auto-detected runtime mode ("peak" or "spectrogram")

    def read_exact(self, nbytes):
        buf = bytearray()
        while len(buf) < nbytes:
            chunk = self.ser.read(nbytes - len(buf))
            if not chunk:
                return None
            buf.extend(chunk)
        return bytes(buf)

    def update_data(self):
        while self.ser.in_waiting >= HEADER_BYTES:
            header = self.ser.read(HEADER_BYTES)
            
            if header == HEADER_PEAK:
                raw_bytes = self.read_exact(SIZE_PEAK)
                if raw_bytes:
                    self.peak_freq = struct.unpack("<f", raw_bytes)[0]
                    self.active_mode = "peak"
                    
            elif header == HEADER_MAG:
                raw_bytes = self.read_exact(SIZE_MAG)
                if raw_bytes:
                    self.mag_data = np.array(
                        struct.unpack(f"<{MAG_SIZE}f", raw_bytes),
                        dtype=np.float32,
                    )
                    self.active_mode = "spectrogram"


try:
    parser = UARTAudioParser(PORT, BAUDRATE)
except serial.SerialException as e:
    print(f"Error opening serial port {PORT}: {e}")
    sys.exit(1)

# Single Plot Layout setup
fig, ax = plt.subplots(figsize=(10, 5))
fig.suptitle("RP2350 Display Visualizer Prototype", fontsize=12)

# State buffers for rendering
current_rendered_mode = None
display_buffer = np.zeros((INTERP_SAMPLES, DISPLAY_WIDTH), dtype=np.float32)
peak_history = deque([MIN_FREQ] * DISPLAY_WIDTH, maxlen=DISPLAY_WIDTH)

img_artist = None
line_artist = None


def setup_view_mode(mode):
    global img_artist, line_artist, current_rendered_mode
    ax.clear()
    
    if mode == "spectrogram":
        ax.set_title("Scrolling Spectrogram (Magnitude)")
        ax.set_xlabel("Time History (Frames)")
        ax.set_ylabel("Frequency (Hz)")
        img_artist = ax.imshow(
            display_buffer,
            aspect="auto",
            origin="lower",
            extent=[0, DISPLAY_WIDTH, MIN_FREQ, MAX_FREQ],
            cmap="inferno",
            vmin=0,
            vmax=MAX_AMP,
        )
        line_artist = None

    elif mode == "peak":
        ax.set_title("Horizontal Scrolling Peak Frequency Tracker")
        ax.set_xlabel("Time History (Frames)")
        ax.set_ylabel("Frequency (Hz)")
        ax.set_xlim(0, DISPLAY_WIDTH)
        ax.set_ylim(MIN_FREQ, MAX_FREQ)
        ax.grid(True, linestyle="--", alpha=0.6)
        (line_artist,) = ax.plot(
            np.arange(DISPLAY_WIDTH), list(peak_history), color="#ff7f0e", lw=2
        )
        img_artist = None

    current_rendered_mode = mode
    plt.draw()


def animate(frame):
    parser.update_data()
    
    # Wait until serial receives first valid header
    if parser.active_mode is None:
        return []

    # Re-configure plot dynamically if stream mode changes
    if parser.active_mode != current_rendered_mode:
        setup_view_mode(parser.active_mode)

    if parser.active_mode == "spectrogram":
        mag_sliced = parser.mag_data[K_MIN:K_MAX]

        try:
            quad_interp = interp1d(
                freq_axis_sliced,
                mag_sliced,
                kind="quadratic",
                fill_value="extrapolate",
            )
            mag_interp = quad_interp(freq_axis_interp)
            mag_interp = np.clip(mag_interp, 0, None)
        except Exception:
            mag_interp = np.zeros_like(freq_axis_interp)

        # Shift frame buffer left and append new spectral column to right
        display_buffer[:, :-1] = display_buffer[:, 1:]
        display_buffer[:, -1] = mag_interp

        if img_artist:
            img_artist.set_data(display_buffer)
            return (img_artist,)

    elif parser.active_mode == "peak":
        current_peak = parser.peak_freq if parser.peak_freq is not None else MIN_FREQ
        peak_history.append(current_peak)

        if line_artist:
            line_artist.set_ydata(list(peak_history))
            return (line_artist,)

    return []


ani = FuncAnimation(
    fig, animate, interval=30, blit=False, cache_frame_data=False
)

plt.show()
parser.ser.close()