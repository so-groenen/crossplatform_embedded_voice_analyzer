import struct
import sys
import matplotlib.pyplot as plt
import numpy as np
import serial
from matplotlib.animation import FuncAnimation
from scipy.interpolate import interp1d

# Configuration
PORT        = "/dev/ttyACM1"
BAUDRATE    = 921_600
SAMPLE_RATE = 20_000
FFT_SIZE    = 1024  # Works dynamically for 1024, 2048, 4096, etc.
MAG_SIZE    = FFT_SIZE // 2

# Frequency & Amplitude Constraints
MIN_FREQ = 50
MAX_FREQ = 450
MAX_AMP = 0.01

# Dynamic index mapping using searchsorted
full_freq_axis = np.fft.rfftfreq(FFT_SIZE, d=1.0 / SAMPLE_RATE)
K_MIN = np.searchsorted(full_freq_axis, MIN_FREQ)
K_MAX = np.searchsorted(full_freq_axis, MAX_FREQ)

freq_axis_sliced = full_freq_axis[K_MIN:K_MAX]

# Match exact bounds of the sliced bins to avoid out-of-bounds interpolation errors
INTERP_SAMPLES = 500
freq_axis_interp = np.linspace(
    freq_axis_sliced[0], freq_axis_sliced[-1], INTERP_SAMPLES
)

# Packets definition
HEADER_I2S = b"\xaa\xbb\xcc\xdd"
HEADER_MAG = b"\xab\xbc\xcd\xde"
HEADER_BYTES = 4
 
SIZE_I2S = FFT_SIZE * 4
SIZE_MAG = MAG_SIZE * 4


class UARTAudioParser:

    def __init__(self, port, baudrate):
        self.ser = serial.Serial(port, baudrate, timeout=1)
        self.i2s_data = np.zeros(FFT_SIZE, dtype=np.float32)
        self.mag_data = np.zeros(MAG_SIZE, dtype=np.float32)

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
            if header == HEADER_I2S:
                raw_bytes = self.read_exact(SIZE_I2S)
                if raw_bytes:
                    self.i2s_data = np.array(
                        struct.unpack(f"<{FFT_SIZE}f", raw_bytes),
                        dtype=np.float32,
                    )
            elif header == HEADER_MAG:
                raw_bytes = self.read_exact(SIZE_MAG)
                if raw_bytes:
                    self.mag_data = np.array(
                        struct.unpack(f"<{MAG_SIZE}f", raw_bytes),
                        dtype=np.float32,
                    )


try:
    parser = UARTAudioParser(PORT, BAUDRATE)
except serial.SerialException as e:
    print(f"Error opening serial port {PORT}: {e}")
    sys.exit(1)

time_axis = np.linspace(0, (FFT_SIZE / SAMPLE_RATE) * 1000, FFT_SIZE)

fig, (ax_time, ax_freq) = plt.subplots(2, 1, figsize=(10, 6))
fig.suptitle("RP2350 Real-Time Audio & Bandpass FFT", fontsize=12)

# Time Domain Plot
(line_time,) = ax_time.plot(time_axis, parser.i2s_data, color="#1f77b4")
ax_time.set_title("Time Domain (I2S Signal)")
ax_time.set_xlabel("Time (ms)")
ax_time.set_ylabel("Amplitude")
ax_time.set_ylim(-1.1, 1.1)
ax_time.grid(True, linestyle="--", alpha=0.6)

# Frequency Domain Plot
(line_freq_raw,) = ax_freq.plot(
    freq_axis_sliced,
    np.zeros_like(freq_axis_sliced),
    "o",
    color="#1f77b4",
    markersize=3,
    alpha=0.5,
    label="Raw Bins",
)
(line_freq_interp,) = ax_freq.plot(
    freq_axis_interp,
    np.zeros_like(freq_axis_interp),
    color="#ff7f0e",
    label="Quadratic Interp",
)

ax_freq.set_title(f"Frequency Domain ({MIN_FREQ} Hz - {MAX_FREQ} Hz)")
ax_freq.set_xlabel("Frequency (Hz)")
ax_freq.set_ylabel("Magnitude")
ax_freq.set_xlim(MIN_FREQ, MAX_FREQ)
ax_freq.set_ylim(0, MAX_AMP)
ax_freq.grid(True, linestyle="--", alpha=0.6)

peak_vline = ax_freq.axvline(
    MIN_FREQ, color="red", linestyle="--", alpha=0.8, label="Peak Freq"
)
peak_text = ax_freq.text(
    0.98,
    0.90,
    "",
    transform=ax_freq.transAxes,
    ha="right",
    va="top",
    fontsize=10,
    fontweight="bold",
    bbox=dict(
        boxstyle="round,pad=0.3", facecolor="white", edgecolor="red", alpha=0.8
    ),
)
ax_freq.legend(loc="upper left")

plt.tight_layout()


def animate(frame):
    parser.update_data()

    line_time.set_ydata(parser.i2s_data)

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

    line_freq_raw.set_ydata(mag_sliced)
    line_freq_interp.set_ydata(mag_interp)

    peak_idx = np.argmax(mag_interp)
    peak_freq = freq_axis_interp[peak_idx]
    max_mag = mag_interp[peak_idx]

    peak_vline.set_xdata([peak_freq, peak_freq])
    peak_text.set_text(f"Peak: {peak_freq:.1f} Hz\nMag: {max_mag:.4f}")

    current_ylim = ax_freq.get_ylim()[1]
    if max_mag > current_ylim:
        ax_freq.set_ylim(0, max_mag * 1.2)
    elif max_mag <= MAX_AMP and current_ylim > MAX_AMP:
        ax_freq.set_ylim(0, MAX_AMP)

    return line_time, line_freq_raw, line_freq_interp, peak_vline, peak_text


ani = FuncAnimation(
    fig, animate, interval=30, blit=True, cache_frame_data=False
)

plt.show()
parser.ser.close()